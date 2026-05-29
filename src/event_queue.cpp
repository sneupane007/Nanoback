#include "include/event_queue.hpp"
#include <stdexcept>

// Constructor: This is where we pay the memory "tax" upfront.
EventQueue::EventQueue(size_t cap) : head(0), tail(0), capacity(cap) {
    buffer.resize(cap); 
}

void EventQueue::push(Event event) {
    
    if (count == capacity) {
        throw std::runtime_error("EventQueue overflow! Strategy is generating events faster than processing.");
    }
    buffer[tail] = std::move(event);
    tail = (tail + 1) % capacity;
    ++count;
}

Event EventQueue::pop() {
    
    if (empty()) {
        throw std::runtime_error("Attempted to pop from an empty EventQueue.");
    }

    
    Event event = std::move(buffer[head]);

   
    head = (head + 1) % capacity;
    --count;

    return event;
}

bool EventQueue::empty() const {
    return count == 0;
}