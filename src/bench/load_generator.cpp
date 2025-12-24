#include "load_generator.h"
#include "../client/kv_client.h"
#include "../common/logger.h"
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <string>

LoadGenerator::LoadGenerator(const BenchConfig& config) : config_(config) {}

Stats LoadGenerator::run() {
    size_t base_ops = config_.total_operations / config_.num_threads;
    size_t remainder = config_.total_operations % config_.num_threads;

    std::vector<std::thread> threads;
    std::vector<Stats> per_thread_stats(config_.num_threads);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < config_.num_threads; ++i) {
        size_t ops = base_ops + (i == config_.num_threads - 1 ? remainder : 0);
        threads.emplace_back([this, ops, &per_thread_stats, i]() {
            worker(ops, per_thread_stats[i]);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    Stats merged;
    for (auto& s : per_thread_stats) {
        merged.merge(s);
    }

    std::cout << merged.report(total_duration);
    return merged;
}

void LoadGenerator::worker(size_t ops_per_thread, Stats& thread_stats) {
    KVClient client;
    if (!client.connect(config_.host, config_.port)) {
        LOG_ERROR("Failed to connect to " + config_.host + ":" + std::to_string(config_.port));
        return;
    }

    std::mt19937_64 rng(
        std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    std::uniform_real_distribution<double> real_dist(0.0, 1.0);

    for (size_t i = 0; i < ops_per_thread; ++i) {
        bool is_get = real_dist(rng) < config_.read_ratio;
        std::string key = random_key();

        auto op_start = std::chrono::high_resolution_clock::now();

        if (is_get) {
            client.get(key);
        } else {
            client.put(key, random_value());
        }

        auto op_end = std::chrono::high_resolution_clock::now();
        thread_stats.record(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start));
        ++ops_completed_;
    }
}

std::string LoadGenerator::random_key() {
    thread_local std::mt19937_64 rng(
        std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    std::uniform_int_distribution<size_t> dist(0, config_.key_range - 1);
    return "key:" + std::to_string(dist(rng));
}

std::string LoadGenerator::random_value() {
    thread_local std::mt19937_64 rng(
        std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    static constexpr char alphanum[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<size_t> dist(0, sizeof(alphanum) - 2);
    std::string val;
    val.reserve(config_.value_size);
    for (size_t i = 0; i < config_.value_size; ++i) {
        val += alphanum[dist(rng)];
    }
    return val;
}
