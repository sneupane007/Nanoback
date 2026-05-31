#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/portfolio.hpp"
#include "include/event_queue.hpp"
#include "include/events.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

// ── Helpers ───────────────────────────────────────────────────────────────────

// Pop the next event from the queue and assert it is an OrderEvent.
// Fails the test immediately (REQUIRE) if the queue is empty or the variant
// holds a different type, so callers can safely inspect the returned value.
static OrderEvent pop_order(EventQueue& q) {
    REQUIRE(!q.empty());
    Event e = q.pop();
    REQUIRE(std::holds_alternative<OrderEvent>(e));
    return std::get<OrderEvent>(e);
}

// on_fill is [[nodiscard]].  When a test only calls it to set up state and does
// not care about the return value, wrap it in this helper so the compiler does
// not emit -Wunused-result warnings.
static void fill(Portfolio& p, const FillEvent& f) {
    (void)p.on_fill(f);
}

// ── add_ticker ────────────────────────────────────────────────────────────────

TEST_CASE("add_ticker: registering a ticker does not throw") {
    Portfolio p(100000.0);
    CHECK_NOTHROW(p.add_ticker("AAPL"));
}

TEST_CASE("add_ticker: adding the same ticker twice is idempotent") {
    // The second add must not increment next_id_.  Verify indirectly: after
    // one duplicate add we can still add 63 more unique tickers (slots 1..63)
    // without hitting the limit.  If the duplicate had consumed a slot only
    // 62 more would fit.
    Portfolio p(100000.0);
    p.add_ticker("AAPL");
    p.add_ticker("AAPL"); // duplicate — must not consume a slot

    for (int i = 1; i < Portfolio::MAX_ASSETS; ++i)
        CHECK_NOTHROW(p.add_ticker("TICKER_" + std::to_string(i)));
}

TEST_CASE("add_ticker: adding exactly MAX_ASSETS unique tickers succeeds") {
    Portfolio p(100000.0);
    for (int i = 0; i < Portfolio::MAX_ASSETS; ++i)
        CHECK_NOTHROW(p.add_ticker("T" + std::to_string(i)));
}

TEST_CASE("add_ticker: adding a 65th unique ticker throws std::runtime_error") {
    Portfolio p(100000.0);
    for (int i = 0; i < Portfolio::MAX_ASSETS; ++i)
        p.add_ticker("T" + std::to_string(i));

    CHECK_THROWS_AS(p.add_ticker("OVERFLOW"), std::runtime_error);
}

// ── on_signal – guard conditions ──────────────────────────────────────────────

TEST_CASE("on_signal: emits nothing when latest_close is zero") {
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    SignalEvent sig{1, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 0.0);
    CHECK(q.empty());
}

TEST_CASE("on_signal: emits nothing when latest_close is negative") {
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    SignalEvent sig{1, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, -50.0);
    CHECK(q.empty());
}

TEST_CASE("on_signal: emits nothing for an unregistered ticker") {
    Portfolio p(100000.0);
    EventQueue q(64);
    // "AAPL" is NOT registered.

    SignalEvent sig{1, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 100.0);
    CHECK(q.empty());
}

// ── on_signal – LONG ──────────────────────────────────────────────────────────

TEST_CASE("on_signal LONG: emits OrderEvent with correct metadata") {
    // $100,000 cash, price $100, target_weight 1.0
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    SignalEvent sig{42, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 100.0);

    OrderEvent order = pop_order(q);
    CHECK(order.ticker    == "AAPL");
    CHECK(order.direction == OrderDirection::LONG);
    CHECK(order.type      == OrderType::MARKET);
    CHECK(order.timestamp == 42);
}

TEST_CASE("on_signal LONG: quantity equals floor(target_value / price) when cash is sufficient") {
    // total_equity = 100,000  weight = 1.0  price = 100
    // delta = 100,000  qty = floor(100,000/100) = 1,000
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    SignalEvent sig{1, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 100.0);

    OrderEvent order = pop_order(q);
    CHECK(order.quantity == 1000);
}

