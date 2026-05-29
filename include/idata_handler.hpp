#pragma once
#include "event_queue.hpp"

// Abstract interface for all data sources.
// Any concrete handler must implement stream_next_event() (push one bar onto
// the queue and return true) and is_active() (false when exhausted/stopped).
// This mirrors the IStrategy pattern: main.cpp and LiveSession hold a
// std::unique_ptr<IDataHandler> so the data source is swappable at runtime.
class IDataHandler {
public:
    virtual ~IDataHandler() = default;
    virtual bool stream_next_event(EventQueue& queue) = 0;
    virtual bool is_active() const = 0;
};
