#pragma once
#include <string>
#include "backtest_result.hpp"

// Groups the parameters that main.cpp used to parse from argv into one value
// object, so a single backtest can be launched programmatically — e.g. once
// per thread in a parameter sweep. Field defaults mirror the old CLI defaults.
struct BacktestConfig {
    std::string data_path;
    std::string ticker;
    std::string strategy_name  = "sma";
    int         p1             = 0;
    int         p2             = 0;
    double      fp             = 0.0;
    bool        use_proto      = false;
    double      initial_capital = 100000.0;
};

// Runs one full backtest and returns its metrics + per-bar/per-trade series.
//
// IMPORTANT: this function never writes to stdout. Printing (via
// PerformanceTracker::report()) mutates shared std::cout format flags, which is
// a data race when many runs execute concurrently. Keeping run_backtest() silent
// makes it safe to call from many threads at once; the caller prints serially.
//
// Throws std::invalid_argument if strategy_name is unknown (from make_strategy).
BacktestResult run_backtest(const BacktestConfig& cfg);