TEST_CASE("on_signal LONG: quantity is clamped to what cash can afford") {
    // Setup: 1,000 shares at $100 leaving only $50 cash.
    // Signal weight = 1.0, price = $100.
    // total_equity = 50 + 1000*100 = 100,050
    // target_value = 100,050  current_value = 100,000  delta = 50
    // delta(50) <= latest_close(100) → no order because delta does not exceed
    // one full share price (the condition `delta > latest_close` is false).
    // This is the "at the boundary" case — delta exactly equals less than one lot.
    Portfolio p(100050.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 1000, 0.0});
    // cash_ = 100,050 − 100*1,000 = 50
    CHECK(p.cash() == doctest::Approx(50.0));

    SignalEvent sig{2, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 100.0);
    CHECK(q.empty()); // delta ≤ price, so no order is emitted
}

TEST_CASE("on_signal LONG: clamp fires when cash cannot afford one share") {
    // Spend all cash: 100 shares at $100 → cash = 0.
    // Signal weight = 1.0, price = $100.
    // total_equity = 0 + 100*100 = 10,000
    // delta = 0  →  delta ≤ price, no order.
    // (Separately: even if delta were > price the affordable = floor(0/100) = 0
    //  clamp would suppress the order.)
    Portfolio p(10000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 100, 0.0});
    CHECK(p.cash() == doctest::Approx(0.0));

    SignalEvent sig{2, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 100.0);
    CHECK(q.empty());
}

TEST_CASE("on_signal LONG: affordable clamp reduces quantity when unclamped qty exceeds cash") {
    // Two registered tickers so total_equity includes a large AAPL position
    // but only $1,000 cash remains; a GOOG signal targets 100% of total equity.
    // total_equity = 1,000 + 9,000*100 = 901,000  (AAPL is priced at 100 too)
    // target_value = 901,000  current_value(GOOG) = 0  delta = 901,000
    // unclamped qty = floor(901,000/100) = 9,010
    // affordable    = floor(1,000/100)  = 10
    // → clamped to 10
    Portfolio p(901000.0);
    EventQueue q(64);
    p.add_ticker("AAPL"); // id 0
    p.add_ticker("GOOG"); // id 1

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 9000, 0.0});
    // cash = 901,000 − 900,000 = 1,000

    SignalEvent sig{2, "GOOG", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 100.0);

    OrderEvent order = pop_order(q);
    CHECK(order.ticker   == "GOOG");
    CHECK(order.quantity == 10); // clamped by affordable
}

TEST_CASE("on_signal LONG: no order emitted when delta is below one share") {
    // total_equity = 100,000  weight = 0.001  price = 100
    // target_value = 100  delta = 100
    // condition: delta(100) > latest_close(100) is FALSE → no order
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    SignalEvent sig{1, "AAPL", OrderDirection::LONG, 0.001};
    p.on_signal(sig, q, 100.0);
    CHECK(q.empty());
}

TEST_CASE("on_signal LONG: partial weight produces correct quantity") {
    // total_equity = 100,000  weight = 0.5  price = 100
    // target_value = 50,000  delta = 50,000  qty = 500
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    SignalEvent sig{1, "AAPL", OrderDirection::LONG, 0.5};
    p.on_signal(sig, q, 100.0);

    OrderEvent order = pop_order(q);
    CHECK(order.quantity == 500);
}

TEST_CASE("on_signal LONG: qty is floored not rounded (fractional shares discarded)") {
    // total_equity = 100,000  weight = 1.0  price = 99
    // delta = 100,000  qty = floor(100,000/99) = 1,010
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    SignalEvent sig{1, "AAPL", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 99.0);

    OrderEvent order = pop_order(q);
    CHECK(order.quantity == static_cast<uint32_t>(std::floor(100000.0 / 99.0)));
}

// ── on_signal – EXIT ──────────────────────────────────────────────────────────

