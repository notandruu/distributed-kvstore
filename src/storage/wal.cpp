#include "wal.h"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <array>

static std::array<uint32_t, 256> crc32_table = []() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
        table[i] = crc;
    }
    return table;
}();

static uint32_t crc32_compute(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFFu;
}

static std::vector<uint8_t> serialize_entry(const WALEntry& entry) {
    std::vector<uint8_t> buf;
    buf.reserve(8 + 1 + 2 + 4 + entry.key.size() + entry.value.size());

    uint32_t seq_high = htonl(static_cast<uint32_t>(entry.sequence >> 32));
    uint32_t seq_low  = htonl(static_cast<uint32_t>(entry.sequence & 0xFFFFFFFF));
    const uint8_t* sh = reinterpret_cast<const uint8_t*>(&seq_high);
    const uint8_t* sl = reinterpret_cast<const uint8_t*>(&seq_low);
    buf.insert(buf.end(), sh, sh + 4);
    buf.insert(buf.end(), sl, sl + 4);

    buf.push_back(static_cast<uint8_t>(entry.type));

    uint16_t klen_n = htons(static_cast<uint16_t>(entry.key.size()));
    const uint8_t* kp = reinterpret_cast<const uint8_t*>(&klen_n);
    buf.insert(buf.end(), kp, kp + 2);

    uint32_t vlen_n = htonl(static_cast<uint32_t>(entry.value.size()));
    const uint8_t* vp = reinterpret_cast<const uint8_t*>(&vlen_n);
    buf.insert(buf.end(), vp, vp + 4);

    buf.insert(buf.end(), entry.key.begin(), entry.key.end());
    buf.insert(buf.end(), entry.value.begin(), entry.value.end());

    return buf;
}

WriteAheadLog::WriteAheadLog(const std::string& filepath)
    : filepath_(filepath) {
    fd_ = ::open(filepath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0) {
        throw std::runtime_error("WAL: failed to open " + filepath);
    }
}

WriteAheadLog::~WriteAheadLog() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

uint32_t WriteAheadLog::compute_crc(const WALEntry& entry) const {
    std::vector<uint8_t> buf = serialize_entry(entry);
    return crc32_compute(buf.data(), buf.size());
}

uint64_t WriteAheadLog::append(WALEntryType type, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    WALEntry entry;
    entry.sequence = next_sequence_++;
    entry.type     = type;
    entry.key      = key;
    entry.value    = value;

    std::vector<uint8_t> buf = serialize_entry(entry);

    uint32_t crc = crc32_compute(buf.data(), buf.size());
    uint32_t crc_n = htonl(crc);
    const uint8_t* cp = reinterpret_cast<const uint8_t*>(&crc_n);
    buf.insert(buf.end(), cp, cp + 4);

    ::write(fd_, buf.data(), buf.size());
    ::fsync(fd_);

    return entry.sequence;
}

bool WriteAheadLog::read_entry(int fd, WALEntry& entry) {
    constexpr size_t FIXED_HDR = 8 + 1 + 2 + 4;
    uint8_t hdr[FIXED_HDR];

    ssize_t n = ::read(fd, hdr, FIXED_HDR);
    if (n == 0) return false;
    if (n != static_cast<ssize_t>(FIXED_HDR)) return false;

    size_t off = 0;
    uint32_t seq_high_n, seq_low_n;
    std::memcpy(&seq_high_n, hdr + off, 4); off += 4;
    std::memcpy(&seq_low_n,  hdr + off, 4); off += 4;
    uint64_t seq = (static_cast<uint64_t>(ntohl(seq_high_n)) << 32) |
                    static_cast<uint64_t>(ntohl(seq_low_n));

    WALEntryType type = static_cast<WALEntryType>(hdr[off++]);

    uint16_t klen_n;
    std::memcpy(&klen_n, hdr + off, 2); off += 2;
    uint16_t klen = ntohs(klen_n);

    uint32_t vlen_n;
    std::memcpy(&vlen_n, hdr + off, 4); off += 4;
    uint32_t vlen = ntohl(vlen_n);

    std::string key(klen, '\0');
    if (klen > 0) {
        n = ::read(fd, key.data(), klen);
        if (n != static_cast<ssize_t>(klen)) return false;
    }

    std::string value(vlen, '\0');
    if (vlen > 0) {
        n = ::read(fd, value.data(), vlen);
        if (n != static_cast<ssize_t>(vlen)) return false;
    }

    uint32_t stored_crc_n;
    n = ::read(fd, &stored_crc_n, 4);
    if (n != 4) return false;
    uint32_t stored_crc = ntohl(stored_crc_n);

    entry.sequence = seq;
    entry.type     = type;
    entry.key      = key;
    entry.value    = value;

    uint32_t computed_crc = compute_crc(entry);
    if (computed_crc != stored_crc) return false;

    return true;
}

uint64_t WriteAheadLog::replay(const std::function<void(const WALEntry&)>& fn) {
    int rfd = ::open(filepath_.c_str(), O_RDONLY);
    if (rfd < 0) return 0;

    uint64_t max_seq = 0;
    WALEntry entry;
    while (read_entry(rfd, entry)) {
        fn(entry);
        if (entry.sequence > max_seq) max_seq = entry.sequence;
    }

    ::close(rfd);

    std::lock_guard<std::mutex> lock(mutex_);
    next_sequence_ = max_seq + 1;

    return max_seq;
}

uint64_t WriteAheadLog::current_sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return next_sequence_ - 1;
}

std::vector<WALEntry> WriteAheadLog::read_from(uint64_t start_sequence) {
    std::lock_guard<std::mutex> lock(mutex_);

    int rfd = ::open(filepath_.c_str(), O_RDONLY);
    if (rfd < 0) return {};

    std::vector<WALEntry> result;
    WALEntry entry;
    while (read_entry(rfd, entry)) {
        if (entry.sequence >= start_sequence) {
            result.push_back(entry);
        }
    }

    ::close(rfd);
    return result;
}
