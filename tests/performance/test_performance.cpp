#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/performance.hpp"

// ── PerformanceTracker tests ──────────────────────────────────────────────────
// Public API:
//   record_equity(double)  — appends one bar's equity to the curve
//   record_trade(double)   — records a trade's realized PnL
//   report()               — prints a formatted summary to stdout (const)
//
// NOTE: annualised_sharpe(), max_drawdown(), and win_rate() are private.
// Tests therefore verify observable behaviour: no crashes, no throws, and that
// report() runs without error under various data conditions.

TEST_CASE("PerformanceTracker: construction with default reserve does not throw") {
    CHECK_NOTHROW(PerformanceTracker());
}

TEST_CASE("PerformanceTracker: construction with custom reserve does not throw") {
    CHECK_NOTHROW(PerformanceTracker(500));
}

TEST_CASE("PerformanceTracker: record_equity does not throw") {
    PerformanceTracker t;
    CHECK_NOTHROW(t.record_equity(100000.0));
    CHECK_NOTHROW(t.record_equity(101000.0));
    CHECK_NOTHROW(t.record_equity(99000.0));
}

TEST_CASE("PerformanceTracker: record_trade does not throw") {
    PerformanceTracker t;
    CHECK_NOTHROW(t.record_trade(+500.0));
    CHECK_NOTHROW(t.record_trade(-200.0));
    CHECK_NOTHROW(t.record_trade(0.0));
}

TEST_CASE("PerformanceTracker: report() on empty tracker does not throw") {
    PerformanceTracker t;
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: report() with only one equity bar does not throw") {
    PerformanceTracker t;
    t.record_equity(100000.0);
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: report() with flat equity curve does not throw") {
    PerformanceTracker t;
    for (int i = 0; i < 252; ++i) t.record_equity(100000.0);
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: report() with rising equity curve does not throw") {
    PerformanceTracker t;
    for (int i = 0; i < 100; ++i) t.record_equity(100000.0 + i * 50.0);
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: report() with peak-then-drawdown does not throw") {
    PerformanceTracker t;
    t.record_equity(100000.0);
    t.record_equity(120000.0);  // peak
    t.record_equity(90000.0);   // drawdown
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: report() with mixed win/loss trades does not throw") {
    PerformanceTracker t;
    t.record_trade(+500.0);
    t.record_trade(-100.0);
    t.record_trade(+200.0);
    t.record_trade(-50.0);
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: report() with trades but no equity bars does not throw") {
    PerformanceTracker t;
    t.record_trade(+1000.0);
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: report() with equity bars but no trades does not throw") {
    PerformanceTracker t;
    for (int i = 0; i < 10; ++i) t.record_equity(100000.0 + i * 100.0);
    CHECK_NOTHROW(t.report());
}

TEST_CASE("PerformanceTracker: large number of records does not throw") {
    // Stress-test to confirm the reserve_bars hint and vector growth work.
    PerformanceTracker t(10);  // intentionally small reserve; will grow
    for (int i = 0; i < 10000; ++i) t.record_equity(100000.0 + i * 1.0);
    for (int i = 0; i < 1000; ++i)  t.record_trade(i % 2 == 0 ? +10.0 : -5.0);
    CHECK_NOTHROW(t.report());
}
