#ifndef SPONGE_LIBSPONGE_BYTE_STREAM_HH
#define SPONGE_LIBSPONGE_BYTE_STREAM_HH

#include <deque>
#include <string>

class ByteStream {
  private:
    bool _error{};

    std::deque<char> _buffer = {};
    uint64_t _capacity = 0;

    bool _input_ended_flag = false;

    uint64_t _write_count = 0;
    uint64_t _read_count = 0;

  public:
    ByteStream(const uint64_t capacity);

    uint64_t write(const std::string &data);

    uint64_t remaining_capacity() const;

    void end_input();

    void set_error() { _error = true; }

    std::string peek_output(const uint64_t len) const;

    void pop_output(const uint64_t len);

    std::string read(const uint64_t len);

    bool input_ended() const;

    bool error() const { return _error; }

    uint64_t buffer_size() const;

    bool buffer_empty() const;

    bool eof() const;

    uint64_t bytes_written() const;

    uint64_t bytes_read() const;
};

#endif  // SPONGE_LIBSPONGE_BYTE_STREAM_HH
