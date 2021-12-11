#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

class StreamReassembler {
  private:
  private:
    // Your code here -- add private members as necessary.

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

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
