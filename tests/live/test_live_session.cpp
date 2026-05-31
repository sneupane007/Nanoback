#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "include/live_session.hpp"

// Helper: push N bars of constant OHLCV into a session.
static void feed_flat(LiveSession& s, double price, int n) {
    for (int i = 0; i < n; ++i)
        s.tick(price, price, price, price, 1000);
}

// Helper: push N rising bars (close increases by 1 each bar).
static void feed_rising(LiveSession& s, double start, int n) {
    for (int i = 0; i < n; ++i) {
        double p = start + i;
        s.tick(p, p, p, p, 1000);
    }
}

// ── Initial state ─────────────────────────────────────────────────────────────

TEST_CASE("LiveSession: initial state before any tick") {
    LiveSession s("AAPL", "sma", 3, 5, 0.0, 100000.0);
    auto state = s.get_state();

    CHECK(state.running == true);
    CHECK(state.bar_count == 0);
    CHECK(state.event_count == 0);
    CHECK(state.cash == doctest::Approx(100000.0));
    CHECK(state.equity_curve.empty());
    CHECK(state.trade_pnls.empty());
    CHECK(state.last_signal_direction == "");
    CHECK(state.positions.empty());
}

// ── Single tick ───────────────────────────────────────────────────────────────

TEST_CASE("LiveSession: one tick advances bar_count and records equity") {
    LiveSession s("AAPL", "sma", 2, 4, 0.0, 100000.0);
    s.tick(100.0, 105.0, 95.0, 102.0, 5000);
    auto state = s.get_state();

    CHECK(state.bar_count == 1);
    CHECK(state.equity_curve.size() == 1);
    // No position yet (SMA still warming up) — equity == cash
    CHECK(state.equity == doctest::Approx(state.cash));
}

// ── Strategy warmup preserved across ticks ────────────────────────────────────

TEST_CASE("LiveSession: SMA(2,4) produces no signal before slow_period bars") {
    // SMA needs slow_period=4 bars before it can compare SMAs.
    LiveSession s("AAPL", "sma", 2, 4, 0.0, 100000.0);
    feed_flat(s, 100.0, 3);   // one short of slow_period
    auto state = s.get_state();

    CHECK(state.bar_count == 3);
    CHECK(state.last_signal_direction == "");  // no signal yet
}

// ── LONG signal generated after warmup ────────────────────────────────────────

TEST_CASE("LiveSession: SMA(2,4) generates LONG after bullish bars") {
    // Feed 4 flat bars (warmup), then 2 strongly rising bars so fast > slow.
    LiveSession s("AAPL", "sma", 2, 4, 0.0, 100000.0);
    feed_flat(s, 100.0, 4);
    feed_rising(s, 200.0, 2);   // prices: 200, 201 — fast SMA > slow SMA
    auto state = s.get_state();

    CHECK(state.last_signal_direction == "LONG");
    CHECK(state.positions.size() == 1);
    CHECK(state.positions[0].ticker == "AAPL");
    CHECK(state.positions[0].shares > 0.0);
}

// ── stop() prevents further processing ───────────────────────────────────────

TEST_CASE("LiveSession: stop() causes tick() to be a no-op") {
    LiveSession s("AAPL", "sma", 2, 4, 0.0, 100000.0);
    s.tick(100.0, 100.0, 100.0, 100.0, 1000);
    CHECK(s.is_running() == true);

    s.stop();
    CHECK(s.is_running() == false);

    s.tick(200.0, 200.0, 200.0, 200.0, 1000);  // should be ignored
    auto state = s.get_state();
    CHECK(state.bar_count == 1);  // second tick was a no-op
    CHECK(state.running == false);
}

// ── RSI strategy works over multiple ticks ────────────────────────────────────

TEST_CASE("LiveSession: RSI strategy processes 20 bars without error") {
    LiveSession s("AAPL", "rsi", 14, 0, 70.0, 100000.0);
    for (int i = 0; i < 20; ++i) {
        double p = 100.0 + i;
        s.tick(p, p + 2, p - 2, p, 1000);
    }
    auto state = s.get_state();
    CHECK(state.bar_count == 20);
    CHECK(state.equity_curve.size() == 20);
}

// ── MeanReversion strategy works over multiple ticks ─────────────────────────

TEST_CASE("LiveSession: MeanReversion strategy processes 30 bars without error") {
    LiveSession s("AAPL", "mean_reversion", 10, 0, 2.0, 100000.0);
    for (int i = 0; i < 30; ++i) {
        double p = 100.0 + (i % 5) * 0.5;   // oscillating price
        s.tick(p, p + 1, p - 1, p, 1000);
    }
    auto state = s.get_state();
    CHECK(state.bar_count == 30);
    CHECK(state.equity_curve.size() == 30);
}

// ── get_state() returns consistent snapshot ───────────────────────────────────

TEST_CASE("LiveSession: equity_curve length matches bar_count") {
    LiveSession s("AAPL", "sma", 2, 4, 0.0, 50000.0);
    for (int i = 0; i < 10; ++i)
        s.tick(100.0 + i, 105.0 + i, 95.0 + i, 100.0 + i, 1000);
    auto state = s.get_state();
    CHECK(state.equity_curve.size() == (size_t)state.bar_count);
    CHECK(state.bar_count == 10);
}
