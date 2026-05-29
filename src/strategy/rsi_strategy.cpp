#include "include/strategy/rsi_strategy.hpp"

RsiStrategy::RsiStrategy(int period, double oversold, double overbought)
    : period_(period)
    , oversold_(oversold)
    , overbought_(overbought)
    , prev_close_(0.0)
    , avg_gain_(0.0)
    , avg_loss_(0.0)
    , bars_seen_(0)
    , warmed_up_(false)
    , in_position_(false)
{}

void RsiStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    // First bar: just store the close price, nothing to compute yet.
    if (bars_seen_ == 0) {
        prev_close_ = m.close;
        ++bars_seen_;
        return;
    }

    double change = m.close - prev_close_;
    double gain   = (change > 0.0) ? change  : 0.0;
    double loss   = (change < 0.0) ? -change : 0.0;
    prev_close_   = m.close;
    ++bars_seen_;

    if (!warmed_up_) {
        // Accumulate a simple sum; divide once we reach `period_` data points.
        avg_gain_ += gain;
        avg_loss_ += loss;

        if (bars_seen_ == period_ + 1) {
            avg_gain_ /= period_;
            avg_loss_ /= period_;
            warmed_up_ = true;
        }
        return;
    }

    // Wilder's smoothed moving average — equivalent to EMA with alpha = 1/period.
    // Gives more weight to recent bars while preserving all of history.
    avg_gain_ = (avg_gain_ * (period_ - 1) + gain) / period_;
    avg_loss_ = (avg_loss_ * (period_ - 1) + loss) / period_;

    if (avg_loss_ == 0.0) return; // All gains: RSI = 100, avoid divide-by-zero.

    double rs  = avg_gain_ / avg_loss_;
    double rsi = 100.0 - (100.0 / (1.0 + rs));

    if (!in_position_ && rsi < oversold_) {
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    } else if (in_position_ && rsi > overbought_) {
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }
}
