#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/strategy/strategy.hpp"
#include "include/event_queue.hpp"
#include "include/events.hpp"

// ── SmaCrossStrategy tests ────────────────────────────────────────────────────
// Constructor: SmaCrossStrategy(int fast_period, int slow_period)
// Method:      on_market_event(const MarketEvent&, EventQueue&)
//
// The strategy emits a SignalEvent(LONG) when the fast SMA crosses above the
// slow SMA, and SignalEvent(EXIT) on the reverse. The ticker is forwarded
// directly from the incoming MarketEvent — the strategy itself is ticker-agnostic.

namespace {
// Feed n identical bars (all fields = close) into the strategy.
void feed_bars(SmaCrossStrategy& strat, EventQueue& q,
               const std::string& ticker, double close, int n,
               uint64_t start_ts = 0) {
    for (int i = 0; i < n; ++i) {
        MarketEvent me{start_ts + static_cast<uint64_t>(i),
                       ticker, close, close, close, close, 1000};
        strat.on_market_event(me, q);
    }
}
} // namespace

TEST_CASE("SmaCrossStrategy: no signal emitted before slow SMA warmup is complete") {
    // fast=3, slow=5 — need exactly slow_period bars before any signal can fire.
    SmaCrossStrategy strat(3, 5);
    EventQueue q(64);

    feed_bars(strat, q, "AAPL", 100.0, 4); // one short of slow_period
    CHECK(q.empty());
}

TEST_CASE("SmaCrossStrategy: LONG signal emitted on bullish crossover") {
    // Warm up with flat bars at 100 so both SMAs settle at 100.
    // Then feed rising bars so fast SMA > slow SMA.
    SmaCrossStrategy strat(2, 4);
    EventQueue q(64);

    feed_bars(strat, q, "AAPL", 100.0, 4);       // warmup (no cross yet)
    feed_bars(strat, q, "AAPL", 200.0, 2, 4);    // fast SMA >> slow SMA

    REQUIRE(!q.empty());
    auto sig = std::get<SignalEvent>(q.pop());
    CHECK(sig.ticker    == "AAPL");
    CHECK(sig.direction == OrderDirection::LONG);
    CHECK(sig.target_weight > 0.0);
}

TEST_CASE("SmaCrossStrategy: EXIT signal emitted on bearish crossover") {
    SmaCrossStrategy strat(2, 4);
    EventQueue q(64);

    // Enter a long position
    feed_bars(strat, q, "AAPL", 100.0, 4);
    feed_bars(strat, q, "AAPL", 200.0, 2, 4);
    while (!q.empty()) q.pop(); // drain LONG signal

    // Now drive price sharply down so fast SMA < slow SMA
    feed_bars(strat, q, "AAPL", 10.0, 4, 6);

    bool found_exit = false;
    while (!q.empty()) {
        auto sig = std::get<SignalEvent>(q.pop());
        if (sig.direction == OrderDirection::EXIT) { found_exit = true; break; }
    }
    CHECK(found_exit);
}

TEST_CASE("SmaCrossStrategy: no duplicate LONG signals while already in position") {
    // Once in_position_ is true, successive bullish bars must not emit more LONGs.
    SmaCrossStrategy strat(2, 4);
    EventQueue q(64);

    feed_bars(strat, q, "AAPL", 100.0, 4);
    feed_bars(strat, q, "AAPL", 200.0, 4, 4); // stays bullish

    // Should be exactly one LONG signal, not one per bar
    int long_count = 0;
    while (!q.empty()) {
        auto sig = std::get<SignalEvent>(q.pop());
        if (sig.direction == OrderDirection::LONG) ++long_count;
    }
    CHECK(long_count == 1);
}

TEST_CASE("SmaCrossStrategy: ticker is forwarded from MarketEvent") {
    SmaCrossStrategy strat(2, 4);
    EventQueue q(64);

    feed_bars(strat, q, "TSLA", 100.0, 4);
    feed_bars(strat, q, "TSLA", 999.0, 2, 4);

    REQUIRE(!q.empty());
    auto sig = std::get<SignalEvent>(q.pop());
    CHECK(sig.ticker == "TSLA");
}

TEST_CASE("SmaCrossStrategy: flat prices never trigger a signal") {
    // Identical prices → fast SMA == slow SMA → neither branch fires.
    SmaCrossStrategy strat(3, 6);
    EventQueue q(64);

    feed_bars(strat, q, "AAPL", 100.0, 20);
    CHECK(q.empty());
}
