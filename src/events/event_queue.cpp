#include "include/events/event_queue.hpp"
#include <stdexcept>

// Constructor: This is where we pay the memory "tax" upfront.
EventQueue::EventQueue(size_t cap) : head(0), tail(0), capacity(cap) {
    buffer.resize(cap); 
}

void EventQueue::push(Event event) {
    // Check if the queue is full.
    // In a ring buffer, it's full if the tail "catches" the head.
    if ((tail + 1) % capacity == head) {
        throw std::runtime_error("EventQueue overflow! Strategy is generating events faster than processing.");
    }

    // Move the event into the pre-allocated slot — avoids copying the std::string ticker.
    buffer[tail] = std::move(event);

    // Advance the tail using the Modulo Operator to wrap around.
    tail = (tail + 1) % capacity;
}

Event EventQueue::pop() {
    // Check if the queue is empty.
    if (empty()) {
        throw std::runtime_error("Attempted to pop from an empty EventQueue.");
    }

    // Move the event out of the slot — avoids copying the std::string ticker.
    Event event = std::move(buffer[head]);

    // Advance the head using the Modulo Operator.
    head = (head + 1) % capacity;

    return event;
}

bool EventQueue::empty() const {
    // In a circular buffer, if head == tail, there is no unread data.
    return head == tail;
}