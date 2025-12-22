#pragma once

#include "../common/types.h"
#include "../network/tcp_client.h"
#include <string>
#include <optional>

class KVClient {
public:
    KVClient();
    ~KVClient();

    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    std::optional<std::string> get(const std::string& key);
    bool put(const std::string& key, const std::string& value);
    bool del(const std::string& key);

private:
    TcpClient client_;
};
