#pragma once

#include "NonCopyable.h"

class ChildControl;
class FontCache;

class SearchWindow : NonCopyable
{
public:
    SearchWindow(const FontCache& fontCache, int nCmdShow);
    ~SearchWindow();

    operator HWND() const
    {
        return m_Hwnd;
    }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void AdjustSearchWindowPlacement(int positionX, int positionY, uint32_t dpi);
    void OnCreate(HWND hWnd);

private:
    const FontCache& m_FontCache;
    ATOM m_WindowClass;
    HWND m_Hwnd;

    std::vector<ChildControl> m_Children;
};
