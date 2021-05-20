#ifdef __cplusplus
#include "nnet3/nnet-utils.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

namespace kaldi {
  class TcpServer {
   public:
    explicit TcpServer();
    ~TcpServer();
  
    int32 Listen(int32 port);  // start listening on a given port
    int32 Accept();  // accept a client and return its descriptor
  
    bool ReadChunk(size_t len); // get more data and return false if end-of-stream
  
    Vector<BaseFloat> GetChunk(); // get the data read by above method
  
    bool Write(const std::string &msg); // write to accepted client
    bool WriteLn(const std::string &msg, const std::string &eol = "\n"); // write line to accepted client
  
    void Disconnect();
    void Kill();

    int timeout; 

   private:
    struct ::sockaddr_in h_addr_;
    int32 server_desc_, client_desc_;
    int16 *samp_buf_;
    size_t buf_len_, has_read_;
    pollfd client_set_[1];
    bool kill = false;

  };
}  
  

#endif
