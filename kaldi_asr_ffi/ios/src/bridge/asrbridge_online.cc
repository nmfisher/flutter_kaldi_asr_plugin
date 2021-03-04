#include <libgen.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/mman.h>
#include <unistd.h>
#include "mmap/mmap_stream.hpp"
#include "server/server.hpp"
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
#include "base/kaldi-types.h"
#include "nnet3/decodable-simple-looped.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <fst/fstlib.h>
#include <pthread.h>
#include <unistd.h> 

using namespace kaldi;
using namespace fst;

namespace kaldi {
 FILE* logptr; 

 std::string LatticeToString(const Lattice &lat, const fst::SymbolTable &word_syms) {
   LatticeWeight weight;
   std::vector<int32> alignment;
   std::vector<int32> words;
   GetLinearSymbolSequence(lat, &alignment, &words, &weight);

   std::ostringstream msg;
   for (size_t i = 0; i < words.size(); i++) {
     std::string s = word_syms.Find(words[i]);
     if (s.empty()) {
       KALDI_WARN << "Word-id " << words[i] << " not in symbol table.";
       msg << "<#" << std::to_string(i) << "> ";
     } else
       msg << s << " ";
   }
   return msg.str();
 }

 std::string GetTimeString(int32 t_beg, int32 t_end, BaseFloat time_unit) {
   char buffer[100];
   double t_beg2 = t_beg * time_unit;
   double t_end2 = t_end * time_unit;
   snprintf(buffer, 100, "%.2f %.2f", t_beg2, t_end2);
   return std::string(buffer);
 }

 int32 GetLatticeTimeSpan(const Lattice& lat) {
   std::vector<int32> times;
   LatticeStateTimes(lat, &times);
   return times.back();
 }

 std::string LatticeToString(const CompactLattice &clat, const fst::SymbolTable &word_syms) {
   if (clat.NumStates() == 0) {
     KALDI_WARN << "Empty lattice.";
     return "";
   }
   CompactLattice best_path_clat;
   CompactLatticeShortestPath(clat, &best_path_clat);

   Lattice best_path_lat;
   ConvertLattice(best_path_clat, &best_path_lat);
   return LatticeToString(best_path_lat, word_syms);
 }
}
   BaseFloat chunk_length_secs = 0.18;
   BaseFloat output_period = 1;
   BaseFloat samp_freq = 16000.0;
   bool produce_time = false;
   int timeout = -1;

   TransitionModel *trans_model;
   nnet3::AmNnetSimple *am_nnet;
   LatticeFasterDecoderConfig *decoder_opts;
   OnlineNnet2FeaturePipelineInfo *feature_info;
   OnlineNnet2FeaturePipeline *feature_pipeline;
   nnet3::DecodableNnetSimpleLoopedInfo *decodable_info;
   fst::Fst<fst::StdArc> *decode_fst;
   nnet3::NnetSimpleLoopedComputationOptions *decodable_opts;
   OnlineEndpointConfig *endpoint_opts;
   TcpServer *server;
   fst::SymbolTable *word_syms;
   OnlineCmvnState *cmvn_state;
   int port_num_actual;

