#include "kv_client.h"

KVClient::KVClient() = default;
KVClient::~KVClient() = default;

bool KVClient::connect(const std::string& host, uint16_t port) {
    return client_.connect(host, port);
}

void KVClient::disconnect() {
    client_.disconnect();
}

std::optional<std::string> KVClient::get(const std::string& key) {
    Request req;
    req.header.magic = PROTOCOL_MAGIC;
    req.header.version = PROTOCOL_VERSION;
    req.header.opcode = OpCode::GET;
    req.header.request_id = 0;
    req.header.key_length = static_cast<uint16_t>(key.size());
    req.header.value_length = 0;
    req.header.reserved = 0;
    req.key = key;

    auto resp = client_.send_request(req);
    if (!resp.has_value()) return std::nullopt;
    if (resp->header.status == StatusCode::NOT_FOUND) return std::nullopt;
    if (resp->header.status == StatusCode::OK && resp->header.value_length > 0) {
        return resp->value;
    }
    return std::nullopt;
}

bool KVClient::put(const std::string& key, const std::string& value) {
    Request req;
    req.header.magic = PROTOCOL_MAGIC;
    req.header.version = PROTOCOL_VERSION;
    req.header.opcode = OpCode::PUT;
    req.header.request_id = 0;
    req.header.key_length = static_cast<uint16_t>(key.size());
    req.header.value_length = static_cast<uint32_t>(value.size());
    req.header.reserved = 0;
    req.key = key;
    req.value = value;

    auto resp = client_.send_request(req);
    return resp.has_value() && resp->header.status == StatusCode::OK;
}

bool KVClient::del(const std::string& key) {
    Request req;
    req.header.magic = PROTOCOL_MAGIC;
    req.header.version = PROTOCOL_VERSION;
    req.header.opcode = OpCode::DELETE;
    req.header.request_id = 0;
    req.header.key_length = static_cast<uint16_t>(key.size());
    req.header.value_length = 0;
    req.header.reserved = 0;
    req.key = key;

    auto resp = client_.send_request(req);
    return resp.has_value() && resp->header.status == StatusCode::OK;
}
