#include "include/proto_data_handler.hpp"
#include "include/events.hpp"
#include "ohlcv.pb.h"
#include <string>

ProtoDataHandler::ProtoDataHandler(const std::string& pb_path, const std::string& ticker)
    : ticker_(ticker)
{
    file_.open(pb_path, std::ios::binary);
    if (!file_.is_open())
        active_ = false;
}

bool ProtoDataHandler::is_active() const { return active_; }

// Reads one protobuf varint from file_ into out.
// Returns false if the file is exhausted before a full varint is read.
bool ProtoDataHandler::read_varint(uint32_t& out) {
    out = 0;
    int shift = 0;
    uint8_t byte;
    do {
        if (!file_.read(reinterpret_cast<char*>(&byte), 1)) return false;
        out |= static_cast<uint32_t>(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return true;
}

bool ProtoDataHandler::stream_next_event(EventQueue& queue) {
    uint32_t len = 0;
    if (!read_varint(len)) {
        active_ = false;
        return false;
    }

    std::string buf(len, '\0');
    if (!file_.read(buf.data(), len)) {
        active_ = false;
        return false;
    }

    nanoback::OhlcvBar bar;
    if (!bar.ParseFromString(buf)) {
        active_ = false;
        return false;
    }

    MarketEvent m{};
    m.timestamp = bar.timestamp();
    m.ticker    = ticker_;
    m.open      = bar.open();
    m.high      = bar.high();
    m.low       = bar.low();
    m.close     = bar.close();
    m.volume    = bar.volume();

    queue.push(m);
    return true;
}
