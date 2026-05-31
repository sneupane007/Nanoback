#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/event_queue.hpp"
#include "include/events.hpp"

// ── EventQueue tests ──────────────────────────────────────────────────────────
// The queue is a fixed-capacity ring buffer. Key behaviours to verify:
//   - push/pop round-trip preserves event data
//   - empty() reflects queue state correctly
//   - popping past tail wraps correctly (ring behaviour)
//   - overflow (push beyond capacity) throws or wraps as designed

TEST_CASE("EventQueue: starts empty") {
    EventQueue q(16);
    CHECK(q.empty());
}

TEST_CASE("EventQueue: push then pop returns same event") {
    EventQueue q(16);
    MarketEvent me{1000, "AAPL", 150.0, 155.0, 149.0, 152.0, 1000000};
    q.push(me);
    CHECK(!q.empty());
    Event out = q.pop();
    CHECK(q.empty());
    // std::get throws std::bad_variant_access if the type is wrong
    auto& got = std::get<MarketEvent>(out);
    CHECK(got.ticker == "AAPL");
    CHECK(got.close  == doctest::Approx(152.0));
}

TEST_CASE("EventQueue: FIFO ordering") {
    EventQueue q(16);
    q.push(MarketEvent{1, "A", 1,1,1,1,1});
    q.push(MarketEvent{2, "B", 2,2,2,2,2});
    auto first  = std::get<MarketEvent>(q.pop());
    auto second = std::get<MarketEvent>(q.pop());
    CHECK(first.timestamp  == 1);
    CHECK(second.timestamp == 2);
}

TEST_CASE("EventQueue: ring wrap-around") {
    // Fill and drain the queue twice to exercise wrap-around
    EventQueue q(4);
    for (int round = 0; round < 2; ++round) {
        for (int i = 0; i < 4; ++i)
            q.push(MarketEvent{static_cast<uint64_t>(i), "X", 0,0,0,0,0});
        for (int i = 0; i < 4; ++i) {
            auto e = std::get<MarketEvent>(q.pop());
            CHECK(e.timestamp == static_cast<uint64_t>(i));
        }
        CHECK(q.empty());
    }
}
