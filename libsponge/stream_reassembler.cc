#include "stream_reassembler.hh"

using namespace std;

StreamReassembler::StreamReassembler(const uint64_t capacity) : _output(capacity), _capacity(capacity) {}

// return merged length
StreamReassembler::_uint64_t_resp StreamReassembler::merge_nodes(_node &node1, const _node &node2) {
    // a->ahead, b->behind
    uint64_t a_inx, a_len, b_inx, b_len;
    bool reverse = false;
    if (node1.index > node2.index) {
        reverse = true;
    }
    if (not reverse) {
        a_inx = node1.index;
        a_len = node1.data.length();
        b_inx = node2.index;
        b_len = node2.data.length();
    } else {
        a_inx = node2.index;
        a_len = node2.data.length();
        b_inx = node1.index;
        b_len = node1.data.length();
    }
    if (a_inx + a_len < b_inx) {
        return {0, false};
    }
    // two cases
    if (a_inx + a_len >= b_inx + b_len) {
        if (reverse) {
            node1.data.assign(node2.data, 0);
        }
    } else {
        uint64_t unique = b_inx - a_inx;
        if (not reverse) {
            node1.data.assign(node1.data.substr(0, unique) + node2.data, 0);
        } else {
            node1.data.assign(node2.data.substr(0, unique) + node1.data, 0);
        }
    }
    node1.index = a_inx;
    return {node2.data.length(), true};
}

void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    // ! remain _capacity is dynamic
    _eof = _eof || eof;
    // 此处需注意，index + data.length() 比实际下标多 1
    // 所以 first_unassembled 无歧义
    _eof_index = eof ? index + data.length() : _eof_index;
    // data have been assembled
    if (index + data.length() <= _first_unassembled || data.empty()) {
        if (_eof && _first_unassembled == _eof_index) {
            _output.end_input();
        }
        return;
    }
    // _first_unassembled + avaliable_capacity
    uint64_t first_unaccpetable = _first_unassembled + _output.remaining_capacity();
    string data_t = data;
    uint64_t index_t = index;
    if (index_t < _first_unassembled) {
        data_t = data.substr(_first_unassembled - index);
        index_t = _first_unassembled;
    }
    if (index_t + data_t.length() > first_unaccpetable) {
        data_t = data_t.substr(0, first_unaccpetable - index_t);
    }
    _node elm = {index_t, data_t};
    // merge to RIGHT
    auto iter = _unassembled_nodes.lower_bound(elm);
    while (iter != _unassembled_nodes.end()) {
        auto resp = merge_nodes(elm, *iter);
        if (resp.flag == false) {
            break;  // iter will stop at a correct position
        } else {
            _unassembled_bytes -= resp.resp;
            _unassembled_nodes.erase(iter++);
        }
    }
    // merge to LEFT
    if (iter != _unassembled_nodes.begin()) {  // empty check
        --iter;                                // iter is not at end
        _uint64_t_resp resp;
        while ((resp = merge_nodes(elm, *iter)).flag != false) {
            _unassembled_bytes -= resp.resp;
            if (iter == _unassembled_nodes.begin()) {
                _unassembled_nodes.erase(iter);
                break;
            } else {
                _unassembled_nodes.erase(iter--);
            }
        }
    }
    // no operation size required after merge
    if (elm.index <= _first_unassembled) {
        uint64_t write_bytes = _output.write(elm.data);
        _first_unassembled += write_bytes;
    } else {
        _unassembled_nodes.insert(elm);
        _unassembled_bytes += elm.data.length();
    }
    if (_eof && _first_unassembled == _eof_index) {
        _output.end_input();
    }
}

uint64_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
