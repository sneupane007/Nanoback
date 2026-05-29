#include "include/strategy/orb_strategy.hpp"
#include <limits>

OrbStrategy::OrbStrategy(int range_bars, int cycle_bars)
    : range_bars_(range_bars)
    , cycle_bars_(cycle_bars)
    , in_position_(false)
    , range_high_(-std::numeric_limits<double>::infinity())
    , range_low_( std::numeric_limits<double>::infinity())
    , bar_in_cycle_(0)
{}

void OrbStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    ++bar_in_cycle_;

    // After a full cycle, reset and start accumulating a fresh opening range.
    if (bar_in_cycle_ > cycle_bars_) {
        bar_in_cycle_ = 1;
        range_high_   = -std::numeric_limits<double>::infinity();
        range_low_    =  std::numeric_limits<double>::infinity();

        // Exit any open position when the cycle resets.
        if (in_position_) {
            in_position_ = false;
            queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
        }
    }

    if (bar_in_cycle_ <= range_bars_) {
        // Phase 1: build the opening range from the first `range_bars_` bars.
        if (m.high > range_high_) range_high_ = m.high;
        if (m.low  < range_low_)  range_low_  = m.low;
        return;
    }

    // Phase 2: trade breakouts beyond the established opening range.
    if (!in_position_ && m.close > range_high_) {
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    } else if (in_position_ && m.close < range_low_) {
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }
}
