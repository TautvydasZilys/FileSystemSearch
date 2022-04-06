#pragma once

#include "NonCopyable.h"

class DCHolder : NonCopyable
{
public:
    DCHolder(HWND hWnd, HGDIOBJ objectToSelect) :
        m_Hwnd(hWnd),
        m_DC(GetDC(hWnd)),
        m_OldSelectedObject(SelectObject(m_DC, objectToSelect))
    {
    }

    ~DCHolder()
    {
        SelectObject(m_DC, m_OldSelectedObject);
        ReleaseDC(m_Hwnd, m_DC);
    }

    operator HDC() const
    {
        return m_DC;
    }

private:
    HWND m_Hwnd;
    HDC m_DC;
    HGDIOBJ m_OldSelectedObject;
};