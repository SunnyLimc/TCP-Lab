#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , stream(capacity + 1, {false, 0, ""})
    , _outputed(0)
    , _full(false)
    , _stored(0)
    , _eofindex(0)
    , _state(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &input_tt, size_t index, const bool eof) {
    bool modify = false;
    if (_state == 2)
        return;
    if (eof) {
        _eofindex = index + input_tt.length();
        _state = 1;
    }
    if (input_tt == "") {
        if (_outputed >= _eofindex && _state == 1) {
            _output.end_input();
            _state = 2;
        }
        return;
    }
    // You can convert this state to the state "=="
    if (index < _outputed) {
        if (index + input_tt.length() < _outputed) {
            return;
        } else {
            modify = true;
        }
    }
    const string &input_t = modify ? input_tt.substr(_outputed - index) : input_tt;
    index = modify ? _outputed : index;
    const string &input = index + input_t.length() > _capacity ? input_t.substr(0, _capacity - index) : input_t;
    size_t subleft = 0;
    size_t length = input.length();
    size_t behindindex = index + length;
    if (index == _outputed) {
        if (stream[behindindex - 1].used == false) {
            _output.write(input);
            _outputed += input.length();
        } else {
            const size_t pointindex = stream[behindindex - 1].leftpoint;
            const size_t newinputlen = pointindex - index;
            const string newinput = input.substr(0, newinputlen);
            _output.write(newinput);
            _outputed += newinputlen;
        }
        // if there are still a point, continue read it
        while (stream[_outputed].leftpoint == _outputed) {
            _output.write(stream[_outputed].data);
            size_t datalen = stream[_outputed].data.length();
            _stored -= datalen;
            _outputed += datalen;
        }
    } else {
        if (stream[index].used) {
            const size_t pointindex = stream[index].leftpoint;
            const size_t pointlen = stream[pointindex].data.length();
            const size_t newindex = pointindex + pointlen;
            subleft = newindex - index;
            length = length - subleft;
            index = newindex;
            modify = true;
        }
        if (stream[behindindex - 1].used) {
            const size_t pointindex = stream[behindindex - 1].leftpoint;
            length = length - (behindindex - pointindex);
            modify = true;
        }
        if (length <= 0) {
            return;
        }
        const string &newinput = modify ? input.substr(subleft, length) : input;
        stream[index].data = newinput;
        for (size_t i = index; i < index + newinput.length(); ++i) {
            stream[i].used = true;
            stream[i].leftpoint = index;
        }
        _stored += newinput.length();
    }
    if (_outputed >= _eofindex && _state == 1) {
        _output.end_input();
        _state = 2;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _stored; }

bool StreamReassembler::empty() const { return _stored == 0; }
