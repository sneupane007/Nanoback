#include "include/strategy/mean_reversion_strategy.hpp"
#include <cmath>

MeanReversionStrategy::MeanReversionStrategy(int period, double k)
    : period_(period)
    , k_(k)
    , in_position_(false)
{}

double MeanReversionStrategy::mean() const {
    double sum = 0.0;
    for (double p : prices_) sum += p;
    return sum / static_cast<double>(prices_.size());
}

double MeanReversionStrategy::stddev() const {
    double m        = mean();
    double variance = 0.0;
    for (double p : prices_) variance += (p - m) * (p - m);
    return std::sqrt(variance / static_cast<double>(prices_.size()));
}

void MeanReversionStrategy::on_market_event(const MarketEvent& m, EventQueue& queue) {
    prices_.push_back(m.close);
    if (static_cast<int>(prices_.size()) > period_)
        prices_.pop_front();

    // Not enough data yet to compute meaningful bands.
    if (static_cast<int>(prices_.size()) < period_) return;

    double mid        = mean();
    double sd         = stddev();
    double lower_band = mid - k_ * sd;

    if (!in_position_ && m.close < lower_band) {
        // Price has dipped unusually far below its recent average — buy the dip.
        in_position_ = true;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::LONG, 1.0});
    } else if (in_position_ && m.close >= mid) {
        // Price has returned to the mean — take profit.
        in_position_ = false;
        queue.push(SignalEvent{m.timestamp, m.ticker, OrderDirection::EXIT, 0.0});
    }
}
