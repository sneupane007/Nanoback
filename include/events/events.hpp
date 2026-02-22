#pragma once // Prevents the compiler from copy-pasting this file twice
#include <string>
#include <cstdint> // For fixed-width integers like uint32_t
#include <variant>

// Strictly define what can and cannot be done
enum class OrderDirection { LONG, SHORT, EXIT };
enum class OrderType { MARKET, LIMIT };


// 1. MarketEvent: The market pulse
struct MarketEvent {
    uint64_t timestamp;      // Unix epoch time (faster than passing date strings)
    std::string ticker;
    double open;
    double high;
    double low;
    double close;
    uint32_t volume;         // Volume can't be negative, so use unsigned int
};

// 2. SignalEvent: The Strategy's opinion
struct SignalEvent {
    uint64_t timestamp;
    std::string ticker;
    OrderDirection direction;
    double target_weight;    // e.g., 0.5 means "put 50% of my portfolio in this"
};

// 3. OrderEvent: The Portfolio's command
struct OrderEvent {
    uint64_t timestamp;
    std::string ticker;
    OrderDirection direction;
    OrderType type;
    uint32_t quantity;       // Exact number of shares to buy/sell
};

// 4. FillEvent: The Execution's receipt
struct FillEvent {
    uint64_t timestamp;
    std::string ticker;
    OrderDirection direction;
    double fill_price;       // The actual price you got (including slippage)
    uint32_t quantity;
    double commission;       // What the broker charged you
};

// This is the actual type your EventQueue will store.
using Event = std::variant<MarketEvent, SignalEvent, OrderEvent, FillEvent>;