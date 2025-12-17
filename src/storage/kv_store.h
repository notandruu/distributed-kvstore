#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include <functional>

class KVStore {
public:
    std::optional<std::string> get(const std::string& key) const;
    void put(const std::string& key, const std::string& value);
    bool del(const std::string& key);
    size_t size() const;
    void clear();
    void for_each(const std::function<void(const std::string&, const std::string&)>& fn) const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};
