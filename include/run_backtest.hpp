#pragma once
#include <string>
#include <vector>
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

// Metrics-only result carrier for one swept run. Echoes the sweep axes
// (ticker, p1, p2, fp) plus the scalar metrics, and deliberately OMITS the heavy
// per-bar vectors in BacktestResult (equity_curve, market_data, timestamps) so a
// grid of hundreds of runs stays cheap to move across the PyBind11 boundary.
struct SweepResult {
    std::string ticker;
    int    p1 = 0;
    int    p2 = 0;
    double fp = 0.0;

    double total_return = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double win_rate     = 0.0;
    int    total_trades = 0;
    int    total_bars   = 0;
    double elapsed_ms   = 0.0;
};

// Runs every config in parallel and returns one SweepResult per config, in the
// same order as the input. Work is launched in bounded waves of
// std::thread::hardware_concurrency() so a large multi-ticker grid never spawns
// hundreds of OS threads at once (the workload is CPU-bound). Strategy names are
// validated up front, so a typo throws std::invalid_argument immediately rather
// than surfacing deep inside a worker at future::get().
std::vector<SweepResult> run_batch(const std::vector<BacktestConfig>& configs);
