#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t cap) : _capacity(cap) {}

size_t ByteStream::write(const string &data) {
    size_t length = min(data.size(), remaining_capacity());
    for (size_t i = 0; i < length; i++) {
        _buffer.push_back(data[i]);
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

std::string ByteStream::read(const size_t len) {
    string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _input_ended_flog = true; }

bool ByteStream::input_ended() const { return _input_ended_flog; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
