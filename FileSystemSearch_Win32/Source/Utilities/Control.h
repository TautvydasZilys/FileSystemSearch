#pragma once

#include "WindowUtilities.h"

struct ControlDescription
{
    constexpr ControlDescription(const wchar_t* controlClass, const wchar_t* text, DWORD style, int x, int y, int width, int height) :
        controlClass(controlClass),
        text(text),
        style(style),
        x(x),
        y(y),
        width(width),
        height(height)
    {
    }

    HWND Create(HWND parent, size_t controlIndex = 0) const
    {
        auto hWnd = CreateWindowExW(0, controlClass, text, style, CW_USEDEFAULT, 0, 0, 0, parent, reinterpret_cast<HMENU>(controlIndex), GetHInstance(), nullptr);
        Assert(hWnd != nullptr);
        return hWnd;
    }

    const wchar_t* controlClass;
    const wchar_t* text;
    const DWORD style;
    const int x, y, width, height;
};

constexpr ControlDescription TextBlock(const wchar_t* text, int x = 0, int y = 0, int width = 0)
{
    return ControlDescription(WC_STATIC, text, WS_VISIBLE | WS_CHILD, x, y, width, 16);
}

constexpr ControlDescription TextBlock(const std::wstring& text, int x = 0, int y = 0, int width = 0)
{
    return TextBlock(text.c_str(), x, y, width);
}

constexpr ControlDescription TextBox(const wchar_t* text, int x, int y, int width)
{
    return ControlDescription(WC_EDIT, text, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, x, y, width, 19);
}

constexpr ControlDescription ComboBox(int x, int y, int width)
{
    return ControlDescription(WC_COMBOBOX, L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, x, y, width, 23);
}

constexpr ControlDescription CheckBox(const wchar_t* text, int x, int y, int width)
{
    return ControlDescription(WC_BUTTON, text, WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, width, 16);
}

constexpr ControlDescription Button(const wchar_t* text, int x, int y, int width)
{
    return ControlDescription(WC_BUTTON, text, WS_VISIBLE | WS_CHILD | WS_TABSTOP, x, y, width, 26);
}

constexpr ControlDescription ProgressBar(int x = 0, int y = 0, int width = 0, int height = 0)
{
    return ControlDescription(PROGRESS_CLASS, nullptr, WS_VISIBLE | WS_CHILD | PBS_MARQUEE, x, y, width, height);
}

constexpr ControlDescription ChildWindow(LPCWSTR windowClass, int extraStyles, int x = 0, int y = 0, int width = 0, int height = 0)
{
    Assert(windowClass != nullptr);
    return ControlDescription(windowClass, nullptr, WS_VISIBLE | WS_CHILD | extraStyles, x, y, width, height);
}