static void *thr_func(void *args) {
 
    BaseFloat frame_shift = feature_info->FrameShiftInSeconds();
    int32 frame_subsampling = decodable_opts->frame_subsampling_factor;

    signal(SIGPIPE, SIG_IGN);

    while (true) {
  
        server->Accept();
   
        int32 samp_count = 0;// this is used for output refresh rate
        size_t chunk_len = static_cast<size_t>(chunk_length_secs * samp_freq);
        int32 check_period = static_cast<int32>(samp_freq * output_period);
        int32 check_count = check_period;

        int32 frame_offset = 0;
    
        bool eos = false;
        if(decode_fst == NULL)
          continue; 
        
        // feature_pipeline->SetCmvnState(*cmvn_state);

        SingleUtteranceNnet3Decoder decoder(*decoder_opts, *trans_model,
                                             *decodable_info,
                                             *decode_fst, feature_pipeline);
                                             
        while (!eos) {
          decoder.InitDecoding(frame_offset);

          OnlineSilenceWeighting silence_weighting(
             *trans_model,
             feature_info->silence_weighting_config,
             decodable_opts->frame_subsampling_factor);
          std::vector<std::pair<int32, BaseFloat>> delta_weights; 
          
          
          while (true) {
           eos = !server->ReadChunk(chunk_len);
           if (eos) {
             feature_pipeline->InputFinished();
             decoder.AdvanceDecoding();
             decoder.FinalizeDecoding();
             
             frame_offset += decoder.NumFramesDecoded();
             if (decoder.NumFramesDecoded() > 0) {
               CompactLattice lat;
               decoder.GetLattice(true, &lat);
               std::string msg = LatticeToString(lat, *word_syms);
               // get time-span from previous endpoint to end of audio,
               if (produce_time) {
                 int32 t_beg = frame_offset - decoder.NumFramesDecoded();
                 int32 t_end = frame_offset;
                 msg = GetTimeString(t_beg, t_end, frame_shift * frame_subsampling) + " " + msg;
               }
  
               KALDI_VLOG(1) << "EndOfAudio, sending message: " << msg;
               server->WriteLn(msg);
             } else {
               server->Write("\n");
             }
             server->Disconnect();
             break;
           }
  
           Vector<BaseFloat> wave_part = server->GetChunk();
           feature_pipeline->AcceptWaveform(samp_freq, wave_part);
           samp_count += chunk_len;
  
           if (silence_weighting.Active() &&
               feature_pipeline->IvectorFeature() != NULL) {
             silence_weighting.ComputeCurrentTraceback(decoder.Decoder());
             silence_weighting.GetDeltaWeights(feature_pipeline->NumFramesReady(),
                                               frame_offset * decodable_opts->frame_subsampling_factor,
                                               &delta_weights);
             feature_pipeline->UpdateFrameWeights(delta_weights);
           }
  
           decoder.AdvanceDecoding();
  
           if (samp_count > check_count) {
             if (decoder.NumFramesDecoded() > 0) {
               Lattice lat;
               decoder.GetBestPath(false, &lat);
               TopSort(&lat); // for LatticeStateTimes(),
               std::string msg = LatticeToString(lat, *word_syms);
               KALDI_VLOG(1) << "Temporary transcript: " << msg;
               server->WriteLn(msg, "\r");
             }
             check_count += check_period;
           }
  
           if (decoder.EndpointDetected(*endpoint_opts)) {
             decoder.FinalizeDecoding();
             frame_offset += decoder.NumFramesDecoded();
             CompactLattice lat;
             decoder.GetLattice(true, &lat);
             std::string msg = LatticeToString(lat, *word_syms); 
             KALDI_VLOG(1) << "Endpoint, sending message: " << msg;
             server->WriteLn(msg);
             break; // while (true)
           }
         }
       } 
   }    
  return NULL;
} // initializeFFI

Fst<StdArc> *openFST(istream* is) {
   fst::FstHeader hdr;
   // Read FstHeader which contains the type of FST
   //
   if (!hdr.Read(*is, "User-provided stream")) {
       KALDI_ERR << "Reading FST: error reading FST header";
       return NULL;
   }  
   // Check the type of Arc
   if (hdr.ArcType() != fst::StdArc::Type()) {
       KALDI_ERR << "FST with arc type " << hdr.ArcType() << " is not supported.";
       return NULL;
   }
   // Read the FST
   FstReadOptions ropts("<unspecified>", &hdr);
   Fst<StdArc> *fst = NULL;
   if (hdr.FstType() == "const") {
     fst = ConstFst<StdArc>::Read(*is, ropts);
   } else if (hdr.FstType() == "vector") {
     fst = VectorFst<StdArc>::Read(*is, ropts);
   }
   if (!fst) {
      KALDI_ERR << "Could not read fst";
      return NULL;
   }
   return fst;
}

