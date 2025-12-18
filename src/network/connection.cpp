#include "connection.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() {
    close();
}

Connection::Connection(Connection&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool Connection::read_exact(uint8_t* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t r = ::read(fd_, buf + total, n - total);
        if (r == 0) return false;
        if (r == -1) {
            if (errno == EINTR) continue;
            return false;
        }
        total += static_cast<size_t>(r);
    }
    return true;
}

bool Connection::write_all(const uint8_t* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t w = ::write(fd_, buf + total, n - total);
        if (w == -1) {
            if (errno == EINTR) continue;
            return false;
        }
        total += static_cast<size_t>(w);
    }
    return true;
}

bool Connection::write_all(const std::vector<uint8_t>& buf) {
    return write_all(buf.data(), buf.size());
}

void Connection::close() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

int Connection::fd() const {
    return fd_;
}

bool Connection::is_open() const {
    return fd_ != -1;
}

std::string Connection::peer_address() const {
    struct sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) == -1) {
        return "";
    }
    char buf[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf))) {
        return "";
    }
    return std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
}