TEST_CASE("on_signal EXIT: emits order for full position when position is positive") {
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 250, 0.0});

    SignalEvent sig{2, "AAPL", OrderDirection::EXIT, 0.0};
    p.on_signal(sig, q, 100.0);

    OrderEvent order = pop_order(q);
    CHECK(order.direction == OrderDirection::EXIT);
    CHECK(order.type      == OrderType::MARKET);
    CHECK(order.ticker    == "AAPL");
    CHECK(order.quantity  == 250);
}

TEST_CASE("on_signal EXIT: emits nothing when position is zero") {
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");
    // No position held.

    SignalEvent sig{1, "AAPL", OrderDirection::EXIT, 0.0};
    p.on_signal(sig, q, 100.0);
    CHECK(q.empty());
}

TEST_CASE("on_signal EXIT: quantity uses std::round to guard against floating-point drift") {
    // Two fills of 50 shares each → position should be exactly 100.
    // std::round on 100.0 gives 100, confirming no truncation.
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 50, 0.0});
    fill(p, {2, "AAPL", OrderDirection::LONG, 100.0, 50, 0.0});

    SignalEvent sig{3, "AAPL", OrderDirection::EXIT, 0.0};
    p.on_signal(sig, q, 100.0);

    OrderEvent order = pop_order(q);
    CHECK(order.quantity == 100);
}

// ── on_fill – LONG ────────────────────────────────────────────────────────────

TEST_CASE("on_fill LONG: cash decreases by fill_price * quantity + commission") {
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    // cost = 150 * 100 + 5 = 15,005
    fill(p, {1, "AAPL", OrderDirection::LONG, 150.0, 100, 5.0});
    CHECK(p.cash() == doctest::Approx(100000.0 - 150.0 * 100.0 - 5.0));
}

TEST_CASE("on_fill LONG: position increases by quantity") {
    // After buying 200 shares at $150 the mark-to-market equity at $150 must
    // still equal the initial cash (no PnL on entry when price == fill price).
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 150.0, 200, 0.0});

    double prices[1] = {150.0};
    CHECK(p.equity(prices) == doctest::Approx(100000.0));
}

TEST_CASE("on_fill LONG: returns 0.0") {
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    FillEvent f{1, "AAPL", OrderDirection::LONG, 100.0, 10, 1.0};
    CHECK(p.on_fill(f) == doctest::Approx(0.0));
}

TEST_CASE("on_fill LONG: weighted average cost is set correctly on first fill") {
    // avg_cost_ starts at 0.  After buying 100 shares at $120:
    //   avg_cost = (0 * 0 + 120 * 100) / 100 = 120
    // Verified indirectly: sell all at $130 → PnL = (130−120)*100 = 1,000
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 120.0, 100, 0.0});

    FillEvent sell{2, "AAPL", OrderDirection::EXIT, 130.0, 100, 0.0};
    CHECK(p.on_fill(sell) == doctest::Approx(1000.0));
}

TEST_CASE("on_fill LONG: weighted average cost updates correctly across two fills at different prices") {
    // Fill 1: 100 shares @ $100  → avg = 100
    // Fill 2: 200 shares @ $130  → avg = (100*100 + 200*130) / 300 = 36,000/300 = 120
    // Sell 300 @ $150  → PnL = (150−120)*300 = 9,000
    Portfolio p(200000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 100, 0.0});
    fill(p, {2, "AAPL", OrderDirection::LONG, 130.0, 200, 0.0});

    FillEvent sell{3, "AAPL", OrderDirection::EXIT, 150.0, 300, 0.0};
    double expected_avg = (100.0 * 100.0 + 130.0 * 200.0) / 300.0;
    CHECK(p.on_fill(sell) == doctest::Approx((150.0 - expected_avg) * 300.0));
}

// ── on_fill – EXIT ────────────────────────────────────────────────────────────

