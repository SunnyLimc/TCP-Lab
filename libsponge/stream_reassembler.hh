#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

// first_unacceptible = readed + first_unassembled
// readed = remaining_capacity()

//
//  <- _capacity ->             ↓ first_unacceptible
// | ↓ readed      |            |
// |___|___________|____________|
//   _output ↑    |↑| first_unassembled
//   byte_stream.cc  stream_reassembler.hh
class StreamReassembler {
  private:
    ByteStream _output;
    uint64_t _capacity;

    struct _node {
        uint64_t index = 0;
        std::string data = "";
        bool operator<(const _node n) const { return index < n.index; }
    };
    struct _uint64_t_resp {
        uint64_t resp;
        bool flag;
    };

    std::set<_node> _unassembled_nodes = {};
    std::vector<char> _pre_output = {};
    // stream index
    uint64_t _first_unassembled = 0; 
    uint64_t _unassembled_bytes = 0;
    bool _eof = false;
    uint64_t _eof_index = 0;
    _uint64_t_resp merge_nodes(_node &node1, const _node &node2);

  public:
    StreamReassembler(const uint64_t capacity);

    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }

    uint64_t unassembled_bytes() const;

    bool empty() const;

    uint64_t first_unassembled() const {return _first_unassembled;}
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
