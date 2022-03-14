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
    uint64_t stream_index = 0;
    if (_syn) {
        //! hder.fin has not affect on stream_index
        stream_index = unwrap(WrappingInt32(hder.seqno - 1), _isn, _last_abseqno);
        //! deny re-ack syn
        if (unwrap(WrappingInt32(hder.seqno), _isn, _last_abseqno) == 0) {
            return;
        }
    }
    //! use hder.fin for state, so it should not be in a condition wrap
    _reassembler.push_substring(seg.payload().copy(), stream_index, hder.fin);
    if (_reassembler.first_unassembled() != 0) {
        //! ackno is updated by _last_abseqno, so it's not affected by out-of-order arrival
        _last_abseqno = _reassembler.first_unassembled() - 1;
        //! if condition, then must be syned
        _ackno = WrappingInt32(wrap(_last_abseqno + 1, _isn) + 1);
        if (not _syn)  // the situation when have a payload with syn
            _ackno = _ackno.value() - 1;
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
        // read tcp_receiver.hh for _fin state
        _fin = 1;
    }
    //!! TCP is out-of-order, you can't update _ackno without CALCULATED
    //!! wrong ackno may result in dropping by the opposite
    if (_fin == 1 && _reassembler.eof()) {
        stream_out().end_input();
        _fin = 2;
    }
}

// return ackno
optional<WrappingInt32> TCPReceiver::ackno() const { return _fin == 2 ? WrappingInt32{_ackno.value() + 1} : _ackno; }

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