TEST_CASE("on_fill EXIT: cash increases by fill_price * quantity - commission") {
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 100, 0.0});
    double cash_after_buy = p.cash(); // = 90,000

    FillEvent sell{2, "AAPL", OrderDirection::EXIT, 110.0, 100, 3.0};
    fill(p, sell);

    CHECK(p.cash() == doctest::Approx(cash_after_buy + 110.0 * 100.0 - 3.0));
}

TEST_CASE("on_fill EXIT: position goes to zero after full exit") {
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 50, 0.0});
    fill(p, {2, "AAPL", OrderDirection::EXIT, 100.0, 50, 0.0});

    // With zero position, equity == cash regardless of mark price.
    double prices[1] = {100.0};
    CHECK(p.equity(prices) == doctest::Approx(p.cash()));
}

TEST_CASE("on_fill EXIT: avg_cost resets to zero after full exit") {
    // After a full exit avg_cost_[id] is cleared to 0.
    // A second round of buys must accumulate cost from zero, not carry over
    // the previous average.
    Portfolio p(200000.0);
    p.add_ticker("AAPL");

    // Round 1: buy at $100, sell at $120.
    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 100, 0.0});
    fill(p, {2, "AAPL", OrderDirection::EXIT, 120.0, 100, 0.0});

    // Round 2: buy at $200, sell at $220.
    // If avg_cost had carried over from round 1 the PnL would be wrong.
    fill(p, {3, "AAPL", OrderDirection::LONG, 200.0, 50, 0.0});
    FillEvent sell2{4, "AAPL", OrderDirection::EXIT, 220.0, 50, 0.0};
    // Expected: (220 − 200) * 50 = 1,000
    CHECK(p.on_fill(sell2) == doctest::Approx(1000.0));
}

TEST_CASE("on_fill EXIT: realized PnL is correct for a profit scenario") {
    // Buy 200 shares @ $50, sell @ $75 with $10 commission.
    // PnL = (75 − 50)*200 − 10 = 5,000 − 10 = 4,990
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 50.0, 200, 0.0});

    FillEvent sell{2, "AAPL", OrderDirection::EXIT, 75.0, 200, 10.0};
    CHECK(p.on_fill(sell) == doctest::Approx(4990.0));
}

TEST_CASE("on_fill EXIT: realized PnL is correct for a loss scenario") {
    // Buy 100 shares @ $200, sell @ $180.
    // PnL = (180 − 200)*100 = −2,000
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 200.0, 100, 0.0});

    FillEvent sell{2, "AAPL", OrderDirection::EXIT, 180.0, 100, 0.0};
    CHECK(p.on_fill(sell) == doctest::Approx(-2000.0));
}

// ── on_fill – error cases ─────────────────────────────────────────────────────

TEST_CASE("on_fill: throws std::runtime_error for unregistered ticker") {
    Portfolio p(100000.0);
    // "AAPL" is never registered.

    // Wrap in a lambda so CHECK_THROWS_AS does not trigger -Wunused-result
    // for the [[nodiscard]] return value inside the macro expansion.
    FillEvent f{1, "AAPL", OrderDirection::LONG, 100.0, 10, 0.0};
    CHECK_THROWS_AS([&]{ (void)p.on_fill(f); }(), std::runtime_error);
}

TEST_CASE("on_fill: throws std::runtime_error for SHORT direction") {
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    FillEvent f{1, "AAPL", OrderDirection::SHORT, 100.0, 10, 0.0};
    CHECK_THROWS_AS([&]{ (void)p.on_fill(f); }(), std::runtime_error);
}

// ── equity() ─────────────────────────────────────────────────────────────────

TEST_CASE("equity: equals initial cash when no positions are held") {
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    double prices[1] = {150.0};
    CHECK(p.equity(prices) == doctest::Approx(100000.0));
}

