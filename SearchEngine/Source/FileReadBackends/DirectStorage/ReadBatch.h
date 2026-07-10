#pragma once

#include "Event.h"
#include "FileContentSearchData.h"

struct ReadBatch : NonCopyable
{
public:
    ReadBatch() :
        completionEvent(false)
    {
    }

    ReadBatch(ReadBatch&& other) :
        slots(std::move(other.slots)),
        completionEvent(std::move(other.completionEvent))
    {
    }

    ReadBatch& operator=(ReadBatch&& other)
    {
        slots = std::move(other.slots);
        completionEvent = std::move(other.completionEvent);
        return *this;
    }

public:
    std::vector<SlotSearchData> slots;
    Event<EventType::AutoReset> completionEvent;
};