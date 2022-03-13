#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

//! NEED TO READ ANNOTATE CAREFULLY
//! THIS FUNCTION WASTE A LOT OF TIME OF MY LIFE
//! read ./libsponge/tcp_sponge_socket.cc:98 for more details
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _last_seg_timer; }

// error_type 0: normal close, 1: caused from opposite, >1: caused from self
void TCPConnection::fully_dead(const size_t error_type) {
    if (error_type == 0) {
        _sender.fill_window();
    } else {
        _rst = 1;
        //! once is NOT enough
        if (error_type > 1) {
            if (_sender.segments_out().empty()) {
                _sender.send_empty_segment();
            }
        }
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
    }
    pull_push_segments();
    _active = false;
}

void TCPConnection::pull_push_segments() {
    auto &sender_queue = _sender.segments_out();
    while (not sender_queue.empty()) {
        auto &seg = sender_queue.front();
        auto &hder = seg.header();
        if (_rst) {
            hder.rst = true;
        } else {
            if (_receiver.ackno().has_value()) {
                hder.ack = true;
                hder.ackno = _receiver.ackno().value();
            }
            //! need to judge whether window size out-of-limit
            hder.win = _receiver.window_size() <= UINT16_MAX ? _receiver.window_size() : UINT16_MAX;
        }
        _segments_out.push(seg);
        sender_queue.pop();
    }
    return;
}

void TCPConnection::judge_do() {
    if (not _active) {
        return;
    }
    /**
    //! TRY to judge each time:
    */
    {
        if (_rst == true) {
            fully_dead(1);
            return;
        }
        // the side which ended LATER need to PASSIVE shutdown ---> all relies on sender resend mechanism
        // RESEND TIMES IS OUT OF LIMIT
        //! end stream immediately after last + 1 times resend
        if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
            fully_dead(2);
            return;
        }
        // the side which ended FIRST need to LINGERING shutdown ---> relies on wait time
        // ACTIVE CLOSE
        //! As the side that can close the connection to the other side
        //! We need to ensure our LAST ACK can be received by opposite
        //! So we need to do time-wait if the LAST ACK lost and the other side resend FIN
        if (_linger_after_streams_finish && _sender.full_fined() && _receiver.full_fined() &&
            _last_seg_timer >= 10 * _cfg.rt_timeout) {
            _linger_after_streams_finish = false;
            fully_dead(0);
            return;
        }
        // if _linger_shutdown state is false, means opposite is finished it's stream and we acked it at least once
        // PASSIVE CLOSE
        //! We can use resend to ensure out last FIN can be acked
        //! So when _sender is fully fined, we can close the connection
        if (_linger_after_streams_finish == false && _sender.full_fined()) {
            fully_dead(0);
            return;
        }
    }
    // normal behavior
    return pull_push_segments();
}

// normally close connection is always from a received segment
void TCPConnection::segment_received(const TCPSegment &seg) {
    _last_seg_timer = 0;

    // RST caused by opposite
    if (seg.header().rst) {
        _rst = true;
        judge_do();
        return;
    }

    _receiver.segment_received(seg);

    // connection from opposite --> resend SYN + ACKS
    if (seg.header().syn && not _sender.syned()) {
        connect();
        return;
    }

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (_sender.syned()) {
        _sender.fill_window();
    }
    // simple ack not occupied any size, so it always can be sent
    //! we can't ack acks(empty segment) SO sent once is enough
    if (_active && _sender.segments_out().empty() && seg.length_in_sequence_space() != 0) {
        _sender.send_empty_segment();
    }
    // if _sender not sent fin, that mean it not reach EOF. if it's sent, it will be pulled and sent above
    //! TCP is out-of-order, each time judge whether stream fin, you need you judge the state of stream
    //! dont limit it to hder.fin(), it may received fin already in previous
    //! use _sender's state to keep it not in ACTIVE CLOSE
    if (_receiver.full_fined() && not _sender.fined()) {
        _linger_after_streams_finish = false;
    }

    judge_do();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t written = _sender.stream_in().write(data);
    if (_sender.syned()) {
        _sender.fill_window();
    }
    // send segments
    judge_do();
    return written;
}

// \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _last_seg_timer += ms_since_last_tick;

    _sender.tick(ms_since_last_tick);

    if (_sender.syned()) {
        _sender.fill_window();
    }

    judge_do();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    judge_do();
}

void TCPConnection::connect() {
    _sender.fill_window();
    judge_do();
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
