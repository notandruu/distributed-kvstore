#include "tcp_client.h"
#include "../common/protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

TcpClient::TcpClient() = default;
TcpClient::~TcpClient() = default;

bool TcpClient::connect(const std::string& host, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        return false;
    }

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(fd);
        return false;
    }

    conn_ = std::make_unique<Connection>(fd);
    return true;
}

void TcpClient::disconnect() {
    conn_.reset();
}

bool TcpClient::is_connected() const {
    return conn_ && conn_->is_open();
}

std::optional<Response> TcpClient::send_request(const Request& req) {
    if (!is_connected()) return std::nullopt;

    Request r = req;
    r.header.request_id = next_request_id_++;

    std::vector<uint8_t> bytes = protocol::build_request(r);
    if (!conn_->write_all(bytes)) return std::nullopt;

    uint8_t header_buf[RESPONSE_HEADER_SIZE];
    if (!conn_->read_exact(header_buf, RESPONSE_HEADER_SIZE)) return std::nullopt;

    ResponseHeader resp_header{};
    if (!protocol::deserialize_response_header(header_buf, RESPONSE_HEADER_SIZE, resp_header)) {
        return std::nullopt;
    }

    Response resp{};
    resp.header = resp_header;

    if (resp_header.value_length > 0) {
        std::vector<uint8_t> val_buf(resp_header.value_length);
        if (!conn_->read_exact(val_buf.data(), resp_header.value_length)) return std::nullopt;
        resp.value.assign(reinterpret_cast<char*>(val_buf.data()), resp_header.value_length);
    }

    return resp;
}
