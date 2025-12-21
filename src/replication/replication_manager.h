#pragma once

#include "../common/types.h"
#include "../network/tcp_client.h"
#include "../storage/wal.h"
#include "../storage/kv_store.h"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <chrono>

struct ReplicaState {
    NodeAddress address;
    std::unique_ptr<TcpClient> client;
    uint64_t last_acked_sequence = 0;
    bool connected = false;
    std::chrono::steady_clock::time_point last_heartbeat;
};

class ReplicationManager {
public:
    ReplicationManager(NodeRole role, ReplicationMode mode,
                       WriteAheadLog& wal, KVStore& store,
                       uint32_t heartbeat_interval_ms = 1000,
                       uint32_t replication_timeout_ms = 5000);
    ~ReplicationManager();

    void add_replica(const NodeAddress& addr);
    bool replicate_write(OpCode op, const std::string& key,
                         const std::string& value, uint64_t wal_seq);
    void start();
    void stop();

    void handle_replicate(const Request& req);
    uint64_t handle_heartbeat();
    void catch_up_replica(size_t replica_index);

private:
    NodeRole role_;
    ReplicationMode mode_;
    WriteAheadLog& wal_;
    KVStore& store_;
    uint32_t heartbeat_interval_ms_;
    uint32_t replication_timeout_ms_;

    std::vector<ReplicaState> replicas_;
    std::mutex replicas_mutex_;

    struct ReplicationTask {
        OpCode op;
        std::string key;
        std::string value;
        uint64_t wal_seq;
    };
    std::queue<ReplicationTask> async_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::thread heartbeat_thread_;
    std::thread async_repl_thread_;
    std::atomic<bool> running_{false};

    void heartbeat_loop();
    void async_replication_loop();
    bool send_to_replica(ReplicaState& replica, OpCode op,
                         const std::string& key, const std::string& value,
                         uint64_t wal_seq);
    void try_connect_replica(ReplicaState& replica);
};
