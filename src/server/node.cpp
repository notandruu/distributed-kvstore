#include "node.h"
#include "../common/logger.h"
#include "../common/protocol.h"
#include "../network/connection.h"
#include <filesystem>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

static uint64_t u64_to_be(uint64_t v) {
    return ((v & 0xFFULL) << 56) | (((v >> 8) & 0xFFULL) << 48) |
           (((v >> 16) & 0xFFULL) << 40) | (((v >> 24) & 0xFFULL) << 32) |
           (((v >> 32) & 0xFFULL) << 24) | (((v >> 40) & 0xFFULL) << 16) |
           (((v >> 48) & 0xFFULL) << 8) | ((v >> 56) & 0xFFULL);
}

static uint64_t u64_from_be(uint64_t v) {
    return u64_to_be(v);
}

Node::Node(const Config& config)
    : config_(config) {
    std::filesystem::create_directories(config_.data_dir);

    wal_ = std::make_unique<WriteAheadLog>(config_.data_dir + "/" + config_.wal_filename);
    pool_ = std::make_unique<ThreadPool>(config_.thread_pool_size);

    repl_manager_ = std::make_unique<ReplicationManager>(
        config_.role,
        config_.replication_mode,
        *wal_,
        store_,
        config_.heartbeat_interval_ms,
        config_.replication_timeout_ms
    );

    if (config_.role == NodeRole::PRIMARY) {
        for (const auto& addr : config_.replicas) {
            repl_manager_->add_replica(addr);
        }
    }

    server_ = std::make_unique<TcpServer>(config_.host, config_.port, *pool_);
    server_->set_handler([this](int fd) { handle_connection(fd); });
}

Node::~Node() {
    stop();
}

void Node::start() {
    LOG_INFO("Starting node " + config_.node_id + " role=" +
             (config_.role == NodeRole::PRIMARY ? "primary" : "replica") +
             " at " + config_.host + ":" + std::to_string(config_.port));

    recover_from_wal();
    repl_manager_->start();
    server_->start();
}

void Node::stop() {
    server_->stop();
    repl_manager_->stop();
    pool_->shutdown();
}

void Node::recover_from_wal() {
    size_t count = 0;
    uint64_t latest_seq = wal_->replay([&](const WALEntry& entry) {
        if (entry.type == WALEntryType::PUT) {
            store_.put(entry.key, entry.value);
        } else if (entry.type == WALEntryType::DELETE) {
            store_.del(entry.key);
        }
        ++count;
    });

    LOG_INFO("WAL recovery: " + std::to_string(count) + " entries, latest_seq=" + std::to_string(latest_seq));
}

void Node::handle_connection(int client_fd) {
    Connection conn(client_fd);

    while (conn.is_open()) {
        uint8_t header_buf[REQUEST_HEADER_SIZE];
        if (!conn.read_exact(header_buf, REQUEST_HEADER_SIZE)) break;

        RequestHeader hdr;
        if (!protocol::deserialize_request_header(header_buf, REQUEST_HEADER_SIZE, hdr)) break;

        Request req;
        req.header = hdr;

        if (hdr.key_length > 0) {
            req.key.resize(hdr.key_length);
            if (!conn.read_exact(reinterpret_cast<uint8_t*>(req.key.data()), hdr.key_length)) break;
        }

        if (hdr.value_length > 0) {
            req.value.resize(hdr.value_length);
            if (!conn.read_exact(reinterpret_cast<uint8_t*>(req.value.data()), hdr.value_length)) break;
        }

        if (hdr.opcode == OpCode::REPLICATE) {
            uint8_t seq_buf[8];
            if (!conn.read_exact(seq_buf, 8)) break;
            uint64_t net_seq;
            std::memcpy(&net_seq, seq_buf, 8);
            req.wal_sequence = u64_from_be(net_seq);
        }

        Response resp = process_request(req);
        auto resp_bytes = protocol::build_response(resp);
        if (!conn.write_all(resp_bytes)) break;
    }
}

Response Node::process_request(const Request& req) {
    Response resp;
    resp.header.magic = PROTOCOL_MAGIC;
    resp.header.version = PROTOCOL_VERSION;
    resp.header.request_id = req.header.request_id;
    resp.header.reserved = 0;

    switch (req.header.opcode) {
        case OpCode::GET: {
            auto val = store_.get(req.key);
            if (val.has_value()) {
                resp.header.status = StatusCode::OK;
                resp.value = *val;
            } else {
                resp.header.status = StatusCode::NOT_FOUND;
            }
            break;
        }

        case OpCode::PUT: {
            if (config_.role == NodeRole::REPLICA) {
                resp.header.status = StatusCode::REDIRECT;
                resp.value = config_.primary_address.host + ":" + std::to_string(config_.primary_address.port);
            } else {
                uint64_t wal_seq = wal_->append(WALEntryType::PUT, req.key, req.value);
                store_.put(req.key, req.value);
                repl_manager_->replicate_write(OpCode::PUT, req.key, req.value, wal_seq);
                resp.header.status = StatusCode::OK;
            }
            break;
        }

        case OpCode::DELETE: {
            if (config_.role == NodeRole::REPLICA) {
                resp.header.status = StatusCode::REDIRECT;
                resp.value = config_.primary_address.host + ":" + std::to_string(config_.primary_address.port);
            } else {
                uint64_t wal_seq = wal_->append(WALEntryType::DELETE, req.key, "");
                store_.del(req.key);
                repl_manager_->replicate_write(OpCode::DELETE, req.key, "", wal_seq);
                resp.header.status = StatusCode::OK;
            }
            break;
        }

        case OpCode::REPLICATE: {
            repl_manager_->handle_replicate(req);
            resp.header.status = StatusCode::OK;
            break;
        }

        case OpCode::HEARTBEAT: {
            uint64_t seq = repl_manager_->handle_heartbeat();
            resp.header.status = StatusCode::OK;
            uint64_t net_seq = u64_to_be(seq);
            resp.value.resize(8);
            std::memcpy(resp.value.data(), &net_seq, 8);
            break;
        }

        default:
            resp.header.status = StatusCode::ERROR;
            break;
    }

    resp.header.value_length = static_cast<uint32_t>(resp.value.size());
    return resp;
}
