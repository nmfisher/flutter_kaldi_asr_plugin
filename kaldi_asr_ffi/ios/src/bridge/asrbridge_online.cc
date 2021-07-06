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
#include "asrbridge_online.hpp"
#include <chrono>
#include <thread>

using namespace kaldi;
using namespace fst;

namespace kaldi
{

  static FILE *logptr;
  static istream *mdl_s;
  static istream *symbols_s;

  std::string LatticeToString(const Lattice &lat, const fst::SymbolTable &word_syms)
  {
    LatticeWeight weight;
    std::vector<int32> alignment;
    std::vector<int32> words;
    GetLinearSymbolSequence(lat, &alignment, &words, &weight);

    std::ostringstream msg;
    for (size_t i = 0; i < words.size(); i++)
    {
      std::string s = word_syms.Find(words[i]);
      if (s.empty())
      {
        KALDI_WARN << "Word-id " << words[i] << " not in symbol table.";
        msg << "<#" << std::to_string(i) << "> ";
      }
      else
        msg << s << " ";
    }
    return msg.str();
  }

  std::string GetTimeString(int32 t_beg, int32 t_end, BaseFloat time_unit)
  {
    char buffer[100];
    double t_beg2 = t_beg * time_unit;
    double t_end2 = t_end * time_unit;
    snprintf(buffer, 100, "%.2f %.2f", t_beg2, t_end2);
    return std::string(buffer);
  }

  int32 GetLatticeTimeSpan(const Lattice &lat)
  {
    std::vector<int32> times;
    LatticeStateTimes(lat, &times);
    return times.back();
  }

