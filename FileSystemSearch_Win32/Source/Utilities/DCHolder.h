#pragma once

#include "NonCopyable.h"

class DCHolder : NonCopyable
{
public:
    DCHolder(HWND hWnd) :
        m_Hwnd(hWnd),
        m_DC(GetDC(hWnd))
    {
    }

    ~DCHolder()
    {
        ReleaseDC(m_Hwnd, m_DC);
    }

    operator HDC() const
    {
        return m_DC;
    }

private:
    HWND m_Hwnd;
    HDC m_DC;
};