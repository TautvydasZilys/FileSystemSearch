#pragma once

#include "FileContentSearchData.h"

struct ReadBatch : NonCopyable
{
public:
    ReadBatch() :
        fenceValue(0)
    {
    }

    ReadBatch(ReadBatch&& other) :
        slots(std::move(other.slots)),
        fenceValue(other.fenceValue)
    {
    }

    ReadBatch& operator=(ReadBatch&& other)
    {
        slots = std::move(other.slots);
        fenceValue = other.fenceValue;
        return *this;
    }

public:
    std::vector<SlotSearchData> slots;
    uint64_t fenceValue;
};