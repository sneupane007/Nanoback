#include "include/strategy/momentum_strategy.hpp"

MomentumStrategy::MomentumStrategy(int period, double threshold)
    : period_(period)
    , threshold_(threshold)
    , in_position_(false)
{}

void MomentumStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    prices_.push_back(m.close);

    // Keep one extra bar so we always have `period_` gaps between oldest and newest.
    if (static_cast<int>(prices_.size()) > period_ + 1)
        prices_.pop_front();

    // Need at least period_+1 bars to compute ROC.
    if (static_cast<int>(prices_.size()) < period_ + 1) return;

    // Rate of Change: percentage move over `period_` bars.
    // Positive ROC means the asset has been trending up; negative means down.
    double oldest = prices_.front();
    double roc    = (m.close - oldest) / oldest * 100.0;

    if (!in_position_ && roc > threshold_) {
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    } else if (in_position_ && roc < -threshold_) {
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }
}
