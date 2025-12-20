#include "config.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::pair<std::string, std::string> split_kv(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return {"", ""};
    std::string key = trim(line.substr(0, colon));
    std::string val = trim(line.substr(colon + 1));
    return {key, val};
}

Config parse_config(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filepath);
    }

    Config config;
    std::string line;

    enum class Section { NONE, REPLICAS, PRIMARY };
    Section section = Section::NONE;
    bool in_replica_entry = false;
    NodeAddress current_replica;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        bool is_indented = !line.empty() && (line[0] == ' ' || line[0] == '\t');

        if (section == Section::REPLICAS && is_indented) {
            if (trimmed.substr(0, 2) == "- ") {
                if (in_replica_entry) {
                    config.replicas.push_back(current_replica);
                    current_replica = NodeAddress{};
                }
                in_replica_entry = true;
                std::string rest = trim(trimmed.substr(2));
                auto [k, v] = split_kv(rest);
                if (k == "host") current_replica.host = v;
                else if (k == "port") current_replica.port = static_cast<uint16_t>(std::stoi(v));
            } else {
                auto [k, v] = split_kv(trimmed);
                if (k == "host") current_replica.host = v;
                else if (k == "port") current_replica.port = static_cast<uint16_t>(std::stoi(v));
            }
            continue;
        }

        if (section == Section::PRIMARY && is_indented) {
            auto [k, v] = split_kv(trimmed);
            if (k == "host") config.primary_address.host = v;
            else if (k == "port") config.primary_address.port = static_cast<uint16_t>(std::stoi(v));
            continue;
        }

        if (section == Section::REPLICAS && !is_indented) {
            if (in_replica_entry) {
                config.replicas.push_back(current_replica);
                current_replica = NodeAddress{};
                in_replica_entry = false;
            }
            section = Section::NONE;
        } else if (section == Section::PRIMARY && !is_indented) {
            section = Section::NONE;
        }

        if (trimmed == "replicas:") {
            section = Section::REPLICAS;
            in_replica_entry = false;
            continue;
        }

        if (trimmed == "primary:") {
            section = Section::PRIMARY;
            continue;
        }

        auto [k, v] = split_kv(trimmed);
        if (k.empty()) continue;

        if (k == "node_id") config.node_id = v;
        else if (k == "host") config.host = v;
        else if (k == "port") config.port = static_cast<uint16_t>(std::stoi(v));
        else if (k == "role") config.role = (v == "primary") ? NodeRole::PRIMARY : NodeRole::REPLICA;
        else if (k == "thread_pool_size") config.thread_pool_size = static_cast<size_t>(std::stoi(v));
        else if (k == "data_dir") config.data_dir = v;
        else if (k == "wal_filename") config.wal_filename = v;
        else if (k == "replication_mode") config.replication_mode = (v == "sync") ? ReplicationMode::SYNC : ReplicationMode::ASYNC;
        else if (k == "heartbeat_interval_ms") config.heartbeat_interval_ms = static_cast<uint32_t>(std::stoi(v));
        else if (k == "replication_timeout_ms") config.replication_timeout_ms = static_cast<uint32_t>(std::stoi(v));
        else if (k == "log_level") {
            if (v == "debug") config.log_level = LogLevel::DEBUG;
            else if (v == "info") config.log_level = LogLevel::INFO;
            else if (v == "warn") config.log_level = LogLevel::WARN;
            else if (v == "error") config.log_level = LogLevel::ERROR;
        }
    }

    if (section == Section::REPLICAS && in_replica_entry) {
        config.replicas.push_back(current_replica);
    }

    return config;
}
