#pragma once
#include "strategy/istrategy.hpp"

// Scalping strategy using two Exponential Moving Averages (EMA).
// Uses a fast EMA (default 5 bars) and a slow EMA (default 13 bars).
// Emits SignalEvent(LONG) on a golden cross  (fast EMA crosses above slow EMA).
// Emits SignalEvent(EXIT) on a death  cross  (fast EMA crosses below slow EMA).
// EMA formula: ema = alpha * close + (1 - alpha) * prev_ema, alpha = 2 / (period + 1)
class ScalpingStrategy : public IStrategy {
public:
    explicit ScalpingStrategy(int fast_period = 5, int slow_period = 13);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int  slow_period_; // warm-up guard: wait until slow EMA has seen this many bars
    bool in_position_;

    double fast_ema_;
    double slow_ema_;
    int    bars_seen_;

    double fast_alpha_; // 2.0 / (fast_period + 1)
    double slow_alpha_; // 2.0 / (slow_period + 1)

    double prev_fast_ema_; // EMA from the previous bar — detects crossover
    double prev_slow_ema_;
};