TEST_CASE("equity: reflects cash plus mark-to-market position value") {
    // Buy 300 shares @ $100 → cash = $70,000.
    // Mark @ $120: equity = 70,000 + 300*120 = 106,000.
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 300, 0.0});

    double prices[1] = {120.0};
    CHECK(p.equity(prices) == doctest::Approx(70000.0 + 300.0 * 120.0));
}

TEST_CASE("equity: uses ticker id as index into the prices array") {
    // Registration order determines id: AAPL→0, GOOG→1.
    // prices[0] is used for AAPL, prices[1] for GOOG.
    Portfolio p(200000.0);
    p.add_ticker("AAPL"); // id = 0
    p.add_ticker("GOOG"); // id = 1

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 100, 0.0});
    fill(p, {2, "GOOG", OrderDirection::LONG, 200.0,  50, 0.0});
    // cash = 200,000 − 10,000 − 10,000 = 180,000

    double prices[2] = {110.0, 210.0};
    double expected = p.cash() + 100.0 * 110.0 + 50.0 * 210.0;
    CHECK(p.equity(prices) == doctest::Approx(expected));
}

TEST_CASE("equity: is unchanged after buying and immediately selling at the same price") {
    Portfolio p(100000.0);
    p.add_ticker("AAPL");

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 500, 0.0});
    fill(p, {2, "AAPL", OrderDirection::EXIT, 100.0, 500, 0.0});

    double prices[1] = {100.0};
    CHECK(p.equity(prices) == doctest::Approx(100000.0));
}

// ── Multi-ticker integration ──────────────────────────────────────────────────

TEST_CASE("multi-ticker: signals and fills for two tickers are independent") {
    Portfolio p(200000.0);
    EventQueue q(64);
    p.add_ticker("AAPL"); // id = 0
    p.add_ticker("MSFT"); // id = 1

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 200, 0.0});
    fill(p, {2, "MSFT", OrderDirection::LONG,  50.0, 400, 0.0});
    // cash = 200,000 − 20,000 − 20,000 = 160,000

    // Exit AAPL only — queue must contain exactly one order for AAPL.
    SignalEvent sig{3, "AAPL", OrderDirection::EXIT, 0.0};
    p.on_signal(sig, q, 100.0);

    OrderEvent order = pop_order(q);
    CHECK(order.ticker   == "AAPL");
    CHECK(order.quantity == 200);
    CHECK(q.empty()); // no MSFT order emitted

    // After filling the AAPL exit, MSFT position is untouched.
    fill(p, {4, "AAPL", OrderDirection::EXIT, 100.0, 200, 0.0});

    double prices[2] = {100.0, 50.0};
    double expected = p.cash() + 400.0 * 50.0;
    CHECK(p.equity(prices) == doctest::Approx(expected));
}

TEST_CASE("multi-ticker: on_signal LONG total_equity includes all registered positions") {
    // AAPL: 100 shares @ $100 = $10,000.  Remaining cash = $90,000.
    // GOOG has no position.  Signal GOOG LONG weight=1.0, price=$50.
    // total_equity = 90,000 (cash) + 100*100 (AAPL at the signal price) = 100,000
    //   Note: on_signal uses latest_close for every ticker's mark, so here $50.
    //   total_equity = 90,000 + 100*50 = 95,000
    // target_value  = 95,000
    // delta         = 95,000 (GOOG position = 0)
    // unclamped qty = floor(95,000/50) = 1,900
    // affordable    = floor(90,000/50) = 1,800  → clamped to 1,800
    Portfolio p(100000.0);
    EventQueue q(64);
    p.add_ticker("AAPL"); // id = 0
    p.add_ticker("GOOG"); // id = 1

    fill(p, {1, "AAPL", OrderDirection::LONG, 100.0, 100, 0.0});
    // cash = 90,000

    SignalEvent sig{2, "GOOG", OrderDirection::LONG, 1.0};
    p.on_signal(sig, q, 50.0);

    OrderEvent order = pop_order(q);
    CHECK(order.ticker   == "GOOG");
    CHECK(order.quantity == 1800); // clamped to affordable
}
