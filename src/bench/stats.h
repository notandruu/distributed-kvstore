#pragma once

#include <vector>
#include <chrono>
#include <mutex>
#include <string>

class Stats {
public:
    void record(std::chrono::nanoseconds latency);
    void merge(const Stats& other);
    size_t count() const;
    double ops_per_sec(std::chrono::nanoseconds total_duration) const;
    std::chrono::nanoseconds percentile(double p) const;
    std::chrono::nanoseconds min() const;
    std::chrono::nanoseconds max() const;
    std::chrono::nanoseconds mean() const;
    std::string report(std::chrono::nanoseconds total_duration) const;

private:
    mutable std::mutex mutex_;
    std::vector<std::chrono::nanoseconds> latencies_;
    mutable bool sorted_ = false;

    void ensure_sorted() const;
};
