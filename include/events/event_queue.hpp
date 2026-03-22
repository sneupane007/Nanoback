#pragma once
#include "events.hpp"
#include <vector>

class EventQueue {
public:
    // Constructor pre-allocates the 1,000,000 slots
    EventQueue(size_t capacity = 1000000); 

    void push(Event event);
    Event pop();
    bool empty() const;

private:
    std::vector<Event> buffer; // The contiguous memory block
    size_t head = 0;           // Where we read from
    size_t tail = 0;           // Where we write to
    size_t capacity;           // Fixed size
};