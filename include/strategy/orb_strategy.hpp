#pragma once
#include "strategy/istrategy.hpp"

// Opening Range Breakout (ORB) strategy.
// A classic day-trading technique: use the first `range_bars` bars to define the
// session's "opening range" (highest high, lowest low), then trade any breakout.
//
// Lifecycle per cycle (resets every `cycle_bars` bars):
//   Phase 1 (bars 1 .. range_bars): accumulate the opening range.
//   Phase 2 (bars range_bars+1 .. cycle_bars):
//     - LONG  when close breaks above range_high (upside breakout).
//     - EXIT  when close breaks below range_low  (range fails or reverses).
//   After cycle_bars total bars the range resets and a new cycle begins.
class OrbStrategy : public IStrategy {
public:
    explicit OrbStrategy(int range_bars = 5, int cycle_bars = 30);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int  range_bars_; // bars used to establish the opening range
    int  cycle_bars_; // total bars per trading cycle before reset
    bool in_position_;

    double range_high_;
    double range_low_;
    int    bar_in_cycle_; // 1-based counter within the current cycle
};
