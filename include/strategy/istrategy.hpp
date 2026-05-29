#pragma once
#include "events.hpp"
#include "event_queue.hpp"

// Abstract interface for all trading strategies.
// Any concrete strategy must implement on_market_event(), which receives
// one OHLCV bar and is expected to push zero or more SignalEvents to the queue.
class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual void on_market_event(const MarketEvent& m, EventQueue& queue) = 0;
};
