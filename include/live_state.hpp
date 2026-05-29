#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "backtest_result.hpp"

// Plain data-carrier struct for a single open position.
// Mirrors BacktestResult's pattern: no engine headers, pure POD-like struct.
struct LivePosition {
    std::string ticker;
    double shares        = 0.0;
    double avg_cost      = 0.0;
    double market_value  = 0.0;   // shares * latest_close
    double unrealized_pnl = 0.0; // (latest_close - avg_cost) * shares
};

// Thread-safe snapshot of a LiveSession at a point in time.
// Copied out under the session mutex — safe to read from Python without locks.
struct LiveState {
    // Session metadata
    bool     running      = false;
    uint64_t bar_count    = 0;
    uint64_t event_count  = 0;

    // Portfolio snapshot
    double cash   = 0.0;
    double equity = 0.0;
    std::vector<LivePosition> positions;

    // Historical series (vector copies — safe to hand to Python)
    std::vector<double> equity_curve;
    std::vector<double> trade_pnls;

    // Full bar and signal history for candlestick charting
    std::vector<OHLCVBar>    market_data;
    std::vector<SignalRecord> signals;

    // Most recent strategy signal (empty string if none yet)
    std::string last_signal_direction;
    double      last_signal_price = 0.0;

    // Aggregate metrics (same calculations as PerformanceTracker)
    double total_return = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double win_rate     = 0.0;
    int    total_trades = 0;
};
