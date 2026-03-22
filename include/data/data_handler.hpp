#pragma once
#include <fstream>
#include <string>
#include "events/events.hpp"
#include "events/event_queue.hpp"

class DataHandler {
public:
    DataHandler(const std::string& csv_path, const std::string& ticker);
    ~DataHandler();

    // Lazy iterator: reads one CSV line and pushes a MarketEvent onto queue.
    // Returns false when the file is exhausted.
    bool stream_next_event(EventQueue& queue);

    bool is_active() const;

private:
    std::ifstream file_;
    std::string   line_;
    std::string   ticker_;
    bool          active_;

    MarketEvent parse_csv_line(const std::string& line);
};
