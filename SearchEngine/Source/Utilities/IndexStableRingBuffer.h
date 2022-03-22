#pragma once
#include "HeapArray.h"
#include "NonCopyable.h"

template <typename T, typename TSize>
class IndexStableRingBuffer : NonCopyable
{
public:
    IndexStableRingBuffer() :
        m_Capacity(0),
        m_TailIndex(0),
        m_Head(0),
        m_Tail(0)
    {
    }

    ~IndexStableRingBuffer()
    {
        if (m_Tail >= m_Head)
        {
            for (TSize i = m_Head; i < m_Tail; i++)
                GetPointer(i)->~T();
        }
        else
        {
            for (TSize i = m_Head; i < m_Capacity; i++)
                GetPointer(i)->~T();

            for (TSize i = 0; i < m_Tail; i++)
                GetPointer(i)->~T();
        }
    }

    bool IsEmpty() const
    {
        return Size() == 0;
    }

    T& operator[](TSize index)
    {
        Assert(Size() > 0);

        const auto indexFromEnd = m_TailIndex - index;
        if (m_Tail >= m_Head || indexFromEnd <= m_Tail)
            return *GetPointer(m_Tail - indexFromEnd);

        return *GetPointer(m_Capacity + m_Tail - indexFromEnd);
    }

    TSize Size() const
    {
        if (m_Tail >= m_Head)
            return m_Tail - m_Head;

        return m_Tail + m_Capacity - m_Head;
    }

    T& Front()
    {
        Assert(Size() > 0);
        return *GetPointer(m_Head);
    }

    T& Back()
    {
        Assert(Size() > 0);
        return *GetPointer(m_Tail - 1);
    }

    TSize BackIndex() const
    {
        Assert(Size() > 0);
        return m_TailIndex - 1;
    }

    TSize PushBack(T&& item)
    {
        if (m_Tail >= m_Capacity)
        {
            if (m_Head < 2)
            {
                Grow();
            }
            else
            {
                m_Tail = 0;
            }
        }
        else if (m_Tail == m_Head - 1)
        {
            Grow();
        }

        new (GetPointer(m_Tail++)) T(std::move(item));
        return m_TailIndex++;
    }

    void PopFront()
    {
        GetPointer(m_Head)->~T();
        m_Head++;

        if (m_Head == m_Tail)
        {
            m_Head = m_Tail = 0;
        }
        else if (m_Head == m_Capacity)
        {
            m_Head = 0;
        }
    }

private:
    inline static void TransferT(uint8_t* srcBuffer, TSize srcIndex, uint8_t* dstBuffer, TSize dstIndex)
    {
        auto oldT = reinterpret_cast<T*>(&srcBuffer[srcIndex * sizeof(T)]);
        auto destination = &dstBuffer[dstIndex * sizeof(T)];
        new (destination) T(std::move(*oldT));
        oldT->~T();
    }

    inline void Grow()
    {
        const auto newCapacity = m_Capacity > 0 ? m_Capacity * 2 : 4;
        const auto size = Size();
        std::unique_ptr<uint8_t[]> newBuffer(new uint8_t[newCapacity * sizeof(T)]);

        if (m_Tail >= m_Head)
        {
            TSize dstIndex = 0;
            for (TSize i = m_Head; i < m_Tail; i++, dstIndex++)
                TransferT(m_Buffer.get(), i, newBuffer.get(), dstIndex);
        }
        else
        {
            TSize dstIndex = 0;
            for (TSize i = m_Head; i < m_Capacity; i++, dstIndex++)
                TransferT(m_Buffer.get(), i, newBuffer.get(), dstIndex);

            for (TSize i = 0; i < m_Tail; i++, dstIndex++)
                TransferT(m_Buffer.get(), i, newBuffer.get(), dstIndex);
        }

        m_Buffer = std::move(newBuffer);
        m_Capacity = newCapacity;
        m_Head = 0;
        m_Tail = size;
    }

private:
    T* GetPointer(TSize index)
    {
        return reinterpret_cast<T*>(&m_Buffer[index * sizeof(T)]);
    }

    const T* GetPointer(TSize index) const
    {
        return reinterpret_cast<const T*>(&m_Buffer[index * sizeof(T)]);
    }

private:
    std::unique_ptr<uint8_t[]> m_Buffer;
    TSize m_Capacity;
    TSize m_Head;
    TSize m_Tail;
    TSize m_TailIndex;
};