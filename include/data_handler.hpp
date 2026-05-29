#pragma once
#include <fstream>
#include <string>
#include "events.hpp"
#include "event_queue.hpp"
#include "idata_handler.hpp"

class DataHandler : public IDataHandler {
public:
    DataHandler(const std::string& csv_path, const std::string& ticker);
    ~DataHandler();

    // Lazy iterator: reads one CSV line and pushes a MarketEvent onto queue.
    // Returns false when the file is exhausted.
    bool stream_next_event(EventQueue& queue) override;

    bool   is_active()        const override;
    double header_align_ms()  const { return header_align_ms_; }

private:
    std::ifstream file_;
    std::string   line_;
    std::string   ticker_;
    bool          active_;
    double        header_align_ms_ = 0.0;

    // Column positions detected from the CSV header (defaults = positional 0–5).
    int col_date_  = 0;
    int col_open_  = 1;
    int col_high_  = 2;
    int col_low_   = 3;
    int col_close_ = 4;
    int col_vol_   = 5;

    void        parse_header(const std::string& header_line);
    MarketEvent parse_csv_line(const std::string& line);
};
