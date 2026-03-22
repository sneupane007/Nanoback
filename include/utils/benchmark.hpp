#pragma once
#include <chrono>
#include <cstdint>

// Wraps std::chrono::high_resolution_clock to measure nanoseconds.
// Usage: auto t = Benchmark::now(); ... auto ns = Benchmark::elapsed_ns(t);
struct Benchmark {
    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    static inline TimePoint now() noexcept { return Clock::now(); }

    static inline int64_t elapsed_ns(TimePoint start) noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start).count();
    }
};
