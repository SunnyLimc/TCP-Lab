#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.remain_window_size(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

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

size_t TCPConnection::pull_segments(const bool syn_only) {
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

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (seg.header().rst)
        fully_dead(1);
    if (seg.header().ack)
        _sender.ack_received(seg.header().ackno, seg.header().win);
    if (seg.header().fin)
        _linger_after_streams_finish = false;
    // something special
    if (seg.length_in_sequence_space() == 0 && seg.header().seqno == _receiver.ackno().value() - 1 &&
        _receiver.ackno().has_value())
        _sender.send_empty_segment();
    // not exist ack ACK Segments so sent once is enough
    if (pull_segments(false) == 0) {
        TCPSegment ack_seg{};
        auto &hder = ack_seg.header();
        if (_receiver.ackno().has_value()) {
            hder.ack = true;
            hder.ackno = _receiver.ackno().value();
        }
        hder.win = _receiver.window_size();
        _segments_out.push(ack_seg);
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    auto written = _sender.stream_in().write(data);
    // send segments
    _sender.fill_window();
    pull_segments(false);
    return written;
}

// \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    // the side which ended LATER don't need to lingering shutdown ---> all relies on sender resend mechanism
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        fully_dead(2);
    }
    // the side which ended FIRST need to lingering shutdown
    if (_sender.full_fined()) {
        _linger_timer += ms_since_last_tick;
        if (_linger_timer >= 10 * _cfg.rt_timeout)
            fully_dead(0);
    }
}

void TCPConnection::end_input_stream() { _sender.stream_in().end_input(); }

void TCPConnection::connect() {
    _sender.fill_window();
    pull_segments(true);
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
