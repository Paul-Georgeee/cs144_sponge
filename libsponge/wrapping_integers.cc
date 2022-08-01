#include "wrapping_integers.hh"
#include <cstdlib>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32{static_cast<uint32_t>(n & 0xffffffff) + isn.raw_value()};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
static inline uint64_t getabs(int64_t a)
{
    return a < 0 ? -a : a;
}

uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t tmp_a = static_cast<uint32_t>(n - isn) + (checkpoint & 0xffffffff00000000);
    uint64_t tmp_b = static_cast<uint32_t>(n - isn) + (checkpoint & 0xffffffff00000000) - 0x100000000;
    uint64_t tmp_c = static_cast<uint32_t>(n - isn) + (checkpoint & 0xffffffff00000000) + 0x100000000;

    //uint64_t differ_a = abs(int64_t(tmp_a) - int64_t(checkpoint)), differ_b = abs(int64_t(tmp_b) - int64_t(checkpoint)), differ_c = abs(int64_t(tmp_c) - int64_t(checkpoint));
    
    uint64_t differ_a = max(tmp_a, checkpoint) - min(tmp_a, checkpoint), differ_b = max(tmp_b, checkpoint) - min(tmp_b, checkpoint), differ_c = max(tmp_c, checkpoint) - min(tmp_c, checkpoint);

    if(differ_a < differ_b)
    {
        if(differ_a < differ_c)
            return tmp_a;
        else
            return tmp_c;
    }
    else
    {
        if(differ_b < differ_c)
            return tmp_b;
        else
            return tmp_c;
    }
}
