#include "include/strategy/vwap_strategy.hpp"

VwapStrategy::VwapStrategy(int period)
    : period_(period)
    , in_position_(false)
    , prev_above_(false)
    , sum_tp_vol_(0.0)
    , sum_vol_(0.0)
{}

void VwapStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    // typical_price weights each bar equally across its high/low/close range.
    double tp     = (m.high + m.low + m.close) / 3.0;
    double tp_vol = tp * static_cast<double>(m.volume);

    // Incremental rolling sum: add new bar, subtract the oldest if window is full.
    tp_vol_.push_back(tp_vol);
    volumes_.push_back(m.volume);
    sum_tp_vol_ += tp_vol;
    sum_vol_    += static_cast<double>(m.volume);

    if (static_cast<int>(tp_vol_.size()) > period_) {
        sum_tp_vol_ -= tp_vol_.front();
        sum_vol_    -= static_cast<double>(volumes_.front());
        tp_vol_.pop_front();
        volumes_.pop_front();
    }

    if (static_cast<int>(tp_vol_.size()) < period_) return;
    if (sum_vol_ == 0.0) return; // guard against zero-volume bars

    double vwap       = sum_tp_vol_ / sum_vol_;
    bool   curr_above = m.close > vwap;

    // Golden cross: close just crossed above VWAP — bullish.
    if (!in_position_ && !prev_above_ && curr_above) {
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    }
    // Death cross: close just crossed below VWAP — exit.
    else if (in_position_ && prev_above_ && !curr_above) {
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }

    prev_above_ = curr_above;
}
