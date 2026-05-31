#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/execution_handler.hpp"
#include "include/event_queue.hpp"
#include "include/events.hpp"

// ── ExecutionHandler tests ────────────────────────────────────────────────────
// The handler turns OrderEvents into FillEvents, applying slippage and commission.
// Key behaviours:
//   - A LONG order produces a FillEvent with direction LONG
//   - Fill price includes slippage (fill_price >= order price)
//   - Commission is non-negative
//   - An EXIT order produces a FillEvent with direction EXIT

TEST_CASE("ExecutionHandler: LONG order produces FillEvent") {
    ExecutionHandler handler;
    EventQueue q(16);

    OrderEvent order{1000, "AAPL", OrderDirection::LONG, OrderType::MARKET, 10};
    double latest_close = 150.0;
    handler.on_order(order, q, latest_close);

    REQUIRE(!q.empty());
    auto fill = std::get<FillEvent>(q.pop());
    CHECK(fill.ticker     == "AAPL");
    CHECK(fill.direction  == OrderDirection::LONG);
    CHECK(fill.quantity   == 10u);
    CHECK(fill.fill_price >= latest_close);  // slippage never reduces price on buy
    CHECK(fill.commission >= 0.0);
}

TEST_CASE("ExecutionHandler: EXIT order produces FillEvent with EXIT direction") {
    ExecutionHandler handler;
    EventQueue q(16);

    OrderEvent order{2000, "AAPL", OrderDirection::EXIT, OrderType::MARKET, 5};
    handler.on_order(order, q, 155.0);

    REQUIRE(!q.empty());
    auto fill = std::get<FillEvent>(q.pop());
    CHECK(fill.direction == OrderDirection::EXIT);
    CHECK(fill.quantity  == 5u);
}

TEST_CASE("ExecutionHandler: zero-quantity order emits no fill") {
    ExecutionHandler handler;
    EventQueue q(16);

    OrderEvent order{3000, "AAPL", OrderDirection::LONG, OrderType::MARKET, 0};
    handler.on_order(order, q, 150.0);
    CHECK(q.empty());
}
