#include "mmap_stream.hpp"
#include <streambuf>
#include <functional>
#include <cassert>
#include <cstring>

using namespace std;

mmap_stream::mmap_stream(const char *begin, const char *end) :
    begin_(begin),
    end_(end),
    current_(begin_)
{
    assert(std::less_equal<const char *>()(begin_, end_));
    setg((char*)begin, (char*)begin, (char*)end);
}

mmap_stream::int_type mmap_stream::underflow()
{
    if (current_ == end_) {
        return traits_type::eof();
    }
    return *current_;
}

mmap_stream::int_type mmap_stream::uflow()
{
    if (current_ == end_) {
        return traits_type::eof();
    }
    return *current_++;
}

mmap_stream::int_type mmap_stream::pbackfail(int_type ch)
{
    if (current_ == begin_ || (ch != traits_type::eof() && ch != current_[-1]))
        return traits_type::eof();
    return *--current_;
}

std::streamsize mmap_stream::showmanyc()
{
    assert(std::less_equal<const char *>()(current_, end_));
    return end_ - current_;
}

streampos mmap_stream::seekoff(streamoff off, ios_base::seekdir way, ios_base::openmode which = ios_base::in | ios_base::out) {
  if(way == ios_base::beg) {
    current_ = begin_ + off;
  } else if(way == ios_base::cur) {
    current_ += off;
  } else {
    current_ = end_ - off;
  }
  return current_ - begin_;
}

streampos mmap_stream::seekpos(streampos sp, ios_base::openmode which = ios_base::in | ios_base::out) {
  current_ = begin_ + sp;
  return current_ - begin_;
}
