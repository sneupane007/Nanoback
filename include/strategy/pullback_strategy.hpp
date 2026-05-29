#pragma once
#include "strategy/istrategy.hpp"

// Trend-following Pullback strategy.
// Uses a long-period EMA to determine the trend direction, and a shorter EMA
// to detect pullback entries within that trend.
//
// Logic:
//   - Trend is UP  when close > trend_ema (long EMA, default 50 bars)
//   - A pullback occurs when price dips below the short EMA (default 10 bars)
//   - Entry: trend is UP AND fast EMA crosses above slow EMA after a pullback
//   - Exit:  close drops below the trend EMA (trend reversal)
class PullbackStrategy : public IStrategy {
public:
    explicit PullbackStrategy(int trend_period = 50, int pullback_period = 10);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int  trend_period_; // warm-up guard: wait until trend EMA has seen this many bars
    bool in_position_;
    bool had_pullback_; // price dipped below pullback EMA in this uptrend

    double trend_ema_;
    double pullback_ema_;
    double prev_pullback_ema_;
    double trend_alpha_;
    double pullback_alpha_;

    int bars_seen_;
};
