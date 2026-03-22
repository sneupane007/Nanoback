#include "include/performance/performance.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <algorithm>

PerformanceTracker::PerformanceTracker(size_t reserve_bars) {
    equity_curve_.reserve(reserve_bars);
    trade_pnls_.reserve(256);
}

void PerformanceTracker::record_equity(double equity) {
    equity_curve_.push_back(equity);
    sharpe_ready_ = false; // invalidate cache
}

void PerformanceTracker::record_trade(double pnl) {
    trade_pnls_.push_back(pnl);
}

double PerformanceTracker::total_return() const {
    if (equity_curve_.size() < 2) return 0.0;
    return (equity_curve_.back() / equity_curve_.front() - 1.0) * 100.0;
}

double PerformanceTracker::annualised_sharpe() const {
    if (sharpe_ready_) return sharpe_;

    if (equity_curve_.size() < 2) { sharpe_ = 0.0; sharpe_ready_ = true; return 0.0; }

    // Daily returns
    std::vector<double> rets;
    rets.reserve(equity_curve_.size() - 1);
    for (size_t i = 1; i < equity_curve_.size(); ++i) {
        if (equity_curve_[i - 1] > 0.0)
            rets.push_back(equity_curve_[i] / equity_curve_[i - 1] - 1.0);
    }

    if (rets.empty()) { sharpe_ = 0.0; sharpe_ready_ = true; return 0.0; }

    double mean = std::accumulate(rets.begin(), rets.end(), 0.0) / rets.size();
    double sq_sum = 0.0;
    for (double r : rets) sq_sum += (r - mean) * (r - mean);
    // Sample stddev (N-1): standard financial convention for Sharpe ratio.
    // Population stddev (N) underestimates variance for finite sample sizes.
    double stddev = std::sqrt(sq_sum / (rets.size() - 1));

    sharpe_ = (stddev > 0.0) ? (mean / stddev) * std::sqrt(252.0) : 0.0;
    sharpe_ready_ = true;
    return sharpe_;
}

double PerformanceTracker::max_drawdown() const {
    if (equity_curve_.empty()) return 0.0;
    double peak = equity_curve_[0];
    double max_dd = 0.0;
    for (double e : equity_curve_) {
        if (e > peak) peak = e;
        double dd = (peak - e) / peak * 100.0;
        if (dd > max_dd) max_dd = dd;
    }
    return max_dd;
}

double PerformanceTracker::win_rate() const {
    if (trade_pnls_.empty()) return 0.0;
    int wins = 0;
    for (double p : trade_pnls_) if (p > 0.0) ++wins;
    return static_cast<double>(wins) / trade_pnls_.size() * 100.0;
}

void PerformanceTracker::report() const {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n=== Backtest Performance Report ===\n";
    std::cout << "Total Return : " << total_return()       << "%\n";
    std::cout << std::setprecision(4);
    std::cout << "Sharpe Ratio : " << annualised_sharpe()  << "\n";
    std::cout << std::setprecision(2);
    std::cout << "Max Drawdown : " << max_drawdown()       << "%\n";
    std::cout << "Win Rate     : " << win_rate()           << "%\n";
    std::cout << "Total Trades : " << trade_pnls_.size()   << "\n";
    std::cout << "Total Bars   : " << equity_curve_.size() << "\n";
}
