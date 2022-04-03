#include "PrecompiledHeader.h"
#include "SearchWindow.h"
#include "Utilities/ChildControl.h"
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
    m_Children.clear();

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
    for (const auto& child : m_Children)
        child.Rescale(dpi, font);
}

static ChildControl TextBlock(const wchar_t* text, int x, int y, int width, HWND parent)
{
    return ChildControl(WC_STATIC, text, WS_VISIBLE | WS_CHILD, x, y, width, 16, parent);
}

static ChildControl TextBox(const wchar_t* text, int x, int y, int width, HWND parent)
{
    return ChildControl(WC_EDIT, text, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP, x, y, width, 19, parent);
}

static ChildControl CheckBox(const wchar_t* text, int x, int y, int width, HWND parent, bool checked)
{
    ChildControl control(WC_BUTTON, text, WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, width, 16, parent);

    if (checked)
        SendMessageW(control, BM_SETCHECK, BST_CHECKED, 0);

    return control;
}

void SearchWindow::OnCreate(HWND hWnd)
{
    m_Hwnd = hWnd;

    m_Children.push_back(TextBlock(L"Search Path", 40, 11, 320, hWnd));
    m_Children.push_back(TextBox(L"C:\\", 40, 30, 320, hWnd));

    m_Children.push_back(TextBlock(L"Search Pattern", 40, 71, 320, hWnd));
    m_Children.push_back(TextBox(L"*", 40, 90, 320, hWnd));

    m_Children.push_back(TextBlock(L"Search String", 40, 131, 320, hWnd));
    m_Children.push_back(TextBox(L"", 40, 150, 320, hWnd));

    m_Children.push_back(TextBlock(L"Ignore files larger than", 40, 191, 190, hWnd));
    m_Children.push_back(TextBox(L"10", 40, 210, 190, hWnd));

    m_Children.emplace_back(WC_COMBOBOX, L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 208, 45, 23, hWnd);
    
    HWND byteUnitComboBox = m_Children.back();

    for (size_t i = 0; i < ARRAYSIZE(kByteUnits); i++)
    {
        SendMessageW(byteUnitComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kByteUnits[i].name));
        SendMessageW(byteUnitComboBox, CB_SETITEMDATA, i, static_cast<LPARAM>(kByteUnits[i].unit));
    }

    SendMessageW(byteUnitComboBox, CB_SETCURSEL, 2 /* MB */, 0);

    m_Children.push_back(CheckBox(L"Search for files", 41, 240, 320, hWnd, true));
    m_Children.push_back(CheckBox(L"Search in file path", 41, 260, 320, hWnd, false));
    m_Children.push_back(CheckBox(L"Search in file name", 41, 280, 320, hWnd, true));
    m_Children.push_back(CheckBox(L"Search in file contents", 41, 300, 320, hWnd, false));
    m_Children.push_back(CheckBox(L"Search file contents as UTF8", 41, 320, 320, hWnd, false));
    m_Children.push_back(CheckBox(L"Search file contents as UTF16", 41, 340, 320, hWnd, false));

    m_Children.push_back(CheckBox(L"Search for directories", 41, 380, 320, hWnd, false));
    m_Children.push_back(CheckBox(L"Search in directory path", 41, 400, 320, hWnd, false));
    m_Children.push_back(CheckBox(L"Search in directory name", 41, 420, 320, hWnd, false));

    m_Children.push_back(CheckBox(L"Search recursively", 41, 460, 320, hWnd, true));
    m_Children.push_back(CheckBox(L"Ignore case", 41, 480, 320, hWnd, true));
    m_Children.push_back(CheckBox(L"Ignore files and folders starting with '.'", 41, 500, 320, hWnd, true));

    m_Children.push_back(CheckBox(L"Use DirectStorage for reading files", 41, 540, 320, hWnd, false));

    m_Children.emplace_back(WC_BUTTON, L"Search!", WS_VISIBLE | WS_CHILD | WS_TABSTOP, 40, 580, 320, 26, hWnd);
}

