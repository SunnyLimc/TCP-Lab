#ifndef SPONGE_LIBSPONGE_BYTE_STREAM_HH
#define SPONGE_LIBSPONGE_BYTE_STREAM_HH

#include <deque>
#include <string>

class ByteStream {
  private:
    bool _error{};

    std::deque<char> _buffer = {};
    size_t _capacity = 0;

    bool _input_ended_flog = false;

    size_t _write_count = 0;
    size_t _read_count = 0;

  public:
    ByteStream(const size_t capacity);

    size_t write(const std::string &data);

    size_t remaining_capacity() const;

    void end_input();

    void set_error() { _error = true; }

    std::string peek_output(const size_t len) const;

    void pop_output(const size_t len);

    std::string read(const size_t len);

    bool input_ended() const;

    bool error() const { return _error; }

    size_t buffer_size() const;

    bool buffer_empty() const;

    bool eof() const;

    size_t bytes_written() const;

    size_t bytes_read() const;
};

#endif  // SPONGE_LIBSPONGE_BYTE_STREAM_HH
