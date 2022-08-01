#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t _capacity)
    : capacity(_capacity), remaining_size(capacity), buffer(), had_read(0), had_written(0), is_end_input(false) {}

size_t ByteStream::write(const string &data) {
    size_t len = min(data.length(), this->remaining_size);
    this->buffer.append(Buffer(string(data, 0, len)));
    this->had_written += len;
    this->remaining_size -= len;
    return len;
    // if (len < this->remaining_size) {
    //     this->had_written += len;
    //     this->buffer += data;
    //     this->remaining_size -= len;
    //     return len;
    // } else {
    //     size_t tmp = this->remaining_size;
    //     this->had_written += this->remaining_size;
    //     this->buffer += string(data, 0, this->remaining_size);
    //     this->remaining_size = 0;
    //     return tmp;
    // }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const { 
    const std::deque<Buffer> & _buffer = this->buffer.buffers();
    size_t n = len, limit = _buffer.size();
    string tmp;
    for(size_t i = 0; i < limit; ++i)
    {
        if(n > 0)
        {

            if(n < _buffer[i].str().size())
            {
                tmp += _buffer[i].copy().substr(0, n);
                n = 0;
            }
            else
            {
                tmp += _buffer[i].copy();
                n -= _buffer[i].str().size();
            }
        }
        else
            break;
    }
    return tmp;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    // if (len < this->buffer.length()) {
    //     this->had_read += len;
    //     this->remaining_size += len;
    //     this->buffer.erase(0, len);
    // } else {
    //     this->had_read += this->buffer.length();
    //     this->remaining_size += this->buffer.length();
    //     this->buffer.erase();
    // }
    size_t tmp = min(len, this->buffer.size());
    this->had_read += tmp;
    this->remaining_size += tmp;
    this->buffer.remove_prefix(tmp);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto tmp = this->peek_output(len);
    this->pop_output(len);
    return tmp;
}

void ByteStream::end_input() { this->is_end_input = true; }

bool ByteStream::input_ended() const { return this->is_end_input; }

size_t ByteStream::buffer_size() const { return this->buffer.size(); }

bool ByteStream::buffer_empty() const { return this->buffer.size() == 0; }

bool ByteStream::eof() const { return (this->is_end_input == true && this->buffer_empty()); }

size_t ByteStream::bytes_written() const { return this->had_written; }

size_t ByteStream::bytes_read() const { return this->had_read; }

size_t ByteStream::remaining_capacity() const { return this->remaining_size; }
