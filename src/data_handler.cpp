#include "include/data_handler.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>
#include <charconv>

void DataHandler::parse_header(const std::string& header) {
    static const std::unordered_map<std::string, int> alias_map = {
        {"date",           0}, {"timestamp",      0}, {"time",          0},
        {"datetime",       0}, {"open_time",       0},
        {"open",           1},
        {"high",           2},
        {"low",            3},
        {"close",          4}, {"adj close",       4}, {"adj_close",     4},
        {"adjusted_close", 4},
        {"volume",         5}, {"vol",             5},
    };

    std::vector<std::string> tokens;
    size_t start = 0;
    for (size_t i = 0; i <= header.size(); ++i) {
        if (i == header.size() || header[i] == ',') {
            std::string tok = header.substr(start, i - start);
            if (!tok.empty() && tok.back() == '\r') tok.pop_back();
            std::transform(tok.begin(), tok.end(), tok.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            const size_t s = tok.find_first_not_of(' ');
            const size_t e = tok.find_last_not_of(' ');
            if (s != std::string::npos) tok = tok.substr(s, e - s + 1);
            tokens.push_back(std::move(tok));
            start = i + 1;
        }
    }

    for (int idx = 0; idx < static_cast<int>(tokens.size()); ++idx) {
        auto it = alias_map.find(tokens[idx]);
        if (it == alias_map.end()) continue;
        switch (it->second) {
            case 0: col_date_  = idx; break;
            case 1: col_open_  = idx; break;
            case 2: col_high_  = idx; break;
            case 3: col_low_   = idx; break;
            case 4: col_close_ = idx; break;
            case 5: col_vol_   = idx; break;
        }
    }
}

DataHandler::DataHandler(const std::string& csv_path, const std::string& ticker)
    : ticker_(ticker), active_(true)
{
    file_.open(csv_path);
    if (!file_.is_open()) {
        active_ = false;
    } else {
        const auto t0 = std::chrono::high_resolution_clock::now();
        
        std::getline(file_, line_);
        parse_header(line_);
        
        header_align_ms_ = std::chrono::duration<double, std::nano>(
            std::chrono::high_resolution_clock::now() - t0).count();
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
MarketEvent DataHandler::parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    fields.reserve(8);  
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            std::string f = line.substr(start, i - start);
            if (!f.empty() && f.back() == '\r') f.pop_back(); 
            fields.push_back(std::move(f));
            start = i + 1;
        }
    }

    MarketEvent m{};
    m.ticker = ticker_;

    const int n = static_cast<int>(fields.size());
    const int max_idx = std::max({col_date_, col_open_, col_high_,
                                  col_low_,  col_close_, col_vol_});

    auto parse_d = [](const std::string& s, double&   out) {
        std::from_chars(s.data(), s.data() + s.size(), out);
    };
    auto parse_u = [](const std::string& s, uint64_t& out) {
        std::from_chars(s.data(), s.data() + s.size(), out);
    };

    if (col_date_ < n) {
        const std::string& ds = fields[col_date_];
        
        uint64_t raw = 0;
        auto [ptr, ec] = std::from_chars(ds.data(), ds.data() + ds.size(), raw);
        
        if (ec == std::errc{} && ptr == ds.data() + ds.size() && raw > 1000000000ULL) {
            m.timestamp = raw;
        } else if (ds.size() >= 10 && ds[4] == '-' && ds[7] == '-') {
            
            int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
            std::from_chars(ds.data(),     ds.data() + 4,  y);
            std::from_chars(ds.data() + 5, ds.data() + 7,  mo);
            std::from_chars(ds.data() + 8, ds.data() + 10, d);
            if (ds.size() >= 19 && (ds[10] == ' ' || ds[10] == 'T')) {
                std::from_chars(ds.data() + 11, ds.data() + 13, h);
                std::from_chars(ds.data() + 14, ds.data() + 16, mi);
                std::from_chars(ds.data() + 17, ds.data() + 19, sec);
                
                if (ds.size() >= 25 && (ds[19] == '+' || ds[19] == '-')) {
                    int tz_h = 0, tz_m = 0;
                    std::from_chars(ds.data() + 20, ds.data() + 22, tz_h);
                    std::from_chars(ds.data() + 23, ds.data() + 25, tz_m);
                    int offset_sec = tz_h * 3600 + tz_m * 60;
                    if (ds[19] == '-') offset_sec = -offset_sec;
                    
                    sec -= offset_sec;
                }
            }
            // days_from_civil (Howard Hinnant, public domain): converts a
            // proleptic-Gregorian (y, m, d) to days since 1970-01-01 with a
            // handful of integer ops — no libc, no locale, no timegm. This
            // replaces the per-bar timegm() call that profiling showed was
            // ~90% of CSV parse time.
            auto days_from_civil = [](int yr, unsigned mo_, unsigned dy) -> long long {
                yr -= mo_ <= 2;
                const long long era = (yr >= 0 ? yr : yr - 399) / 400;
                const unsigned  yoe = static_cast<unsigned>(yr - era * 400);      // [0, 399]
                const unsigned  doy = (153 * (mo_ + (mo_ > 2 ? -3 : 9)) + 2) / 5 + dy - 1; // [0, 365]
                const unsigned  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;      // [0, 146096]
                return era * 146097 + static_cast<long long>(doe) - 719468;
            };
            const long long days = days_from_civil(y, static_cast<unsigned>(mo),
                                                       static_cast<unsigned>(d));
            const long long t = days * 86400LL + h * 3600LL + mi * 60LL + sec;
            if (t >= 0) m.timestamp = static_cast<uint64_t>(t);
        }
    }
    if (max_idx  < n) {
        parse_d(fields[col_open_],  m.open);
        parse_d(fields[col_high_],  m.high);
        parse_d(fields[col_low_],   m.low);
        parse_d(fields[col_close_], m.close);
        parse_u(fields[col_vol_],   m.volume);
    }

    return m;
}