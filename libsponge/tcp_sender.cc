#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
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

uint64_t TCPSender::bytes_in_flight() const { return _next_abs_seqno - _last_acked_abs_seqno; }

void TCPSender::fill_window() {
    size_t ex = 0;
    if (_window_size == 0)
        ex = 1;

    //! _window_size is Dynamic
    //! _window_size = the part of sending + the part of future send
    while (_window_size + ex > bytes_in_flight()) {
        if (_fined) {
            break;
        }
        TCPSegment segment{};
        uint64_t allow_size = _window_size - bytes_in_flight() + ex;

        ex &= 0;

        // send syn packet and wait for response
        // may with payload
        //! The first packet of sender must be SYN
        if (not _syned) {
            segment.header().syn = true;
            _syned = true;
            --allow_size;
        }

        size_t read_size = TCPConfig::MAX_PAYLOAD_SIZE;
        if (allow_size < read_size)
            read_size = allow_size;
        Buffer buf = Buffer{_stream.read(read_size)};
        segment.payload() = buf;
        allow_size -= buf.size();

        if (not _fined && allow_size > 0) {
            if (_stream.input_ended() && _stream.buffer_empty()) {
                segment.header().fin = true;
                _fined = true;
                --allow_size;
            }
        }

        auto segment_length = segment.length_in_sequence_space();
        if (segment_length == 0) {
            break;
        }

        //! compatible with native api
        segment.header().seqno = ex == 0 ? wrap(_next_abs_seqno, _isn) : wrap(_next_abs_seqno, _isn) - 1;
        _segments_out.push(segment);

        // states update
        // PLUS first then calculate push
        _next_abs_seqno += segment_length;

        _sent.push_back({_next_abs_seqno, segment});

        //! start timer. (empty)
    }
}

// \param ackno The remote receiver's ackno (acknowledgment number)
// \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //! if window size < (below) that mean it relys on resend rather than send new segments
    _window_size = window_size;

    // convert to abs seqno, NOT stream index, not need to minus 1
    auto tmp = unwrap(ackno, _isn, _last_acked_abs_seqno);
    //! distinguish "||" and "&&”       PLEASE!!!!!!!!!!!!!!!!!!!!!!
    if (tmp <= _last_acked_abs_seqno || tmp > _next_abs_seqno)  //! two prerequisites
        return;
    _last_acked_abs_seqno = tmp;

    // ! DEBUG
    // cerr << "\033[36m(-) Arrv handle: ackno: " << ackno << ", _last_abs_eqno: " << _last_acked_abs_seqno << "\033[0m"
    //  << endl;

    // merge recend seq
    if (not _sent.empty()) {
        // reset some value of states
        _timeout = _initial_retransmission_timeout;
        _last_time = 0;
        _resend_try_times = 0;
        // check from the recent checkpoint
        //! iterator use the format [begin, end)
        for (auto iter = _sent.begin(); iter < _sent.cend() && (iter->expected_ack <= _last_acked_abs_seqno); ++iter) {
            // cerr << "\033[36m(-) Arrv POP: seqno: " << iter->seg.header().seqno
            //  << ", expected_ack: " << iter->expected_ack << "\033[0m" << endl;
            _sent.pop_front();
        }
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
        //!! don't need to use calculated window_size
        if (_window_size != 0) {
            _timeout = 2 * _timeout;
            ++_resend_try_times;
        }
        //! if no segment available, the timer need to be reset,
        //! otherwise, it will result in advance counting
    } else if (_sent.empty()) {
        _last_time = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _resend_try_times; }

void TCPSender::send_empty_segment() {
    TCPSegment seg{};
    //!! need to give each empty seg seqno
    //!! TCPConnection need it and will not automatically fill seqno for it
    seg.header().seqno = wrap(_next_abs_seqno, _isn);
    _segments_out.push(seg);
}