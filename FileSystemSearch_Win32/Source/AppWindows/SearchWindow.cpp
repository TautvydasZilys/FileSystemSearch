#include "PrecompiledHeader.h"
#include "SearchWindow.h"
#include "Utilities/FontCache.h"
#include "Utilities/WindowUtilities.h"

static SearchWindow* s_Instance;
static HINSTANCE GetHInstance();

constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
constexpr DWORD kWindowStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
constexpr int kWindowClientWidth = 409;
constexpr int kWindowClientHeight = 639;

constexpr intptr_t kBackgroundColor = COLOR_WINDOW + 1;

enum class ByteUnit
{
    B,
    KB,
    MB,
    GB,
    TB
};

struct NamedByteUnit
{
    constexpr NamedByteUnit(ByteUnit unit, const wchar_t* name) :
        unit(unit), name(name)
    {
    }

    ByteUnit unit;
    const wchar_t* name;
};

constexpr NamedByteUnit kByteUnits[] =
{
    { ByteUnit::B, L"B" },
    { ByteUnit::KB, L"KB" },
    { ByteUnit::MB, L"MB" },
    { ByteUnit::GB, L"GB" },
    { ByteUnit::TB, L"TB" },
};

SearchWindow::SearchWindow(const FontCache& fontCache, int nCmdShow) :
    m_FontCache(fontCache)
{
    Assert(s_Instance == nullptr); // Current design dictates only one Search Window gets spawned per app instance
    s_Instance = this;

    WNDCLASSEXW classDescription = {};
    classDescription.cbSize = sizeof(classDescription);
    classDescription.style = CS_HREDRAW | CS_VREDRAW;
    classDescription.lpfnWndProc = SearchWindow::WndProc;
    classDescription.hInstance = GetHInstance();
    classDescription.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    classDescription.hbrBackground = reinterpret_cast<HBRUSH>(kBackgroundColor);
    classDescription.lpszClassName = L"SearchWindow";

    m_WindowClass = RegisterClassExW(&classDescription);
    Assert(m_WindowClass != 0);

    auto hwnd = CreateWindowExW(kWindowExStyle, reinterpret_cast<LPCWSTR>(m_WindowClass), L"File System Search - Search", kWindowStyle, CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, GetHInstance(), nullptr);
    Assert(m_Hwnd == hwnd);

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
            s_Instance->OnCreate(hWnd);

            auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            s_Instance->AdjustSearchWindowPlacement(createStruct->x, createStruct->y, GetDpiForWindow(hWnd));
            return 0;
        }

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_CTLCOLORSTATIC:
            return kBackgroundColor;

        case WM_DPICHANGED:
        {
            auto targetRect = reinterpret_cast<RECT*>(lParam);
            s_Instance->AdjustSearchWindowPlacement(targetRect->left, targetRect->top, HIWORD(wParam));

            return 0;
        }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}


void SearchWindow::OnCreate(HWND hWnd)
{
    m_Hwnd = hWnd;

    for (size_t i = 0; i < m_ControlCount; i++)
        m_Controls[i] = kControls[i].Create(hWnd, i);
    
    HWND byteUnitComboBox = m_Controls[m_IgnoreFileLargerThanUnitComboBox];
    for (size_t i = 0; i < ARRAYSIZE(kByteUnits); i++)
    {
        SendMessageW(byteUnitComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kByteUnits[i].name));
        SendMessageW(byteUnitComboBox, CB_SETITEMDATA, i, static_cast<LPARAM>(kByteUnits[i].unit));
    }

    SendMessageW(byteUnitComboBox, CB_SETCURSEL, 2 /* MB */, 0);

    SendMessageW(m_Controls[m_SearchForFilesCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_SearchInFileNameCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_SearchRecursivelyCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_IgnoreCaseCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_IgnoreFilesStartingWithDotCheckBox], BM_SETCHECK, BST_CHECKED, 0);
}

void SearchWindow::AdjustSearchWindowPlacement(int positionX, int positionY, uint32_t dpi)
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

    result = SetWindowPos(m_Hwnd, nullptr, positionX, positionY, adjustedWindowRect.right - adjustedWindowRect.left, adjustedWindowRect.bottom - adjustedWindowRect.top, SWP_NOACTIVATE | SWP_NOZORDER);
    Assert(result != FALSE);

    auto font = m_FontCache.GetFontForDpi(dpi);
    for (size_t i = 0; i < m_ControlCount; i++)
    {
        HWND childHwnd = m_Controls[i];
        Assert(childHwnd != nullptr);

        const auto& desc = kControls[i];
        auto x = DipsToPixels(desc.x, dpi);
        auto y = DipsToPixels(desc.y, dpi);
        auto width = DipsToPixels(desc.width, dpi);
        auto height = DipsToPixels(desc.height, dpi);

        result = SetWindowPos(childHwnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
        Assert(result != FALSE);

        SendMessageW(childHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}
