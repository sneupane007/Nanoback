#include "include/live_session.hpp"
#include "include/strategy/strategy_factory.hpp"

LiveSession::LiveSession(const std::string& ticker,
                         const std::string& strategy_name,
                         int    p1,
                         int    p2,
                         double fp,
                         double initial_capital)
    : ticker_(ticker)
    , queue_(1024)
    , strategy_(make_strategy(strategy_name, p1, p2, fp))
    , portfolio_(initial_capital)
    , execution_()
    , perf_(1000)
{
    portfolio_.add_ticker(ticker_);
}

void LiveSession::tick(double open, double high, double low,
                       double close, uint64_t volume,
                       uint64_t timestamp)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (!running_.load()) return;

    // Use the caller-supplied wall-clock timestamp when available (paper trading
    // passes the real Unix epoch from yfinance).  Fall back to bar_count_ as a
    // monotonic placeholder so backtests still work without a real timestamp.
    uint64_t ts = (timestamp > 0) ? timestamp : bar_count_;
    queue_.push(Event{MarketEvent{ts, ticker_, open, high, low, close, volume}});

    // Drain the full event cascade — identical to main.cpp lines 71-98.
    while (!queue_.empty()) {
        Event e = queue_.pop();
        ++event_count_;

        std::visit(overload{
            [&](const MarketEvent& m) {
                latest_close_ = m.close;
                prices_[0]    = m.close;
                market_data_.push_back({ts, m.open, m.high, m.low, m.close, m.volume});
                strategy_->on_market_event(m, queue_);
            },
            [&](const SignalEvent& s) {
                last_signal_direction_ =
                    (s.direction == OrderDirection::LONG) ? "LONG" : "EXIT";
                last_signal_price_ = latest_close_;
                signals_.push_back({bar_count_, last_signal_direction_, latest_close_});
                portfolio_.on_signal(s, queue_, latest_close_);
            },
            [&](const OrderEvent& o) {
                execution_.on_order(o, queue_, latest_close_);
            },
            [&](const FillEvent& f) {
                double pnl = portfolio_.on_fill(f);
                if (f.direction == OrderDirection::EXIT)
                    perf_.record_trade(pnl);
            },
        }, e);
    }

    perf_.record_equity(portfolio_.equity(prices_));
    ++bar_count_;
}

LiveState LiveSession::get_state() const {
    std::lock_guard<std::mutex> lock(mtx_);

    LiveState state;
    state.running     = running_.load();
    state.bar_count   = bar_count_;
    state.event_count = event_count_;
    state.cash        = portfolio_.cash();
    state.equity      = perf_.equity_curve().empty()
                            ? portfolio_.cash()
                            : perf_.equity_curve().back();

    // Build position entry for the single tracked asset.
    double shares = portfolio_.position(ticker_);
    if (shares > 0.0) {
        LivePosition pos;
        pos.ticker         = ticker_;
        pos.shares         = shares;
        pos.avg_cost       = portfolio_.avg_cost_for(ticker_);
        pos.market_value   = shares * latest_close_;
        pos.unrealized_pnl = (latest_close_ - pos.avg_cost) * shares;
        state.positions.push_back(pos);
    }

    // Vector copies — safe to read from Python without holding the mutex.
    state.equity_curve = perf_.equity_curve();
    state.trade_pnls   = perf_.trade_pnls();
    state.market_data  = market_data_;
    state.signals      = signals_;

    state.last_signal_direction = last_signal_direction_;
    state.last_signal_price     = last_signal_price_;

    // Metrics from PerformanceTracker (all handle empty state safely).
    state.total_return  = perf_.total_return();
    state.sharpe_ratio  = perf_.annualised_sharpe();
    state.max_drawdown  = perf_.max_drawdown();
    state.win_rate      = perf_.win_rate();
    state.total_trades  = static_cast<int>(perf_.trade_pnls().size());

    return state;
}

void LiveSession::stop() {
    running_.store(false);
}
