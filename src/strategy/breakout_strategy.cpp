#include "include/strategy/breakout_strategy.hpp"
#include <algorithm>

BreakoutStrategy::BreakoutStrategy(int period)
    : period_(period)
    , in_position_(false)
    , channel_high_(0.0)
    , channel_low_(0.0)
    , channel_ready_(false)
{}

void BreakoutStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    // Snapshot the channel from the PREVIOUS bar before updating the window.
    // This prevents look-ahead: today's breakout is judged against yesterday's range.
    if (static_cast<int>(highs_.size()) == period_) {
        channel_high_  = *std::max_element(highs_.begin(), highs_.end());
        channel_low_   = *std::min_element(lows_.begin(),  lows_.end());
        channel_ready_ = true;
    }

    highs_.push_back(m.high);
    lows_.push_back(m.low);

    if (static_cast<int>(highs_.size()) > period_) {
        highs_.pop_front();
        lows_.pop_front();
    }

    if (!channel_ready_) return;

    if (!in_position_ && m.close > channel_high_) {
        // Price punched through the top of the range — upside breakout.
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    } else if (in_position_ && m.close < channel_low_) {
        // Price fell below the channel floor — exit to limit losses.
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }
}
