#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`
using namespace std;

ByteStream::ByteStream(const size_t cap) : capacity(cap) {}

size_t ByteStream::write(const string &data) {
    int ret;
    if (stream.length() == capacity)
        return 0;
    if (data.length() + stream.length() <= capacity) {
        stream += data;
        ret = data.length();
    } else {
        int size_o = stream.length();
        stream += data.substr(0, capacity - stream.length());
        ret = capacity - size_o;
    }
    pushed += ret;
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret;
    if (len <= 0)
        return "";
    if (len >= stream.length()) {
        ret = stream;
    } else {
        ret = stream.substr(0, len);
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len <= 0)
        return;
    if (len >= stream.length()) {
        stream = "";
    } else {
        stream = stream.substr(len);
    }
    popped += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ret;
    if (len <= 0)
        return "";
    if (len >= stream.length()) {
        ret = stream;
        pop_output(len);
    } else {
        ret = stream.substr(0, len);
        pop_output(len);
    }
    return ret;
}

void ByteStream::end_input() { inputEnded = true; }

bool ByteStream::input_ended() const { return inputEnded; }

size_t ByteStream::buffer_size() const { return stream.length(); }

bool ByteStream::buffer_empty() const { return stream.length() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return pushed; }

size_t ByteStream::bytes_read() const { return popped; }

size_t ByteStream::remaining_capacity() const { return capacity - stream.length(); }
