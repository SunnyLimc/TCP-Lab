#ifndef SPONGE_LIBSPONGE_BYTE_STREAM_HH
#define SPONGE_LIBSPONGE_BYTE_STREAM_HH

#include <deque>
#include <string>

class ByteStream {
  private:
    bool _error{};

    std::deque<char> _buffer{};
    size_t _capacity = 0;

    bool _input_ended_flag = false;

    size_t _write_count = 0;
    size_t _read_count = 0;

  public:
    ByteStream(const size_t capacity);

    size_t write(const std::string &data);

    void set_error() { _error = true; }

    std::string peek_output(const size_t len) const;

    void pop_output(const size_t len);

    bool error() const { return _error; }

    std::string read(const size_t len) {
        const auto ret = peek_output(len);
        pop_output(len);
        return ret;
    }

    void end_input() { _input_ended_flag = true; }

    bool input_ended() const { return _input_ended_flag; }

    size_t buffer_size() const { return _buffer.size(); }

    bool buffer_empty() const { return _buffer.size() == 0; }

    bool eof() const { return input_ended() && buffer_empty(); }

    size_t bytes_written() const { return _write_count; }

    size_t bytes_read() const { return _read_count; }

    size_t remaining_capacity() const { return _capacity - _buffer.size(); }
};

#endif  // SPONGE_LIBSPONGE_BYTE_STREAM_HH
