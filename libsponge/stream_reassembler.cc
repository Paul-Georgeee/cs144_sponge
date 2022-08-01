#include "stream_reassembler.hh"
#include <assert.h>
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _buffer(capacity, '\0'), _bitmap(capacity, false), _next_expected_index(0), _get_last_string(false), _unressembler_size(0){}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    
    
    uint64_t max_index = this->_next_expected_index + this->_capacity - this->_output.buffer_size();
    uint64_t begin_index, end_index;
    begin_index = max(index, this->_next_expected_index);
    if(index + data.length() > max_index)
        end_index = max_index;
    else
    {
        this->_get_last_string = eof == false ? this->_get_last_string : true;
        end_index = index + data.length();
    }
    if(begin_index > end_index)
        return;


    size_t limit = this->_capacity - this->_output.buffer_size() - this->_buffer.size();
    for(size_t i = 0; i < limit; ++i)
    {
        this->_buffer.push_back('\0');
        this->_bitmap.push_back(false);
    }

    assert(this->_capacity == this->_output.buffer_size() + this->_buffer.size());
    assert(this->_capacity == this->_output.buffer_size() + this->_bitmap.size());

    
    for(size_t i = begin_index - index; i < end_index - index; ++i)
    {
        size_t index_in_buffer = i + index - this->_next_expected_index;
        assert(index_in_buffer < this->_buffer.size());
        if(this->_bitmap[index_in_buffer] == false)
        {
            this->_bitmap[index_in_buffer] = true;
            this->_unressembler_size++;
            this->_buffer[index_in_buffer] = data[i];
        }
    }
    string tmp;
    assert(this->_capacity == this->_output.buffer_size() + this->_buffer.size());
    assert(this->_capacity == this->_output.buffer_size() + this->_bitmap.size());

    while (this->_bitmap.empty() == false && this->_bitmap.front() == true)
    {
        tmp.push_back(this->_buffer.front());
        this->_buffer.pop_front();
        this->_bitmap.pop_front();
        this->_unressembler_size--;
        this->_next_expected_index++;
    }

    assert(tmp[tmp.length()] == '\0');

    if(tmp.length() != 0)
        this->_output.write(tmp);
    
    if(this->_get_last_string == true && this->_unressembler_size == 0)
        this->_output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return this->_unressembler_size; }

bool StreamReassembler::empty() const { return this->_unressembler_size == 0; }
