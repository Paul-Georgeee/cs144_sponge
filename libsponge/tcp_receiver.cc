#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader & head = seg.header();
    

    if(this->_rece_syn == false && head.syn == false)
        return;
    else if(this->_rece_syn == false && head.syn == true)
    {
        this->_rece_syn = true;
        this->_isn = WrappingInt32(head.seqno);
    }

    if(head.syn == true)
        this->_reassembler.push_substring(seg.payload().copy(), unwrap(head.seqno, this->_isn, this->_reassembler.next_expected_index()), head.fin);
    else
        this->_reassembler.push_substring(seg.payload().copy(), unwrap(head.seqno, this->_isn, this->_reassembler.next_expected_index()) - 1, head.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(this->_rece_syn == false)
        return {}; 
    else
    {
        uint64_t ret = this->_reassembler.next_expected_index() + 1;
        if(this->_reassembler.finsih() == true)
            ret++;       
        return wrap(ret, this->_isn);
    }
}

size_t TCPReceiver::window_size() const { return this->_capacity - this->stream_out().buffer_size(); }
