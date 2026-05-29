#pragma once
#include <deque>
#include "strategy/istrategy.hpp"

// Rolling VWAP (Volume-Weighted Average Price) strategy.
// Computes VWAP over a rolling N-bar window:
//   VWAP = sum(typical_price * volume) / sum(volume)
//   typical_price = (high + low + close) / 3
//
// Emits SignalEvent(LONG) when close crosses above VWAP (price gaining strength).
// Emits SignalEvent(EXIT) when close crosses below VWAP (price losing support).
class VwapStrategy : public IStrategy {
public:
    explicit VwapStrategy(int period = 20);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int  period_;
    bool in_position_;
    bool prev_above_; // was close above VWAP on the previous bar?

    std::deque<double>   tp_vol_;  // typical_price * volume per bar
    std::deque<uint64_t> volumes_; // volume per bar

    double sum_tp_vol_;  // rolling sum — updated incrementally to avoid O(n) recompute
    double sum_vol_;
};
