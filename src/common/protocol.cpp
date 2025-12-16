#include "protocol.h"
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

namespace protocol {

std::vector<uint8_t> serialize_request_header(const RequestHeader& h) {
    std::vector<uint8_t> buf(REQUEST_HEADER_SIZE);
    size_t off = 0;

    uint32_t magic_n = htonl(h.magic);
    std::memcpy(buf.data() + off, &magic_n, 4); off += 4;

    buf[off++] = h.version;
    buf[off++] = static_cast<uint8_t>(h.opcode);

    uint32_t req_id_n = htonl(h.request_id);
    std::memcpy(buf.data() + off, &req_id_n, 4); off += 4;

    uint16_t klen_n = htons(h.key_length);
    std::memcpy(buf.data() + off, &klen_n, 2); off += 2;

    uint32_t vlen_n = htonl(h.value_length);
    std::memcpy(buf.data() + off, &vlen_n, 4); off += 4;

    uint32_t reserved_n = htonl(h.reserved);
    std::memcpy(buf.data() + off, &reserved_n, 4); off += 4;

    return buf;
}

std::vector<uint8_t> serialize_response_header(const ResponseHeader& h) {
    std::vector<uint8_t> buf(RESPONSE_HEADER_SIZE);
    size_t off = 0;

    uint32_t magic_n = htonl(h.magic);
    std::memcpy(buf.data() + off, &magic_n, 4); off += 4;

    buf[off++] = h.version;
    buf[off++] = static_cast<uint8_t>(h.status);

    uint32_t req_id_n = htonl(h.request_id);
    std::memcpy(buf.data() + off, &req_id_n, 4); off += 4;

    uint32_t vlen_n = htonl(h.value_length);
    std::memcpy(buf.data() + off, &vlen_n, 4); off += 4;

    uint16_t reserved_n = htons(h.reserved);
    std::memcpy(buf.data() + off, &reserved_n, 2); off += 2;

    return buf;
}

bool deserialize_request_header(const uint8_t* data, size_t len, RequestHeader& out) {
    if (len < REQUEST_HEADER_SIZE) return false;
    size_t off = 0;

    uint32_t magic_n;
    std::memcpy(&magic_n, data + off, 4); off += 4;
    out.magic = ntohl(magic_n);

    out.version = data[off++];
    out.opcode = static_cast<OpCode>(data[off++]);

    uint32_t req_id_n;
    std::memcpy(&req_id_n, data + off, 4); off += 4;
    out.request_id = ntohl(req_id_n);

    uint16_t klen_n;
    std::memcpy(&klen_n, data + off, 2); off += 2;
    out.key_length = ntohs(klen_n);

    uint32_t vlen_n;
    std::memcpy(&vlen_n, data + off, 4); off += 4;
    out.value_length = ntohl(vlen_n);

    uint32_t reserved_n;
    std::memcpy(&reserved_n, data + off, 4); off += 4;
    out.reserved = ntohl(reserved_n);

    return true;
}

bool deserialize_response_header(const uint8_t* data, size_t len, ResponseHeader& out) {
    if (len < RESPONSE_HEADER_SIZE) return false;
    size_t off = 0;

    uint32_t magic_n;
    std::memcpy(&magic_n, data + off, 4); off += 4;
    out.magic = ntohl(magic_n);

    out.version = data[off++];
    out.status = static_cast<StatusCode>(data[off++]);

    uint32_t req_id_n;
    std::memcpy(&req_id_n, data + off, 4); off += 4;
    out.request_id = ntohl(req_id_n);

    uint32_t vlen_n;
    std::memcpy(&vlen_n, data + off, 4); off += 4;
    out.value_length = ntohl(vlen_n);

    uint16_t reserved_n;
    std::memcpy(&reserved_n, data + off, 2); off += 2;
    out.reserved = ntohs(reserved_n);

    return true;
}

std::vector<uint8_t> build_request(const Request& req) {
    std::vector<uint8_t> buf = serialize_request_header(req.header);
    buf.insert(buf.end(), req.key.begin(), req.key.end());
    buf.insert(buf.end(), req.value.begin(), req.value.end());

    if (req.header.opcode == OpCode::REPLICATE) {
        uint64_t seq = req.wal_sequence;
        uint32_t high = htonl(static_cast<uint32_t>(seq >> 32));
        uint32_t low  = htonl(static_cast<uint32_t>(seq & 0xFFFFFFFF));
        const uint8_t* hp = reinterpret_cast<const uint8_t*>(&high);
        const uint8_t* lp = reinterpret_cast<const uint8_t*>(&low);
        buf.insert(buf.end(), hp, hp + 4);
        buf.insert(buf.end(), lp, lp + 4);
    }

    return buf;
}

std::vector<uint8_t> build_response(const Response& resp) {
    std::vector<uint8_t> buf = serialize_response_header(resp.header);
    buf.insert(buf.end(), resp.value.begin(), resp.value.end());
    return buf;
}

ssize_t parse_request(const uint8_t* data, size_t len, Request& out) {
    if (len < REQUEST_HEADER_SIZE) return 0;

    RequestHeader hdr;
    if (!deserialize_request_header(data, len, hdr)) return 0;

    if (hdr.magic != PROTOCOL_MAGIC) return -1;
    if (hdr.version != PROTOCOL_VERSION) return -1;

    size_t body_needed = hdr.key_length + hdr.value_length;
    bool is_replicate = (hdr.opcode == OpCode::REPLICATE);
    if (is_replicate) body_needed += 8;

    if (len < REQUEST_HEADER_SIZE + body_needed) return 0;

    out.header = hdr;
    size_t off = REQUEST_HEADER_SIZE;

    out.key.assign(reinterpret_cast<const char*>(data + off), hdr.key_length);
    off += hdr.key_length;

    out.value.assign(reinterpret_cast<const char*>(data + off), hdr.value_length);
    off += hdr.value_length;

    if (is_replicate) {
        uint32_t high_n, low_n;
        std::memcpy(&high_n, data + off, 4); off += 4;
        std::memcpy(&low_n, data + off, 4); off += 4;
        uint64_t high = ntohl(high_n);
        uint64_t low  = ntohl(low_n);
        out.wal_sequence = (high << 32) | low;
    } else {
        out.wal_sequence = 0;
    }

    return static_cast<ssize_t>(off);
}

ssize_t parse_response(const uint8_t* data, size_t len, Response& out) {
    if (len < RESPONSE_HEADER_SIZE) return 0;

    ResponseHeader hdr;
    if (!deserialize_response_header(data, len, hdr)) return 0;

    if (hdr.magic != PROTOCOL_MAGIC) return -1;
    if (hdr.version != PROTOCOL_VERSION) return -1;

    if (len < RESPONSE_HEADER_SIZE + hdr.value_length) return 0;

    out.header = hdr;
    size_t off = RESPONSE_HEADER_SIZE;

    out.value.assign(reinterpret_cast<const char*>(data + off), hdr.value_length);
    off += hdr.value_length;

    return static_cast<ssize_t>(off);
}

}
