#include "replication_manager.h"
#include "../common/logger.h"
#include <arpa/inet.h>
#include <cstring>

static uint64_t u64_from_be(uint64_t v) {
    return ((v & 0xFFULL) << 56) | (((v >> 8) & 0xFFULL) << 48) |
           (((v >> 16) & 0xFFULL) << 40) | (((v >> 24) & 0xFFULL) << 32) |
           (((v >> 32) & 0xFFULL) << 24) | (((v >> 40) & 0xFFULL) << 16) |
           (((v >> 48) & 0xFFULL) << 8) | ((v >> 56) & 0xFFULL);
}

ReplicationManager::ReplicationManager(NodeRole role, ReplicationMode mode,
                                        WriteAheadLog& wal, KVStore& store,
                                        uint32_t heartbeat_interval_ms,
                                        uint32_t replication_timeout_ms)
    : role_(role)
    , mode_(mode)
    , wal_(wal)
    , store_(store)
    , heartbeat_interval_ms_(heartbeat_interval_ms)
    , replication_timeout_ms_(replication_timeout_ms) {}

ReplicationManager::~ReplicationManager() {
    stop();
}

void ReplicationManager::add_replica(const NodeAddress& addr) {
    std::lock_guard<std::mutex> lock(replicas_mutex_);
    ReplicaState state;
    state.address = addr;
    state.client = std::make_unique<TcpClient>();
    replicas_.push_back(std::move(state));
}

void ReplicationManager::start() {
    running_ = true;
    if (role_ == NodeRole::PRIMARY) {
        heartbeat_thread_ = std::thread(&ReplicationManager::heartbeat_loop, this);
        async_repl_thread_ = std::thread(&ReplicationManager::async_replication_loop, this);
    }
}

void ReplicationManager::stop() {
    running_ = false;
    queue_cv_.notify_all();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    if (async_repl_thread_.joinable()) async_repl_thread_.join();
}

bool ReplicationManager::replicate_write(OpCode op, const std::string& key,
                                          const std::string& value, uint64_t wal_seq) {
    if (mode_ == ReplicationMode::ASYNC) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        async_queue_.push({op, key, value, wal_seq});
        queue_cv_.notify_one();
        return true;
    }

    std::lock_guard<std::mutex> lock(replicas_mutex_);
    if (replicas_.empty()) return true;

    bool any_success = false;
    for (auto& replica : replicas_) {
        if (replica.connected) {
            if (send_to_replica(replica, op, key, value, wal_seq)) {
                any_success = true;
            }
        }
    }
    return any_success || replicas_.empty();
}

bool ReplicationManager::send_to_replica(ReplicaState& replica, OpCode /*op*/,
                                          const std::string& key, const std::string& value,
                                          uint64_t wal_seq) {
    Request req;
    req.header.magic = PROTOCOL_MAGIC;
    req.header.version = PROTOCOL_VERSION;
    req.header.opcode = OpCode::REPLICATE;
    req.header.request_id = 0;
    req.header.key_length = static_cast<uint16_t>(key.size());
    req.header.value_length = static_cast<uint32_t>(value.size());
    req.header.reserved = 0;
    req.key = key;
    req.value = value;
    req.wal_sequence = wal_seq;

    auto resp = replica.client->send_request(req);
    if (resp.has_value() && resp->header.status == StatusCode::OK) {
        replica.last_acked_sequence = wal_seq;
        return true;
    }

    replica.connected = false;
    return false;
}

void ReplicationManager::try_connect_replica(ReplicaState& replica) {
    if (replica.connected) return;
    if (replica.client->connect(replica.address.host, replica.address.port)) {
        replica.connected = true;
        LOG_INFO("Connected to replica " + replica.address.host + ":" + std::to_string(replica.address.port));
    }
}

void ReplicationManager::heartbeat_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms_));

        std::lock_guard<std::mutex> lock(replicas_mutex_);
        for (size_t i = 0; i < replicas_.size(); ++i) {
            auto& replica = replicas_[i];

            if (!replica.connected) {
                try_connect_replica(replica);
            }

            if (!replica.connected) continue;

            Request req;
            req.header.magic = PROTOCOL_MAGIC;
            req.header.version = PROTOCOL_VERSION;
            req.header.opcode = OpCode::HEARTBEAT;
            req.header.request_id = 0;
            req.header.key_length = 0;
            req.header.value_length = 0;
            req.header.reserved = 0;

            auto resp = replica.client->send_request(req);
            if (!resp.has_value() || resp->header.status != StatusCode::OK) {
                replica.connected = false;
                continue;
            }

            replica.last_heartbeat = std::chrono::steady_clock::now();

            if (resp->value.size() >= 8) {
                uint64_t net_seq;
                std::memcpy(&net_seq, resp->value.data(), 8);
                uint64_t replica_seq = u64_from_be(net_seq);

                uint64_t our_seq = wal_.current_sequence();
                if (replica_seq < our_seq) {
                    catch_up_replica(i);
                }
            }
        }
    }
}

void ReplicationManager::async_replication_loop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !async_queue_.empty() || !running_; });

        if (!running_ && async_queue_.empty()) return;

        std::vector<ReplicationTask> tasks;
        while (!async_queue_.empty()) {
            tasks.push_back(std::move(async_queue_.front()));
            async_queue_.pop();
        }
        lock.unlock();

        std::lock_guard<std::mutex> rlock(replicas_mutex_);
        for (auto& task : tasks) {
            for (auto& replica : replicas_) {
                if (replica.connected) {
                    send_to_replica(replica, task.op, task.key, task.value, task.wal_seq);
                }
            }
        }
    }
}

void ReplicationManager::handle_replicate(const Request& req) {
    WALEntryType type = (req.header.opcode == OpCode::REPLICATE)
        ? ((req.header.value_length > 0 || !req.value.empty())
            ? WALEntryType::PUT : WALEntryType::DELETE)
        : WALEntryType::PUT;

    if (req.wal_sequence > 0) {
        wal_.append(type, req.key, req.value);
        if (type == WALEntryType::PUT) {
            store_.put(req.key, req.value);
        } else {
            store_.del(req.key);
        }
    } else {
        wal_.append(WALEntryType::PUT, req.key, req.value);
        store_.put(req.key, req.value);
    }
}

uint64_t ReplicationManager::handle_heartbeat() {
    return wal_.current_sequence();
}

void ReplicationManager::catch_up_replica(size_t replica_index) {
    auto& replica = replicas_[replica_index];
    auto entries = wal_.read_from(replica.last_acked_sequence + 1);

    for (const auto& entry : entries) {
        OpCode op = (entry.type == WALEntryType::PUT) ? OpCode::PUT : OpCode::DELETE;
        if (!send_to_replica(replica, op, entry.key, entry.value, entry.sequence)) break;
    }
}
