#ifdef __cplusplus

#include <iostream>
#include "mmap/mmap_stream.hpp"

using namespace std;

int loadFST(istream* fst);

struct DecoderConfiguration {
  int input_port;
  int output_port;
};

DecoderConfiguration initialize(
  istream* mdl,
  istream* symbols,
  int samp_freq,
  std::string logfile
);



#endif
