#include "stats.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <cmath>

Stats::Stats(Stats&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.mutex_);
    latencies_ = std::move(other.latencies_);
    sorted_ = other.sorted_;
}

Stats& Stats::operator=(Stats&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        latencies_ = std::move(other.latencies_);
        sorted_ = other.sorted_;
    }
    return *this;
}

void Stats::record(std::chrono::nanoseconds latency) {
    std::lock_guard<std::mutex> lock(mutex_);
    latencies_.push_back(latency);
    sorted_ = false;
}

void Stats::merge(const Stats& other) {
    std::lock_guard<std::mutex> lock_this(mutex_);
    std::lock_guard<std::mutex> lock_other(other.mutex_);
    latencies_.insert(latencies_.end(), other.latencies_.begin(), other.latencies_.end());
    sorted_ = false;
}

size_t Stats::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latencies_.size();
}

double Stats::ops_per_sec(std::chrono::nanoseconds total_duration) const {
    if (total_duration.count() == 0) return 0.0;
    double seconds = std::chrono::duration<double>(total_duration).count();
    return static_cast<double>(count()) / seconds;
}

std::chrono::nanoseconds Stats::percentile(double p) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (latencies_.empty()) return std::chrono::nanoseconds{0};
    ensure_sorted();
    size_t n = latencies_.size();
    size_t index = static_cast<size_t>(std::floor(p * static_cast<double>(n - 1)));
    if (index >= n) index = n - 1;
    return latencies_[index];
}

std::chrono::nanoseconds Stats::min() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (latencies_.empty()) return std::chrono::nanoseconds{0};
    ensure_sorted();
    return latencies_.front();
}

std::chrono::nanoseconds Stats::max() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (latencies_.empty()) return std::chrono::nanoseconds{0};
    ensure_sorted();
    return latencies_.back();
}

std::chrono::nanoseconds Stats::mean() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (latencies_.empty()) return std::chrono::nanoseconds{0};
    auto total = std::accumulate(latencies_.begin(), latencies_.end(), std::chrono::nanoseconds{0});
    return total / latencies_.size();
}

void Stats::ensure_sorted() const {
    if (!sorted_) {
        auto& mutable_latencies = const_cast<std::vector<std::chrono::nanoseconds>&>(latencies_);
        std::sort(mutable_latencies.begin(), mutable_latencies.end());
        sorted_ = true;
    }
}

static std::string format_duration(std::chrono::nanoseconds ns) {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(ns).count();
    std::ostringstream oss;
    if (us >= 1000) {
        oss << std::fixed << std::setprecision(1) << (static_cast<double>(us) / 1000.0) << "ms";
    } else {
        oss << us << "us";
    }
    return oss.str();
}

std::string Stats::report(std::chrono::nanoseconds total_duration) const {
    double seconds = std::chrono::duration<double>(total_duration).count();
    size_t n = count();
    double throughput = ops_per_sec(total_duration);

    std::ostringstream oss;
    oss << "=== Benchmark Results ===\n";
    oss << "Total operations: " << n << "\n";
    oss << std::fixed << std::setprecision(2);
    oss << "Duration:         " << seconds << "s\n";
    oss << std::fixed << std::setprecision(0);
    oss << "Throughput:       " << throughput << " ops/sec\n";
    oss << "\nLatency:\n";
    oss << "  min:  " << format_duration(min()) << "\n";
    oss << "  p50:  " << format_duration(percentile(0.50)) << "\n";
    oss << "  p95:  " << format_duration(percentile(0.95)) << "\n";
    oss << "  p99:  " << format_duration(percentile(0.99)) << "\n";
    oss << "  max:  " << format_duration(max()) << "\n";
    oss << "  mean: " << format_duration(mean()) << "\n";
    return oss.str();
}