  std::string LatticeToString(const CompactLattice &clat, const fst::SymbolTable &word_syms)
  {
    if (clat.NumStates() == 0)
    {
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
BaseFloat chunk_length_secs = 0.5;
BaseFloat output_period = 0.5;
BaseFloat samp_freq = 16000.0;
bool produce_time = false;
int timeout = 5000;

bool terminateDecoder = false;
TransitionModel *trans_model;
nnet3::AmNnetSimple *am_nnet;
LatticeFasterDecoderConfig *decoder_opts;
OnlineNnet2FeaturePipelineConfig *feature_opts;
OnlineNnet2FeaturePipelineInfo *feature_info;
nnet3::DecodableNnetSimpleLoopedInfo *decodable_info;
fst::Fst<fst::StdArc> *decode_fst;
nnet3::NnetSimpleLoopedComputationOptions *decodable_opts;
OnlineEndpointConfig *endpoint_opts;
TcpServer *inputServer;
TcpServer *outputServer;
fst::SymbolTable *word_syms;

int input_port_num_actual = -1;
int output_port_num_actual = -1;
static int iter_count = 0;

static void *thr_func(void *args)
{

  BaseFloat frame_shift = feature_info->FrameShiftInSeconds();
  int32 frame_subsampling = decodable_opts->frame_subsampling_factor;

  signal(SIGPIPE, SIG_IGN);

  while (!terminateDecoder)
  {

    KALDI_LOG << "Accepting outputserver";
    if(outputServer->Accept() == -1) {
      // KALDI_LOG << "No output server";
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    };
    KALDI_LOG << "Accepting inputserver";
    if(inputServer->Accept() == -1) {
      // KALDI_LOG << "No inputserver";
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    // KALDI_LOG << "Checking decode fst";
    if (!decode_fst)
    {
      // KALDI_LOG << "No decode_fst";
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }

    int32 samp_count = 0; // this is used for output refresh rate
    size_t chunk_len = static_cast<size_t>(chunk_length_secs * samp_freq);
    int32 check_period = static_cast<int32>(samp_freq * output_period);
    int32 check_count = check_period;

    int32 frame_offset = 0;

    bool eos = false;

    OnlineNnet2FeaturePipeline feature_pipeline(*feature_info);

    SingleUtteranceNnet3Decoder ddecoder(*decoder_opts, *trans_model,
                                         *decodable_info,
                                         *decode_fst, &feature_pipeline);

    // std::string fstring("/data/user/0/academy.fluent.android/app_flutter/asbridgeaudio" + std::to_string(iter_count));
    // FILE *audiofile = fopen(fstring.c_str(), "w");

    while (!eos)
    {
      ddecoder.InitDecoding(frame_offset);

      /*          OnlineSilenceWeighting silence_weighting(
             *trans_model,
             feature_info->silence_weighting_config,
             decodable_opts->frame_subsampling_factor);
          std::vector<std::pair<int32, BaseFloat>> delta_weights; */

      while (!terminateDecoder)
      {
        eos = !inputServer->ReadChunk(chunk_len);
        if (terminateDecoder)
        {
          break;
        }
        if (eos)
        {
          feature_pipeline.InputFinished();
          ddecoder.AdvanceDecoding();
          ddecoder.FinalizeDecoding();

          frame_offset += ddecoder.NumFramesDecoded();
          if (ddecoder.NumFramesDecoded() > 0)
          {
            CompactLattice lat;
            ddecoder.GetLattice(true, &lat);
            std::string msg = LatticeToString(lat, *word_syms);
            // get time-span from previous endpoint to end of audio,
            if (produce_time)
            {
              int32 t_beg = frame_offset - ddecoder.NumFramesDecoded();
              int32 t_end = frame_offset;
              msg = GetTimeString(t_beg, t_end, frame_shift * frame_subsampling) + " " + msg;
            }

            KALDI_LOG << "EndOfAudio, sending message: " << msg;
            if (!outputServer->WriteLn(msg))
            {
              KALDI_LOG << "Failed to write output";
            }
          }
          else
          {
            KALDI_LOG << "EOS received with no frames decoded";
            // outputServer->Write("\n");
          }
          KALDI_LOG << "Finished decoding iteration, disconnecting";
          inputServer->Disconnect();
          break;
        }

        Vector<BaseFloat> wave_part = inputServer->GetChunk();
        float* data = wave_part.Data();
        int16 copy[wave_part.Dim()];
        for(int i =0; i < wave_part.Dim(); i++)
          copy[i] = static_cast<int16>(data[i]);

        // fwrite(&copy, sizeof(int16), wave_part.Dim(), audiofile);

        feature_pipeline.AcceptWaveform(samp_freq, wave_part);

        samp_count += chunk_len;

        /*if (silence_weighting.Active() &&
               feature_pipeline.IvectorFeature() != NULL) {
             silence_weighting.ComputeCurrentTraceback(ddecoder.Decoder());
             silence_weighting.GetDeltaWeights(feature_pipeline.NumFramesReady(),
                                               frame_offset * decodable_opts->frame_subsampling_factor,
                                               &delta_weights);
             feature_pipeline.UpdateFrameWeights(delta_weights);
           }*/
        ddecoder.AdvanceDecoding();

        if (samp_count > check_count)
        {

          if (ddecoder.NumFramesDecoded() > 0)
          {
            Lattice lat;
            ddecoder.GetBestPath(false, &lat);
            TopSort(&lat); // for LatticeStateTimes(),
            std::string msg = LatticeToString(lat, *word_syms);
            KALDI_VLOG(1) << "Temporary transcript: " << msg;
            outputServer->WriteLn(msg, "\r");
          }
          check_count += check_period;
        }

        if (ddecoder.EndpointDetected(*endpoint_opts))
        {
          KALDI_LOG << "endpoint detected ";
          ddecoder.FinalizeDecoding();
          frame_offset += ddecoder.NumFramesDecoded();
          CompactLattice lat;
          ddecoder.GetLattice(true, &lat);
          std::string msg = LatticeToString(lat, *word_syms);
          KALDI_LOG << "Endpoint, sending message: " << msg;
          outputServer->WriteLn(msg);
          break;
        }
      }
      if (terminateDecoder)
      {
        break;
      }
    }
    iter_count++;
    // fclose(audiofile);
    KALDI_LOG << "Reached end";
  }
  delete(inputServer);
  inputServer = nullptr;
  // KALDI_LOG << "Deleted inputserver";
  return nullptr;
} // initializeFFI

Fst<StdArc> *openFST(istream *is)
{
  fst::FstHeader hdr;
  // Read FstHeader which contains the type of FST
  //
  if (!hdr.Read(*is, "User-provided stream"))
  {
    KALDI_ERR << "Reading FST: error reading FST header";
    return nullptr;
  }
  // Check the type of Arc
  if (hdr.ArcType() != fst::StdArc::Type())
  {
    KALDI_ERR << "FST with arc type " << hdr.ArcType() << " is not supported.";
    return nullptr;
  }
  // Read the FST
  FstReadOptions ropts("<unspecified>", &hdr);
  Fst<StdArc> *fst = nullptr;
  if (hdr.FstType() == "const")
  {
    fst = ConstFst<StdArc>::Read(*is, ropts);
  }
  else if (hdr.FstType() == "vector")
  {
    fst = VectorFst<StdArc>::Read(*is, ropts);
  }
  if (!fst)
  {
    KALDI_ERR << "Could not read fst";
  }
  return fst;
}

int start_decoder_thread()
{
  pthread_t thr;

  int rc;

  if ((rc = pthread_create(&thr, NULL, thr_func, nullptr)))
  {
    std::cerr << "error: pthread_create, rc: %d\n"
              << rc;
    return -1;
  }
  return 0;
}

int fstnum = 0;
int loadFST(istream *fst)
{
  KALDI_LOG << "Loading FST";
  terminateDecoder = true;
  inputServer->Kill();
  while (inputServer)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    KALDI_LOG << "Waiting";
  }
  KALDI_LOG << "Decoder thread killed.";

  if (decode_fst)
  {
    delete (decode_fst);
  }
  KALDI_LOG << "Opening FST " << fstnum;
  fstnum++;
  decode_fst = openFST(fst);

  inputServer = new TcpServer();
  inputServer->timeout = timeout;

  input_port_num_actual = inputServer->Listen(input_port_num_actual);
  terminateDecoder = false;

  return start_decoder_thread();
}

DecoderConfiguration initialize(
    istream *mdl,
    istream *symbols,
    int sampleFrequency,
    std::string logfile)
{

  if (!logptr)
  {
    logptr = fopen(logfile.c_str(), "a");
  }
  int fd = fileno(logptr);
  dup2(fd, 1);
  dup2(fd, 2);

  struct DecoderConfiguration configuration;
  configuration.input_port = input_port_num_actual;
  configuration.output_port = output_port_num_actual;

  if (input_port_num_actual != -1 && output_port_num_actual != -1)
  {
    KALDI_LOG << "Decoder already initialized, returning";
    return configuration;
  }

  KALDI_LOG << "Creating decoder with sample rate " << samp_freq;

  char _[2];
  mdl->read(_, 2);

  mdl_s = mdl;
  symbols_s = symbols;

  samp_freq = double(sampleFrequency);

  try
  {

    typedef kaldi::int32 int32;
    typedef kaldi::int64 int64;

    feature_opts = new OnlineNnet2FeaturePipelineConfig;
    decodable_opts = new nnet3::NnetSimpleLoopedComputationOptions;
    decoder_opts = new LatticeFasterDecoderConfig;
    endpoint_opts = new OnlineEndpointConfig;

    ParseOptions po("usage");

    feature_opts->Register(&po);
    decodable_opts->Register(&po);
    decoder_opts->Register(&po);
    endpoint_opts->Register(&po);

    g_num_threads = 1;
    string directory;
    const size_t last_slash_idx = logfile.rfind('/');
    if (std::string::npos != last_slash_idx)
    {
      directory = logfile.substr(0, last_slash_idx);
    }
    const char *dir = directory.c_str();

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

    string cmvn = "--cmvn-config=" + cmvn_opts;
    string fbank = "--fbank-config=" + fbank_conf;
    string stats = "--global-cmvn-stats=" + cmvn_stats;

    const char *cmvn_c = cmvn.c_str();
    const char *fbank_c = fbank.c_str();
    const char *stats_c = stats.c_str();

    const char *opts[10] = {
        "", stats_c, cmvn_c, "--feature-type=fbank", fbank_c, "--acoustic-scale=1.0", "--frame-subsampling-factor=3", "--frames-per-chunk=50", "--minimize=false", "--endpoint.silence-phones=1"};

    po.Read(10, opts);

    feature_info = new OnlineNnet2FeaturePipelineInfo(*feature_opts);

    trans_model = new TransitionModel;

    am_nnet = new nnet3::AmNnetSimple;
    trans_model->Read(*mdl, true);
    am_nnet->Read(*mdl, true);
    SetBatchnormTestMode(true, &(am_nnet->GetNnet()));
    SetDropoutTestMode(true, &(am_nnet->GetNnet()));
    nnet3::CollapseModel(nnet3::CollapseModelConfig(), &(am_nnet->GetNnet()));

    decodable_info = new nnet3::DecodableNnetSimpleLoopedInfo(*decodable_opts,
                                                              am_nnet);

    if (!(word_syms = fst::SymbolTable::ReadText(*symbols, "user-provided")))
      KALDI_ERR << "Could not read symbol table from stream";

    inputServer = new TcpServer();
    inputServer->timeout = timeout;

    input_port_num_actual = inputServer->Listen(0);
    if (input_port_num_actual <= 0)
    {
      KALDI_ERR << "Error configuring input port " << input_port_num_actual;
    }
    outputServer = new TcpServer();
    outputServer->timeout = timeout;
    output_port_num_actual = outputServer->Listen(0);
    if (output_port_num_actual <= 0)
    {
      KALDI_ERR << "Error configuring output port " << output_port_num_actual;
    }

    if(start_decoder_thread() == 0) {
      configuration.input_port = input_port_num_actual;
      configuration.output_port = output_port_num_actual;
      KALDI_LOG << "Created decoder, sample rate was " << samp_freq << ", input port " << input_port_num_actual << ", output port " << output_port_num_actual;
    }
  }
  catch (const KaldiFatalError &e)
  {
    KALDI_ERR << e.KaldiMessage();
  }
  catch (const std::exception &e)
  {
    KALDI_ERR << e.what();
  }
  return configuration;
}

int main(int argc, char **argv)
{
  std::cout << "Running" << std::endl;
  int fd = open("demo.fst", O_RDONLY);
  struct stat buf;
  fstat(fd, &buf);

  logptr = fopen("out.log", "a");

  char *mmapped_buffer_ = (char *)mmap(nullptr, buf.st_size, PROT_READ, MAP_SHARED, fd,
                                       0);
  if (mmapped_buffer_ == MAP_FAILED)
  {
    return NULL;
  }

  char *stream_start = mmapped_buffer_;
  char *stream_end = mmapped_buffer_ + buf.st_size;
  mmap_stream *mmap_buf = new mmap_stream(stream_start, stream_end);

  fprintf(stdout, "Mmaped size %d ", buf.st_size);

  istream *in = new istream(mmap_buf);

  loadFST(in);
}
