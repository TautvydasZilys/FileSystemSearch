#pragma once

#include "NonCopyable.h"

class ChildControl : NonCopyable
{
public:
    ChildControl(const wchar_t* controlClass, const wchar_t* text, DWORD style, int x, int y, int width, int height, HWND parent);
    ChildControl(ChildControl&& other);

    ~ChildControl();

    void Rescale(uint32_t dpi, HFONT font) const;

    operator HWND() const
    {
        return m_Hwnd;
    }

private:
    const int m_X, m_Y, m_Width, m_Height;
    HWND m_Hwnd;
};