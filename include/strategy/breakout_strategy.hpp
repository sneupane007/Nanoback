#pragma once
#include <deque>
#include "strategy/istrategy.hpp"

// Donchian Channel Breakout strategy.
// Tracks the rolling N-period highest high and lowest low.
// Emits SignalEvent(LONG) when close breaks above the N-period high (momentum entry).
// Emits SignalEvent(EXIT) when close breaks below the N-period low  (stop-loss exit).
// Uses the *previous* bar's channel to avoid look-ahead bias.
class BreakoutStrategy : public IStrategy {
public:
    explicit BreakoutStrategy(int period = 20);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int  period_;
    bool in_position_;

    std::deque<double> highs_; // rolling window of the last `period_` highs
    std::deque<double> lows_;  // rolling window of the last `period_` lows

    double channel_high_; // snapshot from previous bar — prevents look-ahead
    double channel_low_;
    bool   channel_ready_;
};
