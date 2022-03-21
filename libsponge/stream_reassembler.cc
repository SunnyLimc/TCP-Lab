#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

// \details This function accepts a substring (aka a segment) of bytes,
// possibly out-of-order, from the logical stream, and assembles any newly
// contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (_eofed) {
        return;
    }
    if (eof && not _eof) {
        _eof = true;
        _eof_index = index + data.length();
    }

    // states and judge
    auto index_c = index;
    auto length_c = data.length();
    auto end_index = index_c + length_c - 1;
    auto max_acindex = _first_index + _output.remaining_capacity();
    if (end_index < _first_index || index_c >= max_acindex || length_c == 0) {
        if (_eof && _first_index == _eof_index) {
            _output.end_input();
            _eofed = true;
        }
        return;
    }
    size_t offset = 0;

    // preprocessing
    if (index < _first_index) {
        index_c = _first_index;
        offset = _first_index - index;
        length_c -= offset;
    }
    if (index_c + length_c > max_acindex) {
        length_c = max_acindex - index_c;
        end_index = index_c + length_c - 1;
    }
    //! c++ always return a shadow copy for retrun value optimize, so it can directly bind to && type or normal type
    //! move(it) actually get it memory ownership from reference, let x = move(it), it will trigger a shadow copy, copy
    //! the same memory to x (not the reference)
    BufferList buf{move(const_cast<string &>(data).substr(offset, length_c))};
    auto it = _unassembled.lower_bound(index_c);
    bool merged = false;
    bool finish = false;

    //! be care when convert while to if. do not omit ANY conditions
    if (it != _unassembled.cbegin() || (it != _unassembled.cend() && it->first == index_c)) {
        //!! if a seg with a same start_index existed, one of the two will be discarded
        //!! handle duplicate start_index problem
        if (index_c != it->first) {
            --it;  // if start_index is same, you do not need to check the previous index
        }
        //! plus 1 to solve the case that 0:"a" 1:"b" and combine it to "ab"
        if (it->second.first + 1 >= index_c) {
            if (it->second.first - index_c + 1 >= length_c)
                // just discard if this candidate seg covered by the previous range
                finish = true;
            else {
                auto olap_len = it->second.first - index_c + 1;  //! in the case of adjacent merge, the olap_len is 0
                buf.remove_prefix(olap_len);
                it->second.second.append(buf);
                merged = true;
                _unassembled_bytes += buf.size();
                //!! need to update states after merge
                it->second.first = end_index;
            }
        }
    }

    auto insert = it;
    if (not finish) {
        if (not merged) {
            it = _unassembled.insert(it, make_pair(index_c, make_pair(end_index, buf)));
            _unassembled_bytes += buf.size();
        }
        insert = it++;
        while (it != _unassembled.cend()) {
            if (insert->second.first >= it->second.first) {
                _unassembled_bytes -= it->second.second.size();
                it = _unassembled.erase(it);
                //! el-if implicit: insert->first.second < it->first.second
            } else if (insert->second.first + 1 >= it->first) {  //! solve the adjacent problem described above
                auto olap_len = insert->second.first - it->first + 1;
                _unassembled_bytes -= olap_len;
                it->second.second.remove_prefix(olap_len);
                insert->second.second.append(it->second.second);
                //!! need to update states after merge
                insert->second.first = it->second.first;
                _unassembled.erase(it);
                break;
            } else {
                break;
            }
            // do not need to ++it, it will automatically exectued with "it = _unassembled.erase(it);"
        }
    }
    if (insert->first == _first_index) {
        auto seg_len = insert->second.second.size();
        _first_index += seg_len;
        _unassembled_bytes -= seg_len;
        _output.write(insert->second.second.concatenate());
        _unassembled.erase(insert);
    }
    if (_eof && _first_index == _eof_index) {
        _output.end_input();
        _eofed = true;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
