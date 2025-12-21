#pragma once

#include "../common/types.h"
#include "../storage/kv_store.h"
#include "../storage/wal.h"
#include "../network/tcp_server.h"
#include "../threading/thread_pool.h"
#include "../replication/replication_manager.h"
#include <memory>

class Node {
public:
    explicit Node(const Config& config);
    ~Node();

    void start();
    void stop();

private:
    Config config_;
    KVStore store_;
    std::unique_ptr<WriteAheadLog> wal_;
    std::unique_ptr<ThreadPool> pool_;
    std::unique_ptr<TcpServer> server_;
    std::unique_ptr<ReplicationManager> repl_manager_;

    void handle_connection(int client_fd);
    Response process_request(const Request& req);
    void recover_from_wal();
};