int fstnum = 0;
int loadFST(istream* fst) {
  delete(decode_fst);    
  fprintf(logptr, "Opening FST %d\n", fstnum);
  fstnum++;
  decode_fst = openFST(fst);
  return 0;
}

int initialize(
  istream* mdl, 
  istream* symbols,
  int sampleFrequency,
  std::string logfile
) {
  
  if(!logptr) {
    logptr = fopen(logfile.c_str(), "a");
  }
  int fd = fileno(logptr);
  dup2(fd, 1);
  dup2(fd, 2);
  
  if(port_num_actual != 0) {
    fprintf(logptr, "Decoder already initialized, returning");  
    fflush(logptr);
    return 0;
  }

  // mdl->seekg(2);
  char _[2];
  mdl->read(_, 2);
  fprintf(logptr, "Initializing FFI\n");
  fflush(logptr);

  samp_freq = double(sampleFrequency);
  
    try {
      
      typedef kaldi::int32 int32;
      typedef kaldi::int64 int64;

      decodable_opts = new nnet3::NnetSimpleLoopedComputationOptions;
      decodable_opts->acoustic_scale = 1.0;
      decodable_opts->frame_subsampling_factor = 3.0;
      decodable_opts->frames_per_chunk = 50;
      decoder_opts = new LatticeFasterDecoderConfig;
      endpoint_opts = new OnlineEndpointConfig;
      endpoint_opts->silence_phones = "1"; 

      g_num_threads = 1;
      char* dir = dirname(logfile.c_str());

      string cmvn_stats = string(dir) + "/cmvnstats";
      ofstream out(cmvn_stats);
      out << " [  2.216189e+10 2.600521e+10 2.916423e+10 3.178614e+10 3.200586e+10 3.195871e+10 3.259387e+10 3.319084e+10 3.290808e+10 3.275058e+10 3.242506e+10 3.245142e+10 3.249573e+10 3.253589e+10 3.268942e+10 3.275069e+10 3.297577e+10 3.325684e+10 3.355946e+10 3.367139e+10 3.371792e+10 3.37716e+10 3.397606e+10 3.431521e+10 3.471255e+10 3.493304e+10 3.50486e+10 3.522139e+10 3.540982e+10 3.557348e+10 3.571568e+10 3.570442e+10 3.557826e+10 3.542649e+10 3.535669e+10 3.527832e+10 3.525018e+10 3.524271e+10 3.49012e+10 3.373126e+10 2.214375e+09\n2.469868e+11 3.336857e+11 4.160106e+11 4.922849e+11 4.981688e+11 4.95332e+11 5.158198e+11 5.346668e+11 5.244608e+11 5.174505e+11 5.058809e+11 5.04861e+11 5.044923e+11 5.04394e+11 5.079913e+11 5.08971e+11 5.149713e+11 5.23036e+11 5.319296e+11 5.346789e+11 5.352361e+11 5.361379e+11 5.419342e+11 5.524423e+11 5.651946e+11 5.722539e+11 5.756799e+11 5.810493e+11 5.870023e+11 5.921571e+11 5.965064e+11 5.957083e+11 5.909437e+11 5.853348e+11 5.825735e+11 5.797483e+11 5.788734e+11 5.790902e+11 5.687691e+11 5.33679e+11 0 ]";
      out.close();

      string fbank_conf = string(dir) + "/fbank.conf";
      out = ofstream(fbank_conf);
      out << "--num-mel-bins=40\n--allow-downsample=true";
      out.close();

      string cmvn_opts = string(dir) + "/cmvn_opts";
      out = ofstream(cmvn_opts);
      out << "--norm-means=true\n--norm-vars=false";
      out.close();

      // stringstream global_cmvn(cmvn_s);

      // Matrix<double> *global_cmvn_stats = new Matrix<double>();
      // global_cmvn_stats->Read(global_cmvn, false);
      OnlineNnet2FeaturePipelineConfig cfg;
      cfg.feature_type="fbank";
      cfg.fbank_config = fbank_conf;
      cfg.cmvn_config = cmvn_opts;
      cfg.global_cmvn_stats_rxfilename = cmvn_stats;
      feature_info = new OnlineNnet2FeaturePipelineInfo(cfg);
      // feature_info->use_ivectors = false;
      // // feature_info->ivector_extractor_info.global_cmvn_stats = *global_cmvn_stats;
      // // feature_info->ivector_extractor_info.cmvn_opts.normalize_mean = true;
      // // feature_info->ivector_extractor_info.cmvn_opts.normalize_variance = false;
      // feature_info->use_cmvn = true;
      // feature_info->feature_type = "fbank";
      // feature_info->cmvn_opts.normalize_mean = true;
      // feature_info->cmvn_opts.normalize_variance = false;
      // feature_info->global_cmvn_stats_rxfilename = cmvn_file;
      // feature_info->fbank_opts.frame_opts.allow_downsample= true;
      // feature_info->fbank_opts.mel_opts.num_bins = 40;

      // cmvn_state = new OnlineCmvnState(*global_cmvn_stats);
      // cmvn_state = new OnlineCmvnState();  
      feature_pipeline = new OnlineNnet2FeaturePipeline(*feature_info);

      trans_model = new TransitionModel;

      am_nnet = new nnet3::AmNnetSimple;
    {
      trans_model->Read(*mdl, true);
      fprintf(logptr, "Read transition model\n");
      fflush(logptr);

      am_nnet->Read(*mdl, true);
      fprintf(logptr, "Read acoustic model\n");
      fflush(logptr);
      SetBatchnormTestMode(true, &(am_nnet->GetNnet()));
      SetDropoutTestMode(true, &(am_nnet->GetNnet()));
      nnet3::CollapseModel(nnet3::CollapseModelConfig(), &(am_nnet->GetNnet()));
    }

    decodable_info = new nnet3::DecodableNnetSimpleLoopedInfo(*decodable_opts,
                                                      am_nnet);

    if (!(word_syms = fst::SymbolTable::ReadText(*symbols, "user-provided")))
        KALDI_ERR << "Could not read symbol table from stream";

    server = new TcpServer();
    server->timeout = timeout;
    port_num_actual = server->Listen(0);
    if(port_num_actual == -1) {
      return -1;
    }
    pthread_t thr;

    int rc;
    
    if ((rc = pthread_create(&thr, NULL, thr_func, nullptr))) {
      std::cerr << "error: pthread_create, rc: %d\n" << rc;
      return -1;
    }
     fprintf(stderr, "Created decoder, sample rate was %f and port %d \n", samp_freq, port_num_actual);
     return port_num_actual;
    } catch(const KaldiFatalError& e) {
        fprintf(logptr, "%s", e.KaldiMessage());
    } catch(const std::exception& e) {
        fprintf(logptr, "%s", e.what());
    }
    return -1;
}

int main(int argc, char** argv) {
  std::cout << "Running" << std::endl;
  int fd = open("demo.fst", O_RDONLY);
  struct stat buf;
  fstat(fd,&buf);

  logptr = fopen("out.log","a");
 
  char *mmapped_buffer_ = (char *) mmap(nullptr, buf.st_size, PROT_READ, MAP_SHARED, fd,
                                       0);
  if (mmapped_buffer_ == MAP_FAILED) {
     return NULL;
  }

  char* stream_start = mmapped_buffer_;
  char* stream_end = mmapped_buffer_ + buf.st_size;
  mmap_stream* mmap_buf = new mmap_stream(stream_start, stream_end);

  fprintf(stdout, "Mmaped size %d ", buf.st_size);

  istream* in = new istream(mmap_buf);

  loadFST(in);

}

