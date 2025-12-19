#pragma once

#include "../common/types.h"
#include "connection.h"
#include <string>
#include <memory>
#include <optional>

class TcpClient {
public:
    TcpClient();
    ~TcpClient();

    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    bool is_connected() const;
    std::optional<Response> send_request(const Request& req);

private:
    std::unique_ptr<Connection> conn_;
    uint32_t next_request_id_ = 1;
};
