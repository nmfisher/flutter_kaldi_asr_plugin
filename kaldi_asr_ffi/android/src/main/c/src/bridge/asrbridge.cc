// online2bin/online2-wav-nnet3-latgen-faster.cc

// Copyright 2014  Johns Hopkins University (author: Daniel Povey)
//           2016  Api.ai (Author: Ilya Platonov)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include "feat/wave-reader.h"
#include "online2/online-nnet3-decoding.h"
#include "online2/online-nnet2-feature-pipeline.h"
#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"
#include "decoder/lattice-faster-decoder.h"
#include "util/kaldi-thread.h"
#include "nnet3/nnet-utils.h"
#include "base/kaldi-types.h"
#include "nnet3/decodable-simple-looped.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <fst/fstlib.h>

FILE* logptr;

namespace kaldi {

  class GlobalDecoderWrapper
  {
    public:
      SingleUtteranceNnet3Decoder *decoder;
      OnlineSilenceWeighting *silence_weighting;
      OnlineNnet2FeaturePipeline *feature_pipeline;
      OnlineEndpointConfig *endpoint_opts;
      TransitionModel* trans_model;
      fst::SymbolTable *word_syms;
      nnet3::AmNnetSimple* am_nnet;
      nnet3::DecodableNnetSimpleLoopedInfo *decodable_info;
      OnlineIvectorExtractorAdaptationState *adaptation_state;
      OnlineCmvnState *cmvn_state;
      OnlineNnet2FeaturePipelineInfo *feature_info;
      LatticeFasterDecoderConfig *decoder_opts;
      fst::Fst<fst::StdArc> *decode_fst;
      
      bool do_endpointing = false;
      kaldi::BaseFloat chunk_length_secs = 0.18;
      bool decoding = false;
      GlobalDecoderWrapper() { }
      ~GlobalDecoderWrapper() {
        cleanup();
      }

      void cleanup() {
        delete decoder;
        decoder = NULL;
        delete silence_weighting;
        silence_weighting = NULL;
        delete feature_pipeline;
        feature_pipeline = NULL;
        delete endpoint_opts;
        endpoint_opts = NULL;
        delete trans_model;
        trans_model = NULL;
        delete word_syms;
        word_syms = NULL;
        delete am_nnet;
        am_nnet = NULL;
        delete decodable_info;
        decodable_info = NULL;
        delete adaptation_state;
        adaptation_state = NULL;
        delete cmvn_state;
        cmvn_state = NULL;
        delete decode_fst;
        decode_fst = NULL;
        delete decoder_opts;
        decoder_opts = NULL;
        delete feature_info;
        feature_info = NULL;
      }

      void initialize(
        OnlineNnet2FeaturePipelineConfig feature_opts,
        nnet3::NnetSimpleLoopedComputationOptions *decodable_opts,
        LatticeFasterDecoderConfig *decoder_opts,
        OnlineEndpointConfig *endpoint_opts,
        std::string nnet3_rxfilename,
        std::string word_syms_rxfilename, 
        std::string fst_rxfilename,
        bool online, bool do_endpointing) {

        using namespace kaldi;
        using namespace fst;

        cleanup();

        this->endpoint_opts = endpoint_opts;
        this->decoder_opts = decoder_opts;

        this->feature_info = new OnlineNnet2FeaturePipelineInfo(feature_opts);
        if (!online) {
          feature_info->ivector_extractor_info.use_most_recent_ivector = true;
          feature_info->ivector_extractor_info.greedy_ivector_extractor = true;
          chunk_length_secs = -1.0;
        }

        Matrix<double> global_cmvn_stats;
        if (feature_info->global_cmvn_stats_rxfilename != "")
          ReadKaldiObject(feature_info->global_cmvn_stats_rxfilename,
                          &global_cmvn_stats);

        this->trans_model = new TransitionModel();
        this->am_nnet = new nnet3::AmNnetSimple();
        {
          bool binary;
          Input ki(nnet3_rxfilename, &binary);
          trans_model->Read(ki.Stream(), binary);
          am_nnet->Read(ki.Stream(), binary);
          SetBatchnormTestMode(true, &(am_nnet->GetNnet()));
          SetDropoutTestMode(true, &(am_nnet->GetNnet()));
          nnet3::CollapseModel(nnet3::CollapseModelConfig(), &(am_nnet->GetNnet()));
        }

        this->decodable_info = new nnet3::DecodableNnetSimpleLoopedInfo(*decodable_opts,
                                                          &(*am_nnet));


        this->decode_fst = ReadFstKaldiGeneric(fst_rxfilename);

        if (word_syms_rxfilename != "")
          if (!(this->word_syms = fst::SymbolTable::ReadText(word_syms_rxfilename)))
            KALDI_ERR << "Could not read symbol table from file "
                      << word_syms_rxfilename;

        this->adaptation_state = new OnlineIvectorExtractorAdaptationState(
            this->feature_info->ivector_extractor_info);
        this->cmvn_state = new OnlineCmvnState(global_cmvn_stats);

        this->silence_weighting = new OnlineSilenceWeighting(
                *trans_model,
                this->feature_info->silence_weighting_config,
                decodable_opts->frame_subsampling_factor);
        
        this->do_endpointing = do_endpointing;

      }

