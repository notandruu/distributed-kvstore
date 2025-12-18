#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    bool read_exact(uint8_t* buf, size_t n);
    bool write_all(const uint8_t* buf, size_t n);
    bool write_all(const std::vector<uint8_t>& buf);
    void close();
    int fd() const;
    bool is_open() const;
    std::string peer_address() const;

private:
    int fd_ = -1;
};
