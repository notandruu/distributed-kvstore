#pragma once

#include <cstdint>
#include <string>

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
