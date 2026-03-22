#include "include/data/data_handler.hpp"
#include <vector>
#include <stdexcept>

DataHandler::DataHandler(const std::string& csv_path, const std::string& ticker)
    : ticker_(ticker), active_(true)
{
    file_.open(csv_path);
    if (!file_.is_open()) {
        active_ = false;
    } else {
        std::getline(file_, line_); // skip header
    }
}

DataHandler::~DataHandler() {
    if (file_.is_open()) file_.close();
}

bool DataHandler::stream_next_event(EventQueue& queue) {
    if (std::getline(file_, line_)) {
        queue.push(parse_csv_line(line_));
        return true;
    }
    active_ = false;
    return false;
}

bool DataHandler::is_active() const { return active_; }

// Manual comma scan — avoids constructing a std::stringstream per bar.
// Expected columns (0-based): Date, Open, High, Low, Close, Volume
MarketEvent DataHandler::parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    fields.reserve(6);

    size_t start = 0;
    for (size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            fields.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }

    MarketEvent m{};
    m.ticker = ticker_;

    // Date field: try numeric first; fall back to 0 for non-numeric dates.
    try {
        m.timestamp = std::stoull(fields[0]);
    } catch (...) {
        m.timestamp = 0;
    }

    if (fields.size() >= 6) {
        try { m.open   = std::stod(fields[1]); } catch (...) { m.open   = 0.0; }
        try { m.high   = std::stod(fields[2]); } catch (...) { m.high   = 0.0; }
        try { m.low    = std::stod(fields[3]); } catch (...) { m.low    = 0.0; }
        try { m.close  = std::stod(fields[4]); } catch (...) { m.close  = 0.0; }
        try { m.volume = std::stoull(fields[5]); } catch (...) { m.volume = 0; }
    }

    return m;
}
