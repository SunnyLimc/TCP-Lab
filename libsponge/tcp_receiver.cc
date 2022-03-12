#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &hder = seg.header();
    if ((not hder.syn && not _syn) || (hder.syn && _syn)) {
        return;
    }
    /** DEPRECATED: (dont do that)
        // deny modify fully ack index
        // but allow modify current seqno
        // allow previous seqno because arrival is out-of-order
        // if (_syn && hder.seqno < _ackno.value() && hder.seqno.raw_value() != _last_seqno) {
        //     return;
        // }
    */
    if (seg.payload().size() != 0) {
        uint64_t abseqno = 0;
        if (_syn) {
            abseqno = unwrap(WrappingInt32(hder.seqno - 1), _isn, _checkpoint);
            //! deny re-ack syn
            if (unwrap(WrappingInt32(hder.seqno), _isn, _checkpoint) == 0) {
                return;
            }
        }
        _reassembler.push_substring(seg.payload().copy(), abseqno, _fin);
        if (_reassembler.first_unassembled() != 0) {
            //! ackno is updated by checkpoint, so it's not affected by out-of-order arrival
            _checkpoint = _reassembler.first_unassembled() - 1;
            _ackno = WrappingInt32(wrap(_checkpoint + 1, _isn) + 1);
            if (not _syn)  // payload with syn
                _ackno = _ackno.value() - 1;
        }
    }
    if (hder.syn) {
        _syn = true;
        _isn = hder.seqno;
        if (_ackno.has_value())
            _ackno = _isn + 1 + _ackno.value().raw_value();
        else
            _ackno = _isn + 1;
    }
    if (hder.fin) {
        _fin = true;
    }
    if (_fin && not _full_fined && unassembled_bytes() == 0) {
        stream_out().end_input();
        _ackno = _ackno.value() + 1;
        _full_fined = true;
    }
    _last_seqno = hder.seqno.raw_value();
}

// return ackno
optional<WrappingInt32> TCPReceiver::ackno() const { return _ackno; }

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
