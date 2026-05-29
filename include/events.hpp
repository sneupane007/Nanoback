#pragma once
#include <string>
#include <cstdint>
#include <variant>
#include "overload.hpp"

enum class OrderDirection { LONG, SHORT, EXIT };
enum class OrderType      { MARKET, LIMIT };

struct MarketEvent {
    uint64_t    timestamp;
    std::string ticker;
    double      open;
    double      high;
    double      low;
    double      close;
    uint64_t    volume;
};

struct SignalEvent {
    uint64_t       timestamp;
    std::string    ticker;
    OrderDirection direction;
    double         target_weight;
};

struct OrderEvent {
    uint64_t       timestamp;
    std::string    ticker;
    OrderDirection direction;
    OrderType      type;
    uint32_t       quantity;
};

struct FillEvent {
    uint64_t       timestamp;
    std::string    ticker;
    OrderDirection direction;
    double         fill_price;
    uint32_t       quantity;
    double         commission;
};

using Event = std::variant<MarketEvent, SignalEvent, OrderEvent, FillEvent>;
