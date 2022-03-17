#pragma once

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
    std::vector<uint16_t> slots;
    uint64_t fenceValue;
};