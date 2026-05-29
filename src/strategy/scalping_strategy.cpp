#include "include/strategy/scalping_strategy.hpp"

ScalpingStrategy::ScalpingStrategy(int fast_period, int slow_period)
    : slow_period_(slow_period)
    , in_position_(false)
    , fast_ema_(0.0)
    , slow_ema_(0.0)
    , bars_seen_(0)
    , fast_alpha_(2.0 / (fast_period + 1))
    , slow_alpha_(2.0 / (slow_period + 1))
    , prev_fast_ema_(0.0)
    , prev_slow_ema_(0.0)
{}

void ScalpingStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    ++bars_seen_;

    // Seed both EMAs on the very first bar with the closing price.
    if (bars_seen_ == 1) {
        fast_ema_ = slow_ema_ = m.close;
        prev_fast_ema_ = prev_slow_ema_ = m.close;
        return;
    }

    // Standard EMA update: blend today's close into the running average.
    // alpha controls how quickly the EMA reacts — larger alpha → more responsive.
    prev_fast_ema_ = fast_ema_;
    prev_slow_ema_ = slow_ema_;
    fast_ema_ = fast_alpha_ * m.close + (1.0 - fast_alpha_) * fast_ema_;
    slow_ema_ = slow_alpha_ * m.close + (1.0 - slow_alpha_) * slow_ema_;

    // Wait for the slow EMA to warm up before trading.
    if (bars_seen_ < slow_period_) return;

    bool prev_above = prev_fast_ema_ > prev_slow_ema_;
    bool curr_above = fast_ema_      > slow_ema_;

    // Golden cross: fast EMA just crossed above slow EMA — bullish momentum.
    if (!in_position_ && !prev_above && curr_above) {
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    }
    // Death cross: fast EMA just crossed below slow EMA — exit position.
    else if (in_position_ && prev_above && !curr_above) {
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }
}
