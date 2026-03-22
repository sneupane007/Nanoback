#pragma once
#include <string>
#include <cstdint>
#include <variant>
#include "utils/overload.hpp"

enum class OrderDirection { LONG, SHORT, EXIT };
enum class OrderType      { MARKET, LIMIT };

// alignas(64) pins each struct to a cache-line boundary so adjacent
// elements in the std::vector<Event> buffer never straddle two lines.

struct alignas(64) MarketEvent {
    uint64_t    timestamp;
    std::string ticker;
    double      open;
    double      high;
    double      low;
    double      close;
    uint64_t    volume;
};

struct alignas(64) SignalEvent {
    uint64_t       timestamp;
    std::string    ticker;
    OrderDirection direction;
    double         target_weight;
};

struct alignas(64) OrderEvent {
    uint64_t       timestamp;
    std::string    ticker;
    OrderDirection direction;
    OrderType      type;
    uint32_t       quantity;
};

struct alignas(64) FillEvent {
    uint64_t       timestamp;
    std::string    ticker;
    OrderDirection direction;
    double         fill_price;
    uint32_t       quantity;
    double         commission;
};

using Event = std::variant<MarketEvent, SignalEvent, OrderEvent, FillEvent>;
