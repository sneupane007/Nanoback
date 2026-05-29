#include "include/strategy/pullback_strategy.hpp"

PullbackStrategy::PullbackStrategy(int trend_period, int pullback_period)
    : trend_period_(trend_period)
    , in_position_(false)
    , had_pullback_(false)
    , trend_ema_(0.0)
    , pullback_ema_(0.0)
    , prev_pullback_ema_(0.0)
    , trend_alpha_(2.0 / (trend_period + 1))
    , pullback_alpha_(2.0 / (pullback_period + 1))
    , bars_seen_(0)
{}

void PullbackStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    ++bars_seen_;

    if (bars_seen_ == 1) {
        trend_ema_ = pullback_ema_ = prev_pullback_ema_ = m.close;
        return;
    }

    prev_pullback_ema_ = pullback_ema_;

    trend_ema_    = trend_alpha_    * m.close + (1.0 - trend_alpha_)    * trend_ema_;
    pullback_ema_ = pullback_alpha_ * m.close + (1.0 - pullback_alpha_) * pullback_ema_;

    // Wait for the slower trend EMA to warm up before trading.
    if (bars_seen_ < trend_period_) return;

    bool uptrend = m.close > trend_ema_;

    if (!in_position_) {
        if (!uptrend) {
            // Not in an uptrend — reset the pullback flag.
            had_pullback_ = false;
            return;
        }

        // In an uptrend: detect a pullback (close dips below the fast EMA).
        if (m.close < pullback_ema_) {
            had_pullback_ = true;
        }

        // Entry: after a pullback, wait for close to recross above the fast EMA.
        // This confirms the pullback is over and the trend is resuming.
        if (had_pullback_ && m.close > pullback_ema_ && prev_pullback_ema_ <= m.close) {
            in_position_  = true;
            had_pullback_ = false;
            queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
        }
    } else {
        // Exit when the trend reverses: close falls below the long trend EMA.
        if (!uptrend) {
            in_position_  = false;
            had_pullback_ = false;
            queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
        }
    }
}
