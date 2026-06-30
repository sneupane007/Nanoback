#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/proto_data_handler.hpp"
#include "include/event_queue.hpp"
#include "include/events.hpp"
#include "ohlcv.pb.h"

#include <fstream>
#include <string>

namespace {

// Write a length-delimited protobuf stream to a temp file.
// Returns the file path.
std::string write_temp_pb(const std::vector<nanoback::OhlcvBar>& bars,
                          const std::string& path = "/tmp/nanoback_test.pb")
{
    std::ofstream f(path, std::ios::binary);
    for (const auto& bar : bars) {
        std::string data = bar.SerializeAsString();
        uint32_t len = static_cast<uint32_t>(data.size());
        // Write varint length prefix (same format read by ProtoDataHandler)
        while (true) {
            uint8_t byte = len & 0x7F;
            len >>= 7;
            if (len) byte |= 0x80;
            f.write(reinterpret_cast<char*>(&byte), 1);
            if (!len) break;
        }
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    return path;
}

nanoback::OhlcvBar make_bar(uint64_t ts, double o, double h, double lo,
                            double c, uint64_t vol)
{
    nanoback::OhlcvBar b;
    b.set_timestamp(ts);
    b.set_open(o);
    b.set_high(h);
    b.set_low(lo);
    b.set_close(c);
    b.set_volume(vol);
    return b;
}

} // namespace

TEST_CASE("ProtoDataHandler: streams a single valid bar") {
    auto path = write_temp_pb({ make_bar(1000, 100.0, 110.0, 90.0, 105.0, 500000) });
    ProtoDataHandler ph(path, "AAPL");
    EventQueue q(16);

    CHECK(ph.stream_next_event(q));

    auto e = std::get<MarketEvent>(q.pop());
    CHECK(e.ticker    == "AAPL");
    CHECK(e.timestamp == 1000u);
    CHECK(e.open      == doctest::Approx(100.0));
    CHECK(e.high      == doctest::Approx(110.0));
    CHECK(e.low       == doctest::Approx(90.0));
    CHECK(e.close     == doctest::Approx(105.0));
    CHECK(e.volume    == 500000u);
}

TEST_CASE("ProtoDataHandler: returns false on empty file") {
    auto path = write_temp_pb({}, "/tmp/nanoback_test_empty.pb");
    ProtoDataHandler ph(path, "TEST");
    EventQueue q(16);
    CHECK(!ph.stream_next_event(q));
    CHECK(!ph.is_active());
}

TEST_CASE("ProtoDataHandler: returns false on missing file") {
    ProtoDataHandler ph("/tmp/no_such_file_xyz.pb", "TEST");
    EventQueue q(16);
    CHECK(!ph.is_active());
    CHECK(!ph.stream_next_event(q));
}

TEST_CASE("ProtoDataHandler: multiple bars emitted in order") {
    auto path = write_temp_pb({
        make_bar(1, 10.0, 11.0,  9.0, 10.5, 100),
        make_bar(2, 11.0, 12.0, 10.0, 11.5, 200),
        make_bar(3, 12.0, 13.0, 11.0, 12.5, 300),
    });
    ProtoDataHandler ph(path, "TEST");
    EventQueue q(32);

    for (int i = 0; i < 3; ++i) CHECK(ph.stream_next_event(q));
    CHECK(!ph.stream_next_event(q));

    auto e1 = std::get<MarketEvent>(q.pop());
    auto e2 = std::get<MarketEvent>(q.pop());
    auto e3 = std::get<MarketEvent>(q.pop());
    CHECK(e1.timestamp == 1u);
    CHECK(e2.timestamp == 2u);
    CHECK(e3.timestamp == 3u);
    CHECK(e3.close == doctest::Approx(12.5));
}
