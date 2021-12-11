#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

class StreamReassembler {
  private:
    ByteStream _output;
    size_t _capacity;

    struct _node {
        size_t index = 0;
        std::string data = "";
        bool operator<(const _node n) const { return index < n.index; }
    };
    struct _size_t_resp {
        size_t resp;
        bool flag;
    };

    std::set<_node> _unassembled_nodes = {};
    std::vector<char> _pre_output = {};
    size_t _first_unassembled = 0;
    size_t _unassembled_bytes = 0;
    bool _eof = false;
    size_t _eof_index = 0;
    _size_t_resp merge_nodes(_node &node1, const _node &node2);

  public:
    StreamReassembler(const size_t capacity);

    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }

    size_t unassembled_bytes() const;

    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
