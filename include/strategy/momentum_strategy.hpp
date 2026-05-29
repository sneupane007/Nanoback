#pragma once
#include <deque>
#include "strategy/istrategy.hpp"

// Momentum (Rate-of-Change) strategy.
// Computes ROC = (close - close_N_bars_ago) / close_N_bars_ago * 100.
// Emits SignalEvent(LONG)  when ROC rises above  +threshold.
// Emits SignalEvent(EXIT)  when ROC falls below  -threshold.
class MomentumStrategy : public IStrategy {
public:
    explicit MomentumStrategy(int period = 10, double threshold = 1.0);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int    period_;
    double threshold_;
    bool   in_position_;

    std::deque<double> prices_; // sliding window of the last `period_+1` closes
};
