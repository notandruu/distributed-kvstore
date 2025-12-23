#pragma once

#include "stats.h"
#include "../common/types.h"
#include <string>
#include <cstdint>
#include <atomic>

struct BenchConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 7001;
    size_t num_threads = 4;
    size_t total_operations = 100000;
    size_t key_range = 10000;
    double read_ratio = 0.8;
    size_t value_size = 64;
};

class LoadGenerator {
public:
    explicit LoadGenerator(const BenchConfig& config);

    Stats run();

private:
    BenchConfig config_;
    std::atomic<size_t> ops_completed_{0};

    void worker(size_t ops_per_thread, Stats& thread_stats);
    std::string random_key();
    std::string random_value();
};
