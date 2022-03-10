#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

using namespace std;

// \param[in] capacity the capacity of the outgoing byte stream
// \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
// \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
    _timeout = _initial_retransmission_timeout;
}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _last_acked_seqno; }

inline size_t TCPSender::send_segment(TCPSegment &segment, const size_t step_size, const uint16_t window_size) {
    if (_fined)
        return 0;
    segment.header().seqno = wrap(_next_seqno, _isn);
    if (_stream.input_ended() && _stream.buffer_empty() && window_size != 0) {
        segment.header().fin = true;
        if (_saved_window_size != 0)
            --_saved_window_size;
        ++_next_seqno;
    }
    // dont send empty segment after update
    if (segment.length_in_sequence_space() == 0)
        return 0;
    _segments_out.push(segment);
    // PLUS first then calculate push
    _next_seqno += step_size;
    // minus 1 to ensure the end position right
    _sent.push_back({_next_seqno - 1, segment});
    if (segment.header().fin)
        _fined = true;
    return 1;
}

void TCPSender::fill_window() {
    // send syn packet and wait for response
    if (not _syn_acked) {
        if (_syned)
            return;
        TCPSegment segment{};
        segment.header().syn = true;
        send_segment(segment, 1, _saved_window_size);
        _syned = true;
        return;
    }
    if (_zero_window_size == 1) {
        TCPSegment segment{};
        Buffer buf = Buffer{_stream.read(1)};
        segment.payload() = buf;
        send_segment(segment, buf.size(), 1 - buf.size());
        _zero_window_size = 2;
    }
    while (_saved_window_size > 0) {
        TCPSegment segment{};
        uint32_t read_size = TCPConfig::MAX_PAYLOAD_SIZE;
        if (_saved_window_size < read_size)
            read_size = _saved_window_size;
        Buffer buf = Buffer{_stream.read(read_size)};
        segment.payload() = buf;
        _saved_window_size -= buf.size();
        // SYN has been included in buf.size()
        if (send_segment(segment, buf.size(), _saved_window_size) == 0)
            break;
    }
}

// \param ackno The remote receiver's ackno (acknowledgment number)
// \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (_fined && _last_acked_seqno == _next_seqno)
        return;
    // convert to abs seqno, NOT stream index, not need to minus 1
    auto &&tmp = unwrap(ackno, _isn, _last_acked_seqno);
    if (tmp <= _next_seqno)
        _last_acked_seqno = tmp;
    _saved_window_size =
        window_size <= (_next_seqno - _last_acked_seqno) ? 0 : (window_size - (_next_seqno - _last_acked_seqno));
    _zero_window_size = window_size == 0 ? 1 : 0;
    if (not _syn_acked && _last_acked_seqno == 1)
        _syn_acked = true;
    // merge recend seq
    if (_syn_acked && not _sent.empty()) {
        // reset some value of states
        if (_sent.front().seqend < _last_acked_seqno) {
            _timeout = _initial_retransmission_timeout;
            _last_time = 0;
            _resend_try_times = 0;
        }
        // check from the recent checkpoint
        //! IMPORTANT iterator use [begin, end)
        for (auto iter = _sent.begin(); iter < _sent.cend() && (iter->seqend < _last_acked_seqno); ++iter)
            _sent.pop_front();
    }
}

// \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _last_time += ms_since_last_tick;
    if (not _sent.empty() && _last_time >= _timeout) {
        // resend segment
        _segments_out.push(_sent.front().seg);
        _last_time = 0;
        // increase the value of some states
        if (_zero_window_size == 0) {
            _timeout = 2 * _timeout;
            ++_resend_try_times;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _resend_try_times; }

void TCPSender::send_empty_segment() {
    TCPSegment seg{};
    send_segment(seg, 0, 0);
}
