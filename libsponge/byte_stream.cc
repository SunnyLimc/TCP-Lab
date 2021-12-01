#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&.../* unused */ s) {

// }

using namespace std;

ByteStream::ByteStream(const size_t capacity) : capacity(capacity), streamLen(0) { this->stream = deque<string>(0); }

size_t ByteStream::write(const string &data) {
    if (streamLen == capacity) {
        return 0;
    }
    int dataLen = data.length();
    int availableLen = capacity - streamLen;
    if (dataLen <= availableLen) {
        stream.push_back(data);
        streamLen += dataLen;
        return dataLen;
    } else {
        stream.push_back(data.substr(0, availableLen));
        streamLen = capacity;
        return availableLen;
    }
}

// ! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (len <= 0) {
        return "";
    }
    string ret;
    ret.reserve(len);
    if (len >= streamLen) {
        for( )
    }
}
//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (len <= 0) {
        return "";
    }
    string ret;
    ret.reserve(len);
    if (len >= streamLen) {
        while (!stream.empty()) {
            ret += stream.front();
            stream.pop_front();
        }
        streamLen = 0;
    } else {
        string &pop = stream.back();
        while (ret.length() + pop.length() <= len) {
            ret += pop;
            stream.pop_front();
            pop = stream.back();
        }
        ret += pop.substr(0, len - ret.length());
        string tmp = pop.substr(len - ret.length());
        stream.pop_back();
        stream.push_back(tmp);
        streamLen -= len;
    }
    return ret;
}

void ByteStream::end_input() {}

bool ByteStream::input_ended() const { return {}; }

size_t ByteStream::buffer_size() const { return {}; }

bool ByteStream::buffer_empty() const { return {}; }

bool ByteStream::eof() const { return false; }

size_t ByteStream::bytes_written() const { return {}; }

size_t ByteStream::bytes_read() const { return {}; }

size_t ByteStream::remaining_capacity() const { return {}; }
