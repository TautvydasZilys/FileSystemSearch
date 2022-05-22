#pragma once

#include "NonCopyable.h"
#include "WindowUtilities.h"

class WndClassHolder
{
public:
    WndClassHolder() :
        m_Atom(0)
    {
    }

    WndClassHolder(WndClassHolder&& other) :
        m_Atom(other.m_Atom)
    {
        other.m_Atom = 0;
    }

    WndClassHolder& operator=(WndClassHolder&& other)
    {
        std::swap(m_Atom, other.m_Atom);
    }

    WndClassHolder& operator=(ATOM hwnd)
    {
        Free();
        m_Atom = hwnd;
        return *this;
    }

    operator ATOM() const
    {
        return m_Atom;
    }

    operator LPCWSTR() const
    {
        return reinterpret_cast<LPCWSTR>(m_Atom);
    }

private:
    void Free()
    {
        if (m_Atom != 0)
        {
            UnregisterClassW(*this, GetHInstance());
            m_Atom = 0;
        }
    }

private:
    ATOM m_Atom;
};