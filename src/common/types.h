#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <queue>
#include <chrono>

constexpr uint32_t PROTOCOL_MAGIC = 0x4B565331;
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint16_t MAX_KEY_SIZE = 256;
constexpr uint32_t MAX_VALUE_SIZE = 1048576;

constexpr size_t REQUEST_HEADER_SIZE = 20;
constexpr size_t RESPONSE_HEADER_SIZE = 16;

enum class OpCode : uint8_t {
    GET = 0x01,
    PUT = 0x02,
    DELETE = 0x03,
    REPLICATE = 0x04,
    HEARTBEAT = 0x05,
};

enum class StatusCode : uint8_t {
    OK = 0x00,
    NOT_FOUND = 0x01,
    ERROR = 0x02,
    REDIRECT = 0x03,
};

enum class NodeRole : uint8_t {
    PRIMARY = 0,
    REPLICA = 1,
};

enum class ReplicationMode : uint8_t {
    SYNC = 0,
    ASYNC = 1,
};

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
};

struct RequestHeader {
    uint32_t magic;
    uint8_t version;
    OpCode opcode;
    uint32_t request_id;
    uint16_t key_length;
    uint32_t value_length;
    uint32_t reserved;
};

struct ResponseHeader {
    uint32_t magic;
    uint8_t version;
    StatusCode status;
    uint32_t request_id;
    uint32_t value_length;
    uint16_t reserved;
};

struct Request {
    RequestHeader header;
    std::string key;
    std::string value;
    uint64_t wal_sequence = 0;
};

struct Response {
    ResponseHeader header;
    std::string value;
};

struct NodeAddress {
    std::string host;
    uint16_t port = 0;
};

struct Config {
    std::string node_id;
    std::string host = "127.0.0.1";
    uint16_t port = 7000;
    NodeRole role = NodeRole::REPLICA;

    size_t thread_pool_size = 4;

    std::string data_dir = "./data";
    std::string wal_filename = "wal.bin";

    ReplicationMode replication_mode = ReplicationMode::ASYNC;
    std::vector<NodeAddress> replicas;
    NodeAddress primary_address;
    uint32_t heartbeat_interval_ms = 1000;
    uint32_t replication_timeout_ms = 5000;

    LogLevel log_level = LogLevel::INFO;
};
