#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Plain data structs that cross the C++/Python boundary via PyBind11.
// No engine headers included here — these are pure data carriers.

struct OHLCVBar {
    uint64_t timestamp;
    double   open;
    double   high;
    double   low;
    double   close;
    uint64_t volume;
};

struct SignalRecord {
    // bar_index is used instead of raw timestamp because DataHandler stores
    // timestamp=0 for date-string CSVs (e.g. "2024-01-02"), making all bars
    // indistinguishable by timestamp. The sequential bar counter is reliable.
    uint64_t    bar_index;
    std::string direction;  // "LONG" or "EXIT"
    double      price;      // close price at the signal bar
};

struct BacktestResult {
    // Per-bar series (one entry per bar, aligned by index)
    std::vector<double>       equity_curve;
    std::vector<uint64_t>     timestamps;
    std::vector<OHLCVBar>     market_data;

    // Per-signal series
    std::vector<SignalRecord>  signals;

    // Per-trade series
    std::vector<double>        trade_pnls;

    // Scalar performance metrics
    double total_return = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double win_rate     = 0.0;

    int total_trades = 0;
    int total_bars   = 0;

    // Engine throughput metrics
    double   elapsed_ms       = 0.0;  // wall-clock time for the full event loop
    uint64_t total_events     = 0;    // all events dispatched through std::visit
    double   events_per_sec   = 0.0;  // total_events / elapsed_seconds
    double   bars_per_sec     = 0.0;  // total_bars    / elapsed_seconds
    double   column_align_ms  = 0.0;  // one-time CSV header parsing cost
};
