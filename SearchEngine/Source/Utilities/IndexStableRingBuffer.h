#pragma once
#include "HeapArray.h"
#include "NonCopyable.h"

template <typename T, typename TSize>
class IndexStableRingBuffer : NonCopyable
{
public:
    IndexStableRingBuffer() :
        m_Capacity(0),
        m_NextIndex(0),
        m_Head(0),
        m_Tail(0)
    {
    }

    ~IndexStableRingBuffer()
    {
        if (m_Tail >= m_Head)
        {
            for (TSize i = m_Head; i < m_Tail; i++)
                GetElementPointer(i)->~Element();
        }
        else
        {
            for (TSize i = m_Head; i < m_Capacity; i++)
                GetElementPointer(i)->~Element();

            for (TSize i = 0; i < m_Tail; i++)
                GetElementPointer(i)->~Element();
        }
    }

    bool IsEmpty() const
    {
        return Size() == 0;
    }

    T& operator[](TSize index)
    {
        Assert(Size() > 0);
        Assert(GetElementPointer(m_Head)->index <= index);
        Assert(GetElementPointer(m_Tail - 1)->index >= index);

        Element* begin;
        Element* end;

        if (m_Tail >= m_Head)
        {
            begin = GetElementPointer(m_Head);
            end = GetElementPointer(m_Tail);
        }
        else if (GetElementPointer(0)->index > index)
        {
            begin = GetElementPointer(m_Head);
            end = GetElementPointer(m_Capacity);
        }
        else
        {
            begin = GetElementPointer(0);
            end = GetElementPointer(m_Tail);
        }

        auto result = std::lower_bound(begin, end, index, [](const Element& element, TSize index)
        {
            return element.index < index;
        });

        Assert(result != GetElementPointer(m_Tail));
        Assert(result->index == index);
        return result->item;
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
        return GetElementPointer(m_Head)->item;
    }

    T& Back()
    {
        Assert(Size() > 0);
        return GetElementPointer(m_Tail - 1)->item;
    }

    TSize BackIndex() const
    {
        Assert(Size() > 0);
        return GetElementPointer(m_Tail - 1)->index;
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

        auto index = m_NextIndex++;
        new (GetElementPointer(m_Tail++)) Element(std::move(item), index);
        return index;
    }

    void PopFront()
    {
        GetElementPointer(m_Head)->~Element();
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
    inline static void TransferElement(uint8_t* srcBuffer, TSize srcIndex, uint8_t* dstBuffer, TSize dstIndex)
    {
        auto oldElement = reinterpret_cast<Element*>(&srcBuffer[srcIndex * sizeof(Element)]);
        auto destination = &dstBuffer[dstIndex * sizeof(Element)];
        new (destination) Element(std::move(*oldElement));
        oldElement->~Element();
    }

    inline void Grow()
    {
        const auto newCapacity = m_Capacity > 0 ? m_Capacity * 2 : 4;
        const auto size = Size();
        std::unique_ptr<uint8_t[]> newBuffer(new uint8_t[newCapacity * sizeof(Element)]);

        if (m_Tail >= m_Head)
        {
            TSize dstIndex = 0;
            for (TSize i = m_Head; i < m_Tail; i++, dstIndex++)
                TransferElement(m_Buffer.get(), i, newBuffer.get(), dstIndex);
        }
        else
        {
            TSize dstIndex = 0;
            for (TSize i = m_Head; i < m_Capacity; i++, dstIndex++)
                TransferElement(m_Buffer.get(), i, newBuffer.get(), dstIndex);

            for (TSize i = 0; i < m_Tail; i++, dstIndex++)
                TransferElement(m_Buffer.get(), i, newBuffer.get(), dstIndex);
        }

        m_Buffer = std::move(newBuffer);
        m_Capacity = newCapacity;
        m_Head = 0;
        m_Tail = size;
    }

private:
    struct Element : NonCopyable
    {
        T item;
        TSize index;

        Element(T item, TSize index) :
            item(std::move(item)),
            index(index)
        {
        }

        Element(Element&& other) :
            item(std::move(other.item)),
            index(other.index)
        {
        }

        Element& operator=(Element&& other)
        {
            item = std::move(other.item);
            index = other.index;
            return *this;
        }
    };

    Element* GetElementPointer(TSize index)
    {
        return reinterpret_cast<Element*>(&m_Buffer[index * sizeof(Element)]);
    }

    const Element* GetElementPointer(TSize index) const
    {
        return reinterpret_cast<const Element*>(&m_Buffer[index * sizeof(Element)]);
    }

private:
    std::unique_ptr<uint8_t[]> m_Buffer;
    TSize m_Capacity;
    TSize m_Head;
    TSize m_Tail;
    TSize m_NextIndex;
};