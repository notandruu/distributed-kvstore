#pragma once

#include "types.h"
#include <vector>
#include <cstdint>

namespace protocol {

std::vector<uint8_t> serialize_request_header(const RequestHeader& h);
std::vector<uint8_t> serialize_response_header(const ResponseHeader& h);

bool deserialize_request_header(const uint8_t* data, size_t len, RequestHeader& out);
bool deserialize_response_header(const uint8_t* data, size_t len, ResponseHeader& out);

std::vector<uint8_t> build_request(const Request& req);
std::vector<uint8_t> build_response(const Response& resp);

ssize_t parse_request(const uint8_t* data, size_t len, Request& out);
ssize_t parse_response(const uint8_t* data, size_t len, Response& out);

}
