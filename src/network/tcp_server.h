#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <atomic>

class ThreadPool;

class TcpServer {
public:
    using ConnectionHandler = std::function<void(int client_fd)>;

    TcpServer(const std::string& host, uint16_t port, ThreadPool& pool);
    ~TcpServer();

    void set_handler(ConnectionHandler handler);
    void start();
    void stop();

private:
    std::string host_;
    uint16_t port_;
    ThreadPool& pool_;
    ConnectionHandler handler_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};

    void setup_socket();
};
