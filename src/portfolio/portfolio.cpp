#include "include/portfolio/portfolio.hpp"
#include <cmath>
#include <stdexcept>


Portfolio::Portfolio(double initial_cash)
    : cash_(initial_cash), initial_cash_(initial_cash) {}

void Portfolio::add_ticker(const std::string& ticker) {
    if (ticker_to_id_.count(ticker) == 0) {
        if (next_id_ >= MAX_ASSETS)
            throw std::runtime_error("Portfolio: MAX_ASSETS limit of 64 reached");
        ticker_to_id_[ticker] = next_id_++;
    }
}

int Portfolio::id_of(const std::string& ticker) const {
    auto it = ticker_to_id_.find(ticker);
    if (it == ticker_to_id_.end()) return -1;
    return it->second;
}

void Portfolio::on_signal(const SignalEvent& s, EventQueue& queue, double latest_close) {
    if (latest_close <= 0.0) return;

    int id = id_of(s.ticker);
    if (id < 0) return;

    double total_equity = cash_;
    for (auto& [t, i] : ticker_to_id_)
        total_equity += positions_[i] * latest_close; // simplified: single-asset

    uint32_t qty = 0;
    OrderDirection dir = s.direction;

    if (s.direction == OrderDirection::LONG) {
        double target_value = s.target_weight * total_equity;
        double current_value = positions_[id] * latest_close;
        double delta = target_value - current_value;
        if (delta > latest_close) {
            qty = static_cast<uint32_t>(std::floor(delta / latest_close));
            // Clamp to what cash can actually cover — prevents negative cash / hidden leverage.
            // Guard: if cash_ is negative (e.g. after commission), casting to uint32_t wraps.
            uint32_t affordable = 0;
            if (cash_ > 0.0)
                affordable = static_cast<uint32_t>(std::floor(cash_ / latest_close));
            if (qty > affordable) qty = affordable;
        }
    } else if (s.direction == OrderDirection::EXIT) {
        if (positions_[id] > 0.0) {
            // std::round guards against floating-point drift (e.g. 99.9999 → 99 without it)
            qty = static_cast<uint32_t>(std::round(positions_[id]));
        }
    }

    if (qty > 0) {
        queue.push(OrderEvent{s.timestamp, s.ticker, dir, OrderType::MARKET, qty});
    }
}

double Portfolio::on_fill(const FillEvent& f) {
    int id = id_of(f.ticker);
    if (id < 0)
        throw std::runtime_error("FillEvent for unregistered ticker: " + f.ticker);

    if (f.direction == OrderDirection::LONG) {
        double cost = f.fill_price * f.quantity + f.commission;
        // Weighted average cost
        double total_shares = positions_[id] + f.quantity;
        if (total_shares > 0.0)
            avg_cost_[id] = (avg_cost_[id] * positions_[id] + f.fill_price * f.quantity)
                            / total_shares;
        positions_[id] += f.quantity;
        cash_ -= cost;
        return 0.0;
    } else if (f.direction == OrderDirection::EXIT) {
        double realized_pnl = (f.fill_price - avg_cost_[id]) * f.quantity - f.commission;
        cash_ += f.fill_price * f.quantity - f.commission;
        positions_[id] -= f.quantity;
        if (positions_[id] <= 0.0) {
            positions_[id] = 0.0;
            avg_cost_[id]  = 0.0;
        }
        return realized_pnl;
    }

    throw std::runtime_error("on_fill: SHORT orders are not implemented");
}

double Portfolio::equity(const double prices[]) const {
    double total = cash_;
    for (auto& [t, i] : ticker_to_id_)
        total += positions_[i] * prices[i];
    return total;
}
