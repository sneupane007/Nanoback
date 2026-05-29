#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "events.hpp"
#include "event_queue.hpp"
#include "strategy/istrategy.hpp"
#include "portfolio.hpp"
#include "execution_handler.hpp"
#include "performance.hpp"
#include "live_state.hpp"
#include "overload.hpp"

// LiveSession holds all engine components as persistent member state so that
// strategy warmup, portfolio positions, and equity history are preserved across
// individual tick() calls.  This is the paper-trading equivalent of main.cpp's
// local-variable setup: same components, same event cascade, but driven one bar
// at a time instead of streaming an entire CSV in a tight loop.
//
// Thread safety: tick() and get_state() are protected by a std::mutex so that
// a Python asyncio background task can call tick() while the HTTP handler calls
// get_state() concurrently without data races.
class LiveSession {
public:
    LiveSession(const std::string& ticker,
                const std::string& strategy_name,
                int    p1             = 0,
                int    p2             = 0,
                double fp             = 0.0,
                double initial_capital = 100000.0);

    ~LiveSession() = default;

    // Feed one OHLCV bar. Runs the full event cascade (MarketEvent →
    // SignalEvent → OrderEvent → FillEvent) synchronously under the mutex.
    void tick(double open, double high, double low,
              double close, uint64_t volume,
              uint64_t timestamp = 0);

    // Return a thread-safe snapshot of the current session state.
    // Copies equity_curve and trade_pnls so Python can read them without locks.
    LiveState get_state() const;

    void stop();
    bool is_running() const { return running_.load(); }

private:
    mutable std::mutex  mtx_;
    std::atomic<bool>   running_{true};

    // Engine components — identical to main.cpp's local variables, but living
    // as members so they persist across tick() calls.
    std::string                ticker_;
    EventQueue                 queue_;          // smaller than backtest (1024 slots)
    std::unique_ptr<IStrategy> strategy_;
    Portfolio                  portfolio_;
    ExecutionHandler           execution_;
    PerformanceTracker         perf_;

    // Per-bar tracking state
    double   latest_close_  = 0.0;
    double   prices_[Portfolio::MAX_ASSETS] = {};
    uint64_t bar_count_     = 0;
    uint64_t event_count_   = 0;

    // Last signal for UI display
    std::string last_signal_direction_;
    double      last_signal_price_ = 0.0;

    // Full bar and signal history for charting
    std::vector<OHLCVBar>    market_data_;
    std::vector<SignalRecord> signals_;
};
