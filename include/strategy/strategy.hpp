#pragma once
#include <deque>
#include "strategy/istrategy.hpp"

// Dual SMA crossover strategy.
// Emits SignalEvent(LONG) when fast SMA crosses above slow SMA,
// and SignalEvent(EXIT) on the reverse crossover.
class SmaCrossStrategy : public IStrategy {
public:
    SmaCrossStrategy(int fast_period = 10, int slow_period = 30);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int  fast_period_;
    int  slow_period_;
    bool in_position_;

    std::deque<double> prices_; // sliding window — O(1) push_back / pop_front

    double sma(int period) const;
};
