#ifdef __cplusplus

#pragma once
#include <streambuf>
#include <iostream>
#include <sys/mman.h>
using namespace std;
class mmap_stream : public std::streambuf
{
    public:
        mmap_stream(const char *begin, const char *end);
        const char * const begin_;
        const char * const end_;
        const char * current_;
        ~mmap_stream() {
          munmap((void*)begin_, end_ - begin_);
        }
    private:
        int_type uflow() override;
        int_type underflow() override;
        int_type pbackfail(int_type ch) override;
        streampos seekoff(streamoff off, ios_base::seekdir way, ios_base::openmode which) override;
        streampos seekpos(streampos sp, ios_base::openmode which) override;
        std::streamsize showmanyc() override;
        
};

#endif

