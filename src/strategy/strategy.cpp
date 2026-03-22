#include "include/strategy/strategy.hpp"

SmaCrossStrategy::SmaCrossStrategy(int fast_period, int slow_period)
    : fast_period_(fast_period), slow_period_(slow_period), in_position_(false) {}

double SmaCrossStrategy::sma(int period) const {
    double sum = 0.0;
    auto it = prices_.end();
    for (int i = 0; i < period; ++i) sum += *--it;
    return sum / period;
}

void SmaCrossStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    prices_.push_back(m.close);
    if (static_cast<int>(prices_.size()) > slow_period_)
        prices_.pop_front();

    if (static_cast<int>(prices_.size()) < slow_period_) return;

    double fast = sma(fast_period_);
    double slow = sma(slow_period_);

    if (!in_position_ && fast > slow) {
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    } else if (in_position_ && fast < slow) {
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }
}
