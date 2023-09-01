#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : _capacity( capacity ), 
  _buf(), _isError(false), _isClosed(false), _pushedBytes(0), _popedBytes(0)
{}

void Writer::push( string data )
{
  // Your code here.
  for(auto &&ch : data)
  {
    if (available_capacity() > 0)
    {
      _buf.push(ch);
      ++_pushedBytes;
    }
  }
  (void)data;
}

void Writer::close()
{
  // Your code here.
  _isClosed = true;
}

void Writer::set_error()
{
  // Your code here.
  _isError = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return {_isClosed};
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return {_capacity - _buf.size()};
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return {_pushedBytes};
}

string_view Reader::peek() const
{
  // Your code here.
  return {string_view(&_buf.front(), 1)};
}

bool Reader::is_finished() const
{
  // Your code here.
  return {_buf.empty() &&  _isClosed};
}

bool Reader::has_error() const
{
  // Your code here.
  return {_isError};
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  while (len-- && !_buf.empty())
  {
    _buf.pop();
    ++_popedBytes;
  }
  (void)len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return {_buf.size()};
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return {_popedBytes};
}
