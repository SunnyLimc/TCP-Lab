#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , confirmed(-1)
    , confirmed_used(0)
    , nextup(0)
    , nextend(0)
    , next_used(false)
    , stored(0)
    , unprocessed(vector<_data>(capacity, {0, false, NULL})) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // !
    if (unprocessed[index].used || index <= confirmed && confirmed_used) {
        return;
    }
    if (index == confirmed + 1 && confirmed_used || index == 0 && confirmed_used == false) {
        if (next_used && index == nextup) {
            int i;
            for (i = nextup; unprocessed[i].used; ++i) {
                _output.write(unprocessed[i].data);
            }
            nextup = unprocessed[i - 1].next;
            if (i - 1 > nextend) {
                nextend = i - 1;
            }
            confirmed = i - 1;
        } else {
            _output.write(data);
            ++confirmed;
        }
        if (!confirmed_used)
            confirmed_used = true;
        return;
    }

    //! UPDATE DATA and INDEX
    unprocessed[index].used = true;
    unprocessed[index].data = data;
    if (next_used == false) {
        nextend = index;
        nextup = index;
        next_used = true;
    } else if (index < nextup) {
        unprocessed[index].next = nextup;
        nextup = index;
    } else if (index > nextend) {
        unprocessed[nextend].next = index;
        unprocessed[index].next = index;
        nextend = index;
    } else {
        size_t ret = index;
        for (int i = nextup; unprocessed[i].next != i; i = unprocessed[i].next) {
            if (unprocessed[i].next > index) {
                break;
            }
            ret = i;
        }
        unprocessed[index].next = unprocessed[ret].next;
        unprocessed[ret].next = index;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return stored; }

bool StreamReassembler::empty() const { stored == 0; }
