#pragma once
#include "strategy/istrategy.hpp"

// RSI (Relative Strength Index) mean-reversion strategy.
// Uses Wilder's smoothed moving average over `period` bars.
// Emits SignalEvent(LONG)  when RSI drops below `oversold`  (default 30).
// Emits SignalEvent(EXIT)  when RSI rises above `overbought` (default 70).
class RsiStrategy : public IStrategy {
public:
    explicit RsiStrategy(int period = 14,
                         double oversold   = 30.0,
                         double overbought = 70.0);

    void on_market_event(const MarketEvent& m, EventQueue& queue) override;

private:
    int    period_;
    double oversold_;
    double overbought_;

    double prev_close_;
    double avg_gain_;   // Wilder's smoothed average gain
    double avg_loss_;   // Wilder's smoothed average loss
    int    bars_seen_;
    bool   warmed_up_;
    bool   in_position_;
};
