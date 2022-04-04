#pragma once

#include "NonCopyable.h"

class HwndHolder
{
public:
    HwndHolder() :
        m_Hwnd(nullptr)
    {
    }

    HwndHolder(HWND hWnd) :
        m_Hwnd(hWnd)
    {
    }

    HwndHolder(HwndHolder&& other) :
        m_Hwnd(other.m_Hwnd)
    {
        other.m_Hwnd = nullptr;
    }

    ~HwndHolder()
    {
        Free();
    }

    HwndHolder& operator=(HwndHolder&& other)
    {
        std::swap(m_Hwnd, other.m_Hwnd);
    }

    HwndHolder& operator=(HWND hwnd)
    {
        Free();
        m_Hwnd = hwnd;
        return *this;
    }

    operator HWND() const
    {
        return m_Hwnd;
    }

private:
    void Free()
    {
        if (m_Hwnd != nullptr)
        {
            DestroyWindow(m_Hwnd);
            m_Hwnd = nullptr;
        }
    }

private:
    HWND m_Hwnd;
};