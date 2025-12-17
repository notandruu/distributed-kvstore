#include "kv_store.h"

std::optional<std::string> KVStore::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

void KVStore::put(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    data_.insert_or_assign(key, value);
}

bool KVStore::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return data_.erase(key) > 0;
}

size_t KVStore::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return data_.size();
}

void KVStore::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    data_.clear();
}

void KVStore::for_each(const std::function<void(const std::string&, const std::string&)>& fn) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& [k, v] : data_) {
        fn(k, v);
    }
}
