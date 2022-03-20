#pragma once

#include "NonCopyable.h"

template <typename T>
class ObjectPool : NonCopyable
{
public:
    ObjectPool()
    {
    }

    T GetNewObject()
    {
        if (m_FreeObjects.empty())
            return T();

        auto result = std::move(m_FreeObjects.back());
        m_FreeObjects.pop_back();
        return result;
    }

    void PoolObject(T&& object)
    {
        m_FreeObjects.push_back(std::move(object));
    }

private:
    std::vector<T> m_FreeObjects;
};