#pragma once
#include <fstream>
#include <string>
#include "idata_handler.hpp"

class ProtoDataHandler : public IDataHandler {
public:
    ProtoDataHandler(const std::string& pb_path, const std::string& ticker);
    bool stream_next_event(EventQueue& queue) override;
    bool is_active() const override;
private:
    std::ifstream file_;
    std::string   ticker_;
    bool          active_ = true;
    bool read_varint(uint32_t& out);
};
