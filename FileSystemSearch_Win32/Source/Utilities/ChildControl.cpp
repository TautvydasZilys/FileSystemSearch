#include "PrecompiledHeader.h"
#include "ChildControl.h"
#include "FontCache.h"
#include "WindowUtilities.h"

ChildControl::ChildControl(const wchar_t* controlClass, const wchar_t* text, DWORD style, int x, int y, int width, int height, HWND parent) :
    m_X(x),
    m_Y(y),
    m_Width(width),
    m_Height(height),
    m_Hwnd(CreateWindowExW(0, controlClass, text, style, CW_USEDEFAULT, 0, 0, 0, parent, nullptr, GetHInstance(), nullptr))
{
    Assert(m_Hwnd != nullptr);
}

ChildControl::ChildControl(ChildControl&& other) :
    m_X(other.m_X),
    m_Y(other.m_Y),
    m_Width(other.m_Width),
    m_Height(other.m_Height),
    m_Hwnd(other.m_Hwnd)
{
    other.m_Hwnd = nullptr;
}

ChildControl::~ChildControl()
{
    if (m_Hwnd != nullptr)
        DestroyWindow(m_Hwnd);
}

void ChildControl::Rescale(uint32_t dpi, HFONT font) const
{
    Assert(m_Hwnd != nullptr);

    auto x = DipsToPixels(m_X, dpi);
    auto y = DipsToPixels(m_Y, dpi);
    auto width = DipsToPixels(m_Width, dpi);
    auto height = DipsToPixels(m_Height, dpi);

    auto result = SetWindowPos(m_Hwnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
    Assert(result != FALSE);

    SendMessageW(m_Hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}
