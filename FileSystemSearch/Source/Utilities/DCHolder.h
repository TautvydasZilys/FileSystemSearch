#pragma once

#include "NonCopyable.h"

class DCHolder : NonCopyable
{
public:
    DCHolder(HWND hWnd) :
        m_Hwnd(hWnd),
        m_DC(GetDC(hWnd))
    {
        Assert(m_DC != nullptr);
    }

    DCHolder(HWND hWnd, HRGN region, DWORD flags) :
        m_Hwnd(hWnd),
        m_DC(GetDCEx(hWnd, region, flags))
    {
        Assert(m_DC != nullptr);
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

class DCObjectSelector : NonCopyable
{
public:
    DCObjectSelector(HDC dc, HGDIOBJ objectToSelect) :
        m_DC(dc),
        m_OldSelectedObject(SelectObject(dc, objectToSelect))
    {
    }

    ~DCObjectSelector()
    {
        SelectObject(m_DC, m_OldSelectedObject);
    }

private:
    HDC m_DC;
    HGDIOBJ m_OldSelectedObject;
};