#pragma once
#include <deque>
#include "strategy/istrategy.hpp"

// Bollinger Band mean-reversion strategy.
// Computes a `period`-bar SMA and ±k standard-deviation bands.
// Emits SignalEvent(LONG) when price breaks below the lower band.
// Emits SignalEvent(EXIT) when price returns to or above the middle (SMA).
class MeanReversionStrategy : public IStrategy {
public:
    explicit MeanReversionStrategy(int period = 20, double k = 2.0);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int    period_;
    double k_;
    bool   in_position_;

    std::deque<double> prices_; // sliding window of the last `period_` closes

    double mean()   const;
    double stddev() const;
};
