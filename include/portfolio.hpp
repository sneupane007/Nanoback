#pragma once
#include <string>
#include <unordered_map>
#include "events.hpp"
#include "event_queue.hpp"

class Portfolio {
public:
    static constexpr int MAX_ASSETS = 64;

    explicit Portfolio(double initial_cash = 100000.0);

    // Register a ticker before the backtest loop starts.
    void add_ticker(const std::string& ticker);

    void on_signal(const SignalEvent& s, EventQueue& queue, double latest_close);
    [[nodiscard]] double on_fill(const FillEvent& f);

    // Mark-to-market equity given a prices[] snapshot (indexed by ticker id).
    double equity(const double prices[]) const;

    double cash() const { return cash_; }

    // Read-only position accessors used by LiveSession::get_state().
    double position(const std::string& ticker) const {
        int id = id_of(ticker);
        return (id >= 0) ? positions_[id] : 0.0;
    }
    double avg_cost_for(const std::string& ticker) const {
        int id = id_of(ticker);
        return (id >= 0) ? avg_cost_[id] : 0.0;
    }

private:
    double cash_;
    double initial_cash_;

    double positions_[MAX_ASSETS] = {};   // shares held
    double avg_cost_ [MAX_ASSETS] = {};   // average cost per share

    std::unordered_map<std::string, int> ticker_to_id_;
    int next_id_ = 0;

    int id_of(const std::string& ticker) const;
};
