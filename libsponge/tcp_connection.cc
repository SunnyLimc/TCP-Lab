#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.remain_window_size(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timer - _last_seg_timer; }

// error_type 0: normal close, 1: caused from opposite, >1: caused from self
void TCPConnection::fully_dead(const size_t error_type) {
    // once is enough
    if (error_type > 0) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        if (error_type > 1) {
            TCPSegment seg{};
            seg.header().rst = true;
            _segments_out.push(seg);
        }
    }
    _active = false;
}

size_t TCPConnection::pullpush_segments(const bool syn_only) {
    if (not _active)
        return 0;
    size_t times = 0;
    auto &sender_queue = _sender.segments_out();
    while (not sender_queue.empty()) {
        auto &seg = sender_queue.front();
        auto &hder = seg.header();
        if (syn_only && not hder.syn)
            break;
        if (_receiver.ackno().has_value()) {
            hder.ack = true;
            hder.ackno = _receiver.ackno().value();
        }
        hder.win = _receiver.window_size();
        _segments_out.push(seg);
        ++times;
        sender_queue.pop();
    }
    return times;
}

// normally close connection is always from a received segment
void TCPConnection::segment_received(const TCPSegment &seg) {
    if (not _active)
        return;

    _last_seg_timer = _timer;

    if (seg.header().rst) {
        fully_dead(1);
        return;
    }

    // segment start on a listen state
    if (_sender.syned() || (seg.header().syn && not _sender.syned())) {
        _sender.fill_window();
    }

    // something special
    //! you need to check whether <optional> exist before use it's value
    if (seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value() &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        return;
    }

    if (seg.header().ack) {
        _linger_timer = 0;
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    _receiver.segment_received(seg);

    auto fully_sended = _sender.full_fined();
    // simple ack not occupied any size, so it always can be sent
    //! we can't ack acks and sent once is enough
    if (pullpush_segments(false) == 0 && seg.length_in_sequence_space() != 0) {
        TCPSegment ack_seg{};
        if (_receiver.ackno().has_value()) {
            ack_seg.header().ack = true;
            ack_seg.header().ackno = _receiver.ackno().value();
        }
        /** Finish shutdown (our FIN was asked)
        // we ack the fin at least once, so we just need to simplified ensure it's reach
        // we fully received opposite's stream
        if (fully_sended && _receiver.fully_fined() &&
            seg.header().seqno + seg.length_in_sequence_space() == _receiver.ackno().value()) {
            fully_dead(0);
            // need to judge if both side enter singer shutdown and send fin
            if (not seg.header().fin)
                return;
        }
        */
        ack_seg.header().win = _receiver.window_size();
        _segments_out.push(ack_seg);
    }
    // if _sender not sent fin, that mean it not reach EOF. if it's sent, it will be pulled and sent above
    if (seg.header().fin && not _sender.fined()) {
        _linger_after_streams_finish = false;
    }
    // if _linger_shutdown state is false, means opposite is finished it's stream and we acked it at least once
    if (_linger_after_streams_finish == false && fully_sended) {
        fully_dead(0);
        return;
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (not _active)
        return 0;

    auto written = _sender.stream_in().write(data);
    // send segments
    _sender.fill_window();
    pullpush_segments(false);
    return written;
}

// \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (not _active)
        return;
    _sender.tick(ms_since_last_tick);

    _sender.fill_window();
    pullpush_segments(false);

    _timer += ms_since_last_tick;

    // the side which ended LATER need to PASSIVE shutdown ---> all relies on sender resend mechanism
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        fully_dead(2);
    }
    // the side which ended FIRST need to LINGERING shutdown ---> relies on wait time
    if (_sender.full_fined() && _linger_after_streams_finish) {
        _linger_timer += ms_since_last_tick;
        if (_linger_timer >= 10 * _cfg.rt_timeout) {
            _linger_after_streams_finish = 0;
            fully_dead(0);
        }
    }
}

void TCPConnection::end_input_stream() {
    if (not _active)
        return;
    _sender.stream_in().end_input();
    _sender.fill_window();
    pullpush_segments(false);
}

void TCPConnection::connect() {
    if (not _active)
        return;
    _sender.fill_window();
    pullpush_segments(false);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            fully_dead(2);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
