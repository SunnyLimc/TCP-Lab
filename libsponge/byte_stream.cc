#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t cap) : _capacity(cap) {}

size_t ByteStream::write(const string &data) {
    size_t length = min(data.size(), remaining_capacity());
    for (size_t i = 0; i < length; i++) {
        _buffer.emplace_back(data[i]);
    }
    _write_count += length;
    return length;
}

string ByteStream::peek_output(const size_t len) const {
    return string(_buffer.begin(), _buffer.begin() + min(_buffer.size(), len));
}

void ByteStream::pop_output(const size_t len) {
    size_t length = min(_buffer.size(), len);
    for (size_t i = 0; i < length; i++) {
        _buffer.pop_front();
    }
    _read_count += length;
}
