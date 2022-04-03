#include "PrecompiledHeader.h"
#include "SearchWindow.h"
#include "Utilities/WindowUtilities.h"

static SearchWindow* s_Instance;
static HINSTANCE GetHInstance();

constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
constexpr DWORD kWindowStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
constexpr int kWindowClientWidth = 409;
constexpr int kWindowClientHeight = 639;

SearchWindow::SearchWindow(int nCmdShow)
{
    Assert(s_Instance == nullptr); // Current design dictates only one Search Window gets spawned per app instance
    s_Instance = this;

    WNDCLASSEXW classDescription = {};
    classDescription.cbSize = sizeof(classDescription);
    classDescription.style = CS_HREDRAW | CS_VREDRAW;
    classDescription.lpfnWndProc = SearchWindow::WndProc;
    classDescription.hInstance = GetHInstance();
    classDescription.hCursor = LoadCursorW(GetHInstance(), IDC_ARROW);
    classDescription.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    classDescription.lpszClassName = L"SearchWindow";

    m_WindowClass = RegisterClassExW(&classDescription);
    Assert(m_WindowClass != 0);

    m_Hwnd = CreateWindowExW(kWindowExStyle, reinterpret_cast<LPCWSTR>(m_WindowClass), L"File System Search - Search", kWindowStyle, CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, GetHInstance(), nullptr);
    Assert(m_Hwnd != nullptr);

    auto wasVisible = ShowWindow(m_Hwnd, nCmdShow);
    Assert(wasVisible == FALSE);
}

SearchWindow::~SearchWindow()
{
    DestroyWindow(m_Hwnd);
    UnregisterClassW(reinterpret_cast<LPCWSTR>(m_WindowClass), GetHInstance());

    Assert(s_Instance == this);
    s_Instance = nullptr;
}

LRESULT SearchWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            AdjustSearchWindowPlacement(hWnd, createStruct->x, createStruct->y, GetDpiForWindow(hWnd));
            return 0;
        }

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_DPICHANGED:
        {
            auto targetRect = reinterpret_cast<RECT*>(lParam);
            AdjustSearchWindowPlacement(hWnd, targetRect->left, targetRect->top, HIWORD(wParam));
            return 0;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void SearchWindow::AdjustSearchWindowPlacement(HWND hWnd, int positionX, int positionY, uint32_t dpi)
{
    RECT adjustedWindowRect =
    {
        0,
        0,
        DipsToPixels(kWindowClientWidth, dpi),
        DipsToPixels(kWindowClientHeight, dpi),
    };

    auto result = AdjustWindowRectExForDpi(&adjustedWindowRect, kWindowStyle, FALSE, kWindowExStyle, dpi);
    Assert(result != FALSE);

    result = SetWindowPos(hWnd, nullptr, positionX, positionY, adjustedWindowRect.right - adjustedWindowRect.left, adjustedWindowRect.bottom - adjustedWindowRect.top, SWP_NOACTIVATE | SWP_NOZORDER);
    Assert(result != FALSE);
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
static HINSTANCE GetHInstance()
{
    return reinterpret_cast<HINSTANCE>(&__ImageBase);
}