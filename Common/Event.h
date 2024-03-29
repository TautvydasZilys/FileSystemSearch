#pragma once
#include "HandleHolder.h"

enum class EventType
{
    AutoReset,
    ManualReset
};

template <EventType eventType>
class Event : public EventHandleHolder
{
public:
    Event(bool initialize = true) :
        HandleHolder(initialize ? CreateEventW(nullptr, eventType == EventType::ManualReset ? TRUE : FALSE, FALSE, nullptr) : nullptr)
    {
        if (initialize)
            Assert(*this);
    }

    Event(Event&& other) :
        EventHandleHolder(std::move(other))
    {
    }

    Event& operator=(Event&& other)
    {
        return EventHandleHolder::operator=(std::move(other));
    }

    void Initialize()
    {
        Assert(!*this);
        EventHandleHolder::operator=(CreateEventW(nullptr, eventType == EventType::ManualReset ? TRUE : FALSE, FALSE, nullptr));
    }

    void Set()
    {
        auto result = SetEvent(*this);
        Assert(result != false);
    }

    void Reset()
    {
        auto result = ResetEvent(*this);
        Assert(result != false);
    }
};