      int createDecoder() {
        delete(this->decoder);
        delete(this->feature_pipeline);
        
        if(!this->feature_info) {
          fprintf(logptr, "Decoder cannot be created as initialize() has not yet been called.\n");
          return -1;
        }
        this->feature_pipeline = new OnlineNnet2FeaturePipeline(*this->feature_info);
        this->feature_pipeline->SetAdaptationState(*adaptation_state);
        this->feature_pipeline->SetCmvnState(*cmvn_state);
        this->decoder = new SingleUtteranceNnet3Decoder(*this->decoder_opts, *this->trans_model,
                                              *this->decodable_info,
                                              *this->decode_fst, this->feature_pipeline);
        fprintf(logptr, "Decoder created.\n");
        return 0;
      }
  };

  void GetDiagnosticsAndPrintOutput(const fst::SymbolTable *word_syms,
                                      const CompactLattice &clat,
                                      const std::string &filepath) {
      if (clat.NumStates() == 0) {
          KALDI_WARN << "Empty lattice.";
          return;
      }
      CompactLattice best_path_clat;
      CompactLatticeShortestPath(clat, &best_path_clat);

      Lattice best_path_lat;
      ConvertLattice(best_path_clat, &best_path_lat);

      double likelihood;
      LatticeWeight weight;
      std::vector<int32> alignment;
      std::vector<int32> words;
      GetLinearSymbolSequence(best_path_lat, &alignment, &words, &weight);

      std::remove(filepath.c_str()); // delete file if it already exists
      std::ofstream outfile;
      outfile.open(filepath);
      if (word_syms != NULL) {
          for (size_t i = 0; i < words.size(); i++) {
              std::string s = word_syms->Find(words[i]);
              if (s == "")
                  KALDI_ERR << "Word-id " << words[i] << " not in symbol table.";
              outfile << s << '\t' << words[i];
              outfile << '\n';
          }
          outfile << std::endl;
      }
      outfile.close();
  }

  kaldi::GlobalDecoderWrapper decoderWrapper;

  extern "C" int decode(char *filepath, char *output_filename) {
    while(decoderWrapper.decoding) {

    }
    try {
        std::ofstream outf(output_filename);
        outf << "Reading from rspecifier " << filepath << "\n";
        outf.close();

        if(decoderWrapper.createDecoder() != 0) {
          return -1;
        }
        fprintf(logptr, "Created decoder.\n");
        decoderWrapper.decoding = true;
        std::string wav_rspecifier(filepath);
        RandomAccessTableReader<WaveHolder> wav_reader(wav_rspecifier);
        const WaveData &wave_data = wav_reader.Value("000");
        // get the data for channel zero (if the signal is not mono, we only
        // take the first channel).
        SubVector<BaseFloat> data(wave_data.Data(), 0);
        BaseFloat samp_freq = wave_data.SampFreq();
        int32 chunk_length;
        if (decoderWrapper.chunk_length_secs > 0) {
            chunk_length = int32(samp_freq * decoderWrapper.chunk_length_secs);
            if (chunk_length == 0) chunk_length = 1;
        } else {
            chunk_length = std::numeric_limits<int32>::max();
        }

        int32 samp_offset = 0;
        std::vector<std::pair<int32, BaseFloat> > delta_weights;
        while (samp_offset < data.Dim()) {
            int32 samp_remaining = data.Dim() - samp_offset;
            int32 num_samp = chunk_length < samp_remaining ? chunk_length
                                                            : samp_remaining;

            SubVector<BaseFloat> wave_part(data, samp_offset, num_samp);
            decoderWrapper.feature_pipeline->AcceptWaveform(samp_freq, wave_part);
        
            samp_offset += num_samp;
            if (samp_offset == data.Dim()) {
              decoderWrapper.feature_pipeline->InputFinished();
            }
            if (decoderWrapper.silence_weighting->Active() &&
                decoderWrapper.feature_pipeline->IvectorFeature() != NULL) {
              decoderWrapper.silence_weighting->ComputeCurrentTraceback(decoderWrapper.decoder->Decoder());
              decoderWrapper.silence_weighting->GetDeltaWeights(decoderWrapper.feature_pipeline->NumFramesReady(),
                                                &delta_weights);
              decoderWrapper.feature_pipeline->IvectorFeature()->UpdateFrameWeights(delta_weights);
            }
            decoderWrapper.decoder->AdvanceDecoding();

            if (decoderWrapper.do_endpointing && decoderWrapper.decoder->EndpointDetected(*decoderWrapper.endpoint_opts))
              break;
        }
        decoderWrapper.decoder->FinalizeDecoding();
        CompactLattice clat;
        bool end_of_utterance = true;
        decoderWrapper.decoder->GetLattice(end_of_utterance, &clat);
        std::string out(output_filename);

        GetDiagnosticsAndPrintOutput(decoderWrapper.word_syms, clat, out);
        decoderWrapper.feature_pipeline->GetAdaptationState(decoderWrapper.adaptation_state);
        decoderWrapper.feature_pipeline->GetCmvnState(decoderWrapper.cmvn_state);
        decoderWrapper.decoding = false;
        return 0;
    } catch(const KaldiFatalError& e) {
        fprintf(logptr, e.KaldiMessage());
        fflush(logptr);
        return -1;
    }
  }



