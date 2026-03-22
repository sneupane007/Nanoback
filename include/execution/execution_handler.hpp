#pragma once
#include "events/events.hpp"
#include "events/event_queue.hpp"

// Slippage + commission model.
// fill_price = close × (1 ± slippage_factor)
// commission = max(min_commission, qty × per_share_commission)
class ExecutionHandler {
public:
    explicit ExecutionHandler(double slippage_factor     = 0.0005,
                              double per_share_commission = 0.005,
                              double min_commission       = 1.0);

    void on_order(const OrderEvent& o, EventQueue& queue, double latest_close);

private:
    double slippage_factor_;
    double per_share_commission_;
    double min_commission_;
};
