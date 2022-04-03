#pragma once

#include "NonCopyable.h"

class SearchWindow : NonCopyable
{
public:
    SearchWindow(int nCmdShow);
    ~SearchWindow();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static void AdjustSearchWindowPlacement(HWND hWnd, int positionX, int positionY, uint32_t dpi);

private:
    ATOM m_WindowClass;
    HWND m_Hwnd;
};
