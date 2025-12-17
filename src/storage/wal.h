#pragma once

#include <string>
#include <cstdint>
#include <mutex>
#include <functional>
#include <vector>

enum class WALEntryType : uint8_t {
    PUT = 0x01,
    DELETE = 0x02,
};

struct WALEntry {
    uint64_t sequence;
    WALEntryType type;
    std::string key;
    std::string value;
};

class WriteAheadLog {
public:
    explicit WriteAheadLog(const std::string& filepath);
    ~WriteAheadLog();

    uint64_t append(WALEntryType type, const std::string& key, const std::string& value);
    uint64_t replay(const std::function<void(const WALEntry&)>& fn);
    uint64_t current_sequence() const;
    std::vector<WALEntry> read_from(uint64_t start_sequence);

private:
    std::string filepath_;
    mutable std::mutex mutex_;
    int fd_ = -1;
    uint64_t next_sequence_ = 1;

    void write_entry(const WALEntry& entry);
    bool read_entry(int fd, WALEntry& entry);
    uint32_t compute_crc(const WALEntry& entry) const;
};
