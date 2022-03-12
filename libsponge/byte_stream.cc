#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const uint64_t cap) : _capacity(cap) {}

uint64_t ByteStream::write(const string &data) {
    uint64_t length = min(data.size(), remaining_capacity());
    for (uint64_t i = 0; i < length; i++) {
        _buffer.push_back(data[i]);
    }
    _write_count += length;
    return length;
}

string ByteStream::peek_output(const uint64_t len) const {
    return string(_buffer.begin(), _buffer.begin() + min(_buffer.size(), len));
}

void ByteStream::pop_output(const uint64_t len) {
    uint64_t length = min(_buffer.size(), len);
    for (uint64_t i = 0; i < length; i++) {
        _buffer.pop_front();
    }
    _read_count += length;
}

std::string ByteStream::read(const uint64_t len) {
    string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _input_ended_flag = true; }

bool ByteStream::input_ended() const { return _input_ended_flag; }

uint64_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

uint64_t ByteStream::bytes_written() const { return _write_count; }

uint64_t ByteStream::bytes_read() const { return _read_count; }

uint64_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
