#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <assert.h>
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout)
    , _ticks(0)
    , _bytes_in_flight(0)
    , _winsize(1)
    , _is_send_syn(false)
    , _is_send_fin(false) {}

uint64_t TCPSender::bytes_in_flight() const { return this->_bytes_in_flight; }

void TCPSender::fill_window() {
    if (this->_is_send_syn == false) {
        TCPSegment seg;
        seg.header().seqno = this->next_seqno();
        seg.header().syn = true;
        this->_is_send_syn = true;
        this->_bytes_in_flight++;
        this->_next_seqno++;
        this->_wait_for_ack.push(seg);
        this->_segments_out.push(seg);
        this->_timer.start_timer(this->_ticks);
        return;
    }
    if (this->_winsize != 0) {
        while (this->_winsize > this->_bytes_in_flight) {
            TCPSegment seg;
            seg.header().seqno = this->next_seqno();
            size_t max_len = min(this->_winsize - this->_bytes_in_flight, TCPConfig::MAX_PAYLOAD_SIZE);
            seg.payload() = this->_stream.read(max_len);
            size_t len = seg.payload().size();

            if (len < this->_winsize - this->_bytes_in_flight && this->_stream.eof() && this->_is_send_fin == false) {
                seg.header().fin = true;
                this->_is_send_fin = true;
            }

            if (seg.length_in_sequence_space() == 0)
                break;
            this->_bytes_in_flight += seg.length_in_sequence_space();
            this->_next_seqno += seg.length_in_sequence_space();
            this->_wait_for_ack.push(seg);
            this->_segments_out.push(seg);
            this->_timer.start_timer(this->_ticks);
        }
    } else {
        if(this->_wait_for_ack.empty() == false)
            return;
        TCPSegment seg;
        seg.header().seqno = this->next_seqno();
        seg.payload() = this->_stream.read(1);
        if (seg.payload().size() == 0) {
            if (this->_stream.eof() && this->_is_send_fin == false) {
                seg.header().fin = true;
                this->_is_send_fin = true;
            } else
                return;
        }

        assert(seg.length_in_sequence_space() == 1);
        this->_bytes_in_flight += 1;
        this->_next_seqno += 1;
        this->_wait_for_ack.push(seg);
        this->_segments_out.push(seg);
        this->_timer.start_timer(this->_ticks);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (ackno - this->next_seqno() > 0)
        return;

    this->_winsize = window_size;
    bool flag = false;
    while (this->_wait_for_ack.empty() == false) {
        auto it = this->_wait_for_ack.front();
        auto len = it.length_in_sequence_space();
        if (it.header().seqno + len - ackno <= 0) {
            this->_wait_for_ack.pop();
            this->_bytes_in_flight -= len;
            flag = true;
        } else
            break;
    }
    if (flag == true && this->_wait_for_ack.empty() == false)
        this->_timer.restart_timer(this->_ticks);
    else if (this->_wait_for_ack.empty() == true)
        this->_timer.end_timer();

    this->fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    this->_ticks += ms_since_last_tick;
    if (this->_timer.running == true) {
        assert(this->_wait_for_ack.empty() == false);
        if (this->_timer.check_timer(this->_ticks) == true) {
            this->_segments_out.push(this->_wait_for_ack.front());
            if (this->_winsize != 0)
                this->_timer.update_rto();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return this->_timer.retransmit_times; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = this->next_seqno();
    this->_segments_out.push(seg);
}

void TCPSenderTimer::start_timer(uint64_t nowtick) {
    if (this->running == true)
        return;
    else {
        assert(this->retransmit_times == 0);
        this->running = true;
        this->begintime = nowtick;
    }
}

void TCPSenderTimer::end_timer() {
    this->running = false;
    this->retransmit_times = 0;
    this->rto = this->initial_rto;
}

void TCPSenderTimer::restart_timer(uint64_t nowtick) {
    assert(this->running == true);
    this->retransmit_times = 0;
    this->begintime = nowtick;
    this->rto = this->initial_rto;
}

bool TCPSenderTimer::check_timer(uint64_t nowtick) {
    assert(this->running == true);
    if (nowtick - this->begintime < this->rto)
        return false;
    else {
        this->begintime = nowtick;
        return true;
    }
}

void TCPSenderTimer::update_rto() {
    this->retransmit_times++;
    this->rto += this->rto;
}