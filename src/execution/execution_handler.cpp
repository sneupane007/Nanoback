#include "include/execution/execution_handler.hpp"
#include <algorithm>

ExecutionHandler::ExecutionHandler(double slippage_factor,
                                   double per_share_commission,
                                   double min_commission)
    : slippage_factor_(slippage_factor),
      per_share_commission_(per_share_commission),
      min_commission_(min_commission) {}

void ExecutionHandler::on_order(const OrderEvent& o, EventQueue& queue, double latest_close) {
    if (latest_close <= 0.0 || o.quantity == 0) return;

    double slip = (o.direction == OrderDirection::LONG)
                  ? 1.0 + slippage_factor_
                  : 1.0 - slippage_factor_;

    double fill_price = latest_close * slip;
    double commission = std::max(min_commission_,
                                 static_cast<double>(o.quantity) * per_share_commission_);

    queue.push(FillEvent{o.timestamp, o.ticker, o.direction,
                         fill_price, o.quantity, commission});
}
