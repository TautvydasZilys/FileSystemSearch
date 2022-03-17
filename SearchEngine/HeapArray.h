#pragma once

template <typename T>
class HeapArray : NonCopyable
{
public:
    HeapArray() :
        m_Size(0)
    {
    }

    HeapArray(size_t size) :
        m_Data(new T[size]),
        m_Size(size)
    {
    }

    HeapArray(HeapArray&& other) :
        m_Data(std::move(other.m_Data)),
        m_Size(other.m_Size)
    {
    }

    HeapArray& operator=(HeapArray&& other)
    {
        std::swap(m_Data, other.m_Data);
        std::swap(m_Size, other.m_Size);
        return *this;
    }

    T& operator[](size_t index)
    {
        Assert(index < m_Size);
        return m_Data[index];
    }

    const T& operator[](size_t index) const
    {
        Assert(index < m_Size);
        return m_Data[index];
    }

    T* data()
    {
        return m_Data.get();
    }

    const T* data() const
    {
        return m_Data.get();
    }

    T* begin()
    {
        return data();
    }

    T* end()
    {
        return data() + m_Size;
    }

    const T* begin() const
    {
        return data();
    }

    const T* end() const
    {
        return data() + m_Size;
    }

    size_t size() const
    {
        return m_Size;
    }

    bool empty() const
    {
        return size() == 0;
    }

private:
    std::unique_ptr<T[]> m_Data;
    size_t m_Size;
};