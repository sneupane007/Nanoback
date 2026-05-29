#pragma once
#include "events.hpp"
#include <vector>

class EventQueue {
public:
    EventQueue(size_t capacity = 1024);

    void   push(Event event);
    Event  pop();
    bool   empty() const;
    size_t peak_depth() const { return peak_count_; }

private:
    std::vector<Event> buffer;
    size_t head       = 0;
    size_t tail       = 0;
    size_t capacity;
    size_t count      = 0;
    size_t peak_count_ = 0;
};