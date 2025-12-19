#include "tcp_server.h"
#include "../threading/thread_pool.h"
#include "../common/logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <stdexcept>

TcpServer::TcpServer(const std::string& host, uint16_t port, ThreadPool& pool)
    : host_(host), port_(port), pool_(pool) {}

TcpServer::~TcpServer() {
    stop();
}

void TcpServer::set_handler(ConnectionHandler handler) {
    handler_ = std::move(handler);
}

void TcpServer::setup_socket() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw std::runtime_error("socket() failed");
    }

    int opt = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        ::close(fd);
        throw std::runtime_error("setsockopt() failed");
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("inet_pton() failed");
    }

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(fd);
        throw std::runtime_error("bind() failed");
    }

    if (::listen(fd, 128) == -1) {
        ::close(fd);
        throw std::runtime_error("listen() failed");
    }

    listen_fd_ = fd;
    LOG_INFO("TCP server listening on " + host_ + ":" + std::to_string(port_));
}

void TcpServer::start() {
    setup_socket();
    running_ = true;

    while (running_) {
        struct pollfd pfd{};
        pfd.fd = listen_fd_;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 100);
        if (ret <= 0) continue;

        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_fd == -1) continue;

        pool_.submit([this, client_fd]() {
            handler_(client_fd);
        });
    }
}

void TcpServer::stop() {
    running_ = false;
    if (listen_fd_ != -1) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}
