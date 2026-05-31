#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/data_handler.hpp"
#include "include/event_queue.hpp"
#include "include/events.hpp"

#include <fstream>
#include <sstream>

// Helper: write a temporary CSV file and return its path.
// (Placed in an anonymous namespace so it's local to this translation unit.)
namespace {
std::string write_temp_csv(const std::string& contents, const std::string& name = "/tmp/nanoback_test.csv") {
    std::ofstream f(name);
    f << contents;
    return name;
}
} // namespace

// ── DataHandler tests ─────────────────────────────────────────────────────────
// DataHandler lazily streams CSV rows into MarketEvents.
// Key behaviours:
//   - Well-formed CSV produces correct MarketEvent fields
//   - Empty CSV (header only) yields no events
//   - Malformed rows are skipped or throw — verify design intent

TEST_CASE("DataHandler: streams a single valid row") {
    auto path = write_temp_csv(
        "timestamp,open,high,low,close,volume\n"
        "1000,100.0,110.0,90.0,105.0,500000\n"
    );
    DataHandler dh(path, "AAPL");
    EventQueue q(16);

    bool had_event = dh.stream_next_event(q);
    CHECK(had_event);

    auto e  = std::get<MarketEvent>(q.pop());
    CHECK(e.ticker == "AAPL");
    CHECK(e.open   == doctest::Approx(100.0));
    CHECK(e.close  == doctest::Approx(105.0));
    CHECK(e.volume == 500000u);
}

TEST_CASE("DataHandler: returns false when CSV is exhausted") {
    auto path = write_temp_csv(
        "timestamp,open,high,low,close,volume\n"
        "1000,100.0,110.0,90.0,105.0,500000\n"
    );
    DataHandler dh(path, "AAPL");
    EventQueue q(16);

    CHECK(dh.stream_next_event(q));   // first row
    CHECK(!dh.stream_next_event(q));  // EOF
}

TEST_CASE("DataHandler: header-only CSV yields no events") {
    auto path = write_temp_csv("timestamp,open,high,low,close,volume\n");
    DataHandler dh(path, "AAPL");
    EventQueue q(16);
    CHECK(!dh.stream_next_event(q));
}

TEST_CASE("DataHandler: multiple rows are emitted in order") {
    auto path = write_temp_csv(
        "timestamp,open,high,low,close,volume\n"
        "1,10,11,9,10.5,100\n"
        "2,11,12,10,11.5,200\n"
        "3,12,13,11,12.5,300\n"
    );
    DataHandler dh(path, "TEST");
    EventQueue q(32);

    for (int i = 0; i < 3; ++i) CHECK(dh.stream_next_event(q));
    CHECK(!dh.stream_next_event(q));

    auto e1 = std::get<MarketEvent>(q.pop());
    auto e2 = std::get<MarketEvent>(q.pop());
    auto e3 = std::get<MarketEvent>(q.pop());
    CHECK(e1.timestamp == 1u);
    CHECK(e2.timestamp == 2u);
    CHECK(e3.timestamp == 3u);
}
