#pragma once
#include <vector>
#include <string>

class PerformanceTracker {
public:
    explicit PerformanceTracker(size_t reserve_bars = 10000);

    void record_equity(double equity);
    void record_trade(double pnl);

    // Print formatted summary to stdout.
    void report() const;

private:
    std::vector<double> equity_curve_;
    std::vector<double> trade_pnls_;

    // Lazy computed metrics (mutable so report() can be const).
    mutable double  sharpe_       = 0.0;
    mutable bool    sharpe_ready_ = false;

    double total_return()   const;
    double annualised_sharpe() const;
    double max_drawdown()   const;
    double win_rate()       const;
};
