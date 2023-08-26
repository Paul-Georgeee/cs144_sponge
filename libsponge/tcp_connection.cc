#include "tcp_connection.hh"

#include "assert.h"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return this->_sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return this->_sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return this->_receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const {
    return this->_sender.gettick() - this->_time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    const TCPHeader &head = seg.header();

    assert(this->_sender.segments_out().empty() == true);
    _time_since_last_segment_received = this->_sender.gettick();
    if (head.rst == true) {
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        this->_is_abort = true;
    }

    if (head.fin == true) {
        this->_is_recv_fin = true;
        if (this->_sender.is_sent_fin() == false)
            this->_linger_after_streams_finish = false;
    }

    this->_receiver.segment_received(seg);
    if (head.ack == true && this->_sender.is_sent_syn())
        this->_sender.ack_received(head.ackno, head.win);

    if (seg.length_in_sequence_space() != 0) {
        // TODO()
        // Make sure that at least one segment.
        // In ack_received func may will call fill_windows, so I think can see if the
        // queue of segment_out is empty, if not, we should send an empty segment
        this->_sender.fill_window();
        if (this->_sender.segments_out().empty() == true)
            this->_sender.send_empty_segment();
    }

    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        head.seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }

    this->send_segment();
}

bool TCPConnection::active() const {
    if (this->_is_abort == true)
        return false;
    else if (this->_linger_after_streams_finish == false) {
        return !(this->_sender.is_sent_fin() && this->_sender.bytes_in_flight() == 0 &&
                 this->_receiver.unassembled_bytes() == 0);
    } else {
        if (this->_sender.is_sent_fin() && this->_sender.bytes_in_flight() == 0 && this->_is_recv_fin == true &&
            this->_receiver.unassembled_bytes() == 0) {
            if (this->_sender.gettick() - this->_time_since_last_segment_received >= 10 * _cfg.rt_timeout)
                return false;
            else
                return true;
        } else
            return true;
    }
}

size_t TCPConnection::write(const string &data) {
    size_t len = this->_sender.stream_in().write(data);
    this->_sender.fill_window();
    this->send_segment();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    this->_sender.tick(ms_since_last_tick);
    if (this->_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // TODO(): send segment with rst
        std::queue<TCPSegment> &q = this->_sender.segments_out();
        while (q.empty() == false)
            q.pop();

        this->send_segment_with_rst();
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        this->_is_abort = true;
    }
    this->send_segment();
}

void TCPConnection::end_input_stream() {
    this->_sender.stream_in().end_input();
    this->_sender.fill_window();
    this->send_segment();
}

void TCPConnection::connect() {
    this->_sender.fill_window();
    this->send_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            this->send_segment_with_rst();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segment() {
    std::queue<TCPSegment> &q = this->_sender.segments_out();
    while (q.empty() == false) {
        auto it = q.front();
        q.pop();
        if (this->_receiver.ackno().has_value()) {
            it.header().ack = true;
            it.header().ackno = this->_receiver.ackno().value();
            it.header().win = min(this->_receiver.window_size(), static_cast<size_t>(0xffff));
        }
        this->_segments_out.push(it);
    }
    assert(this->_sender.segments_out().empty() == true);
}

void TCPConnection::send_segment_with_rst() {
    std::queue<TCPSegment> &q = this->_sender.segments_out();
    assert(q.empty() == true);
    this->_sender.send_empty_segment();
    auto seg = q.front();
    TCPHeader &head = seg.header();

    head.rst = true;
    if (this->_receiver.ackno().has_value()) {
        head.ack = true;
        head.ackno = this->_receiver.ackno().value();
        head.win = min(this->_receiver.window_size(), static_cast<size_t>(0xffff));
    }
    q.pop();
    this->_segments_out.push(seg);
}