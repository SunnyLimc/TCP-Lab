#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t cap) : _capacity(cap) {}

size_t ByteStream::write(const string &data) {
    size_t length = min(data.size(), remaining_capacity());
    if (data.length() != 0) {
        _buffer.append(move(const_cast<string &>(data).substr(0, length)));
    }
    _write_count += length;
    return length;
    return 0;
}

string ByteStream::peek_output(const size_t len) const { return _buffer.concatenate().substr(0, len); }

void ByteStream::pop_output(const size_t len) {
    size_t length = min(_buffer.size(), len);
    _buffer.remove_prefix(length);
    _read_count += length;
}