  extern "C"  int initializeFFI(int argc, char *argv[]) {
    if(logptr) {
      fclose(logptr);
      logptr = NULL;
    }
    logptr = fopen(argv[0], "w");

    try {
      using namespace kaldi;
      using namespace fst;

      typedef kaldi::int32 int32;
      typedef kaldi::int64 int64;

      const char *usage =
          "Reads in wav file(s) and simulates online decoding with neural nets\n"
          "(nnet3 setup), with optional iVector-based speaker adaptation and\n"
          "optional endpointing.  Note: some configuration values and inputs are\n"
          "set via config files whose filenames are passed as options\n"
          "\n"
          "Usage: online2-wav-nnet3-latgen-faster [options] <nnet3-in> <fst-in> "
          "<spk2utt-rspecifier> <wav-rspecifier> <lattice-wspecifier>\n"
          "The spk2utt-rspecifier can just be <utterance-id> <utterance-id> if\n"
          "you want to decode utterance by utterance.\n";

      ParseOptions po(usage);

      OnlineNnet2FeaturePipelineConfig feature_opts;
      nnet3::NnetSimpleLoopedComputationOptions* decodable_opts = new nnet3::NnetSimpleLoopedComputationOptions();
      LatticeFasterDecoderConfig* decoder_opts = new LatticeFasterDecoderConfig();
      OnlineEndpointConfig* endpoint_opts = new OnlineEndpointConfig();

      feature_opts.Register(&po);
      decodable_opts->Register(&po);
      decoder_opts->Register(&po);
      endpoint_opts->Register(&po);

      std::string word_syms_rxfilename;

      bool do_endpointing = false;
      bool online = true;
      
      BaseFloat chunk_length_secs = 0.18;
      po.Register("chunk-length", &chunk_length_secs,
                  "Length of chunk size in seconds, that we process.  Set to <= 0 "
                  "to use all input in one chunk.");
      po.Register("word-symbol-table", &word_syms_rxfilename,
                  "Symbol table for words [for debug output]");
      po.Register("do-endpointing", &do_endpointing,
                  "If true, apply endpoint detection");
      po.Register("online", &online,
                  "You can set this to false to disable online iVector estimation "
                  "and have all the data for each utterance used, even at "
                  "utterance start.  This is useful where you just want the best "
                  "results and don't care about online operation.  Setting this to "
                  "false has the same effect as setting "
                  "--use-most-recent-ivector=true and --greedy-ivector-extractor=true "
                  "in the file given to --ivector-extraction-config, and "
                  "--chunk-length=-1.");
      po.Register("num-threads-startup", &g_num_threads,
                  "Number of threads used when initializing iVector extractor.");

      po.Read(argc, argv);
    
      if (po.NumArgs() != 2) {
        fprintf(logptr, "ERROR: Insufficient arguments\n.");
        po.PrintUsage();
        return 1;
      }

      std::string nnet3_rxfilename = po.GetArg(1),
      fst_rxfilename = po.GetArg(2);
      
      decoderWrapper.initialize(
        feature_opts, 
        decodable_opts, 
        decoder_opts, 
        endpoint_opts, 
        nnet3_rxfilename, 
        word_syms_rxfilename, 
        fst_rxfilename, 
        online, 
        do_endpointing);
          
      return 0;
    } catch(const KaldiFatalError& e) {
        fprintf(logptr, e.KaldiMessage());
        return -1;
    } catch(const std::exception& e) {
        fprintf(logptr, e.what());
        return -1;
    }
  } // main()
}



