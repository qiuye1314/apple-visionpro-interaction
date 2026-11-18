#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace draw_only_physx {
inline std::int64_t NowMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void ObserveTaskCost(std::atomic<int>& pressure, double milliseconds) {
    int value = pressure.load();
    if (milliseconds > 250.0) {
        while (value < 100 && !pressure.compare_exchange_weak(value, value + 1)) {}
    } else if (milliseconds < 30.0) {
        while (value > 0 && !pressure.compare_exchange_weak(value, value - 1)) {}
    }
}

// The three scene workers share one recovery probe. High pressure suppresses
// bulk work, but must not suppress every future task that could lower pressure.
class PressureCooldown {
    std::atomic<std::int64_t> nextProbe_{0};
public:
    static constexpr std::int64_t IntervalMs = 2000;
    bool Allow(int pressure, std::int64_t now) {
        if (pressure < 4) { nextProbe_.store(0); return true; }
        auto next = nextProbe_.load();
        if (next == 0) {
            if (nextProbe_.compare_exchange_strong(next, now + IntervalMs)) return false;
        }
        if (now < next) return false;
        return nextProbe_.compare_exchange_strong(next, now + IntervalMs);
    }
};

// Used on the UI thread only. Manual initialization records an attempt but
// bypasses this gate; automatic failed initialization retries at most once/s.
class RetryCooldown {
    std::int64_t nextAttempt_ = 0;
public:
    static constexpr std::int64_t IntervalMs = 1000;
    void RecordAttempt(std::int64_t now) { nextAttempt_ = now + IntervalMs; }
    bool Allow(std::int64_t now) {
        if (now < nextAttempt_) return false;
        RecordAttempt(now);
        return true;
    }
};

// Count attempted triangles, including malformed/offscreen ones. Uniform
// sampling spans the whole mesh instead of examining an unbounded prefix.
class TriangleBudget {
    std::size_t checked_ = 0;
public:
    static constexpr std::size_t Limit = 4000;
    static std::size_t Stride(std::size_t count) {
        const auto stride = count / Limit + (count % Limit != 0);
        return stride ? stride : 1;
    }
    bool Check() { if (checked_ >= Limit) return false; ++checked_; return true; }
    std::size_t Checked() const { return checked_; }
};
} // namespace draw_only_physx
