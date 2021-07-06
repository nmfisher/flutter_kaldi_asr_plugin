#include "nnet3/nnet-utils.h"
#include "server.hpp"
#include <stdio.h>
#include <sys/select.h>
#ifndef SOCK_NOBLOCK
#include <fcntl.h>
#include <sys/socket.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

//std::ostream& KALDI_ERR = std::cerr;
//std::ostream& KALDI_LOG = std::cerr;

namespace kaldi
{
  TcpServer::TcpServer()
  {
    server_desc_ = -1;
    client_desc_ = -1;
    samp_buf_ = NULL;
    buf_len_ = 0;
  }

  int32 TcpServer::Listen(int32 port)
  {

    KALDI_LOG << "TCP server attempting to listen on port " << port;


    h_addr_.sin_addr.s_addr = INADDR_ANY;
    h_addr_.sin_port = htons(port);
    h_addr_.sin_family = AF_INET;

    server_desc_ = socket(AF_INET, SOCK_STREAM, 0);

    int32 flag = 1;
    int32 len = sizeof(int32);

   /* struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0; */

    if (setsockopt(server_desc_, SOL_SOCKET, SO_REUSEADDR, &flag, len) == -1 
    // || setsockopt(server_desc_, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(timeval)) == -1
    )
    {
      KALDI_ERR << "TCP server attempting to listen on port " << port;
      return -1;
    }

    if (bind(server_desc_, (struct sockaddr *)&h_addr_, sizeof(h_addr_)) == -1)
    {
      KALDI_ERR << "Cannot bind to port: " << port << " (is it taken?):" << strerror(errno);
      return -1;
    }

    if (listen(server_desc_, 1) == -1)
    {
      KALDI_ERR << "Cannot listen on port: " << strerror(errno);
      return -1;
    }


    struct sockaddr_in sin;
    socklen_t socklen = sizeof(sin);
    if (getsockname(server_desc_, (struct sockaddr *)&sin, &socklen) == -1)
    {
      KALDI_ERR << "Error starting TcpServer";
      return -1;
    }

    KALDI_LOG << "Successfully listening with FD " << server_desc_;

    return ntohs(sin.sin_port);
  }

  TcpServer::~TcpServer()
  {
    Kill();
    delete[] samp_buf_;
  }

  int32 TcpServer::Accept()
  {
    if (client_desc_ != -1)
    {
      return client_desc_;
    }
    KALDI_LOG << "Waiting for client...";

    if(server_desc_ == -1) {
      KALDI_LOG << "Server not listening, runtime error?";
      return -1;
    }

  
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(server_desc_, &rfds);
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 100;
    int available = select(server_desc_ + 1, &rfds, NULL, NULL, &t);

    if (available == -1)
    {
      KALDI_LOG << "Could not select available connections : " << strerror(errno);
      return -1;
    }
    else if (available == 0)
    {
      KALDI_LOG << "Timed out waiting for data.";
      return -1;
    }

    socklen_t len;

    len = sizeof(struct sockaddr);
    client_desc_ = accept(server_desc_, (struct sockaddr *)&h_addr_, &len);

    if (client_desc_ == -1)
    {
      KALDI_ERR << "Could not accept client connection: " << strerror(errno);
      return -1;
    }

    KALDI_LOG << "Got client desc " << client_desc_;

    struct sockaddr_storage addr;
    char ipstr[20];

    len = sizeof addr;
    getpeername(client_desc_, (struct sockaddr *)&addr, &len);

    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

    client_set_[0].fd = client_desc_;
    client_set_[0].events = POLLIN;

    KALDI_LOG << "Accepted connection from: " << ipstr;

    return client_desc_;
  }

  bool TcpServer::ReadChunk(size_t len)
  {
    if (buf_len_ != len)
    {
      buf_len_ = len;
      delete[] samp_buf_;
      samp_buf_ = new int16[len];
    }

    ssize_t ret;
    int poll_ret;
    char *samp_buf_p = reinterpret_cast<char *>(samp_buf_);
    size_t to_read = len * sizeof(int16);
    has_read_ = 0;

    struct timeval t;
    t.tv_sec = 30;
    t.tv_usec = 0;

    if (client_desc_ == -1)
    {
      return false;
    }

    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(client_desc_, &rfds);

    while (to_read > 0)
    {
      // KALDI_LOG << "Selecting ";
      int available = select(client_desc_ + 1, &rfds, NULL, NULL, &t);

      if (available == -1)
      {
        KALDI_LOG << errno << " " << strerror(errno);
        return false;
      }
      else if (available == 0)
      {
        KALDI_LOG << "Timed out waiting for data.";
        return false;
      }

      // KALDI_LOG << "Polling";

      poll_ret = poll(client_set_, 1, timeout);
      if (poll_ret == 0)
      {
        Disconnect();
        KALDI_LOG << "Socket timeout! Disconnecting...";
        break;
      }
      if (poll_ret < 0)
      {
        Disconnect();
        KALDI_LOG << "Socket error! Disconnecting...";
        break;
      }
      ret = read(client_desc_, static_cast<void *>(samp_buf_p + has_read_), to_read);
      if (ret <= 0)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          Disconnect();
          KALDI_LOG << "#recv_response. errno = EAGAIN#";
          return false;
        }
        else if (errno == EINTR)
        {
          Disconnect();
          KALDI_LOG << "recv interruputed";
          return false;
        }
        KALDI_LOG << "Failed to read, aborting";
        break;
      }

      int tot = 0;
      for (size_t i = 0; i < ret; i++)
      {
        tot += samp_buf_[has_read_ + i];
      }
      if (ret > 0 && tot == 0)
      {
        KALDI_LOG << ret << " zero samples detected";
        return false;
      }

      to_read -= ret;
      has_read_ += ret;
    }
    has_read_ /= sizeof(int16);
    KALDI_LOG << "Total samples read : " << has_read_;
    return has_read_ > 0;
  }

  Vector<BaseFloat> TcpServer::GetChunk()
  {
    Vector<BaseFloat> buf;

    buf.Resize(static_cast<MatrixIndexT>(has_read_));

    for (int i = 0; i < has_read_; i++)
    {
      buf(i) = static_cast<BaseFloat>(samp_buf_[i]); 
    }
    
    return buf;
  }

  bool TcpServer::Write(const std::string &msg)
  {
    if (client_desc_ == -1)
    {
      KALDI_LOG << "No active client, ignoring";
      return false;
    }

    const char *p = msg.c_str();
    size_t to_write = msg.size();
    size_t wrote = 0;
    while (to_write > 0)
    {
      ssize_t ret = write(client_desc_, static_cast<const void *>(p + wrote), to_write);
      if (ret <= 0)
      {
        return false;
      }

      to_write -= ret;
      wrote += ret;
    }
    return true;
  }

  bool TcpServer::WriteLn(const std::string &msg, const std::string &eol)
  {
    if (Write(msg))
      return Write(eol);
    else
      return false;
  }

  void TcpServer::Disconnect()
  {
    if (client_desc_ != -1)
    {
      KALDI_LOG << "Closing client connection";
      close(client_desc_);
      client_desc_ = -1;
    }
    else
    {
      KALDI_LOG << "Client connection already closed.";
    }
  }

  void TcpServer::Kill() {
    KALDI_LOG << "Killing TCP server";
    Disconnect();
    if (server_desc_ != -1) {
      close(server_desc_);
      server_desc_ = -1;
    }
  }
} // namespace kaldi
