#include "PrecompiledHeader.h"
#include "SearchResultWindow.h"

#include "CriticalSection.h"
#include "ReaderWriterLock.h"
#include "SearchEngine.h"
#include "Utilities/WndClassHolder.h"

constexpr intptr_t kBackgroundColor = COLOR_WINDOW + 1;

static WndClassHolder s_SearchResultWindowClass;

static ReaderWriterLock s_SearchResultWindowsLock;
static std::map<HWND, SearchResultWindow*> s_SearchResultWindows;

uint64_t s_SearchResultWindowCount;

constexpr int kInitialWidth = 1000;
constexpr int kInitialHeight = 540;
constexpr int kMinWidth = 400;
constexpr int kMinHeight = 200;

SearchResultWindow::StaticInitializer::StaticInitializer()
{
    WNDCLASSEXW classDescription = {};
    classDescription.cbSize = sizeof(classDescription);
    classDescription.style = CS_HREDRAW | CS_VREDRAW;
    classDescription.lpfnWndProc = SearchResultWindow::WndProc;
    classDescription.hInstance = GetHInstance();
    classDescription.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    classDescription.hbrBackground = reinterpret_cast<HBRUSH>(kBackgroundColor);
    classDescription.lpszClassName = L"SearchResultWindow";

    s_SearchResultWindowClass = RegisterClassExW(&classDescription);
    Assert(s_SearchResultWindowClass != 0);
}

SearchResultWindow::StaticInitializer::~StaticInitializer()
{
    for (;;)
    {
        auto currentCount = s_SearchResultWindowCount;
        if (currentCount == 0)
            break;
       
        auto result = WaitOnAddress(&s_SearchResultWindowCount, &currentCount, sizeof(currentCount), INFINITE);
        Assert(result != FALSE);
    }

    Assert(s_SearchResultWindows.empty());
    s_SearchResultWindowClass = 0;
}

void SearchResultWindow::Spawn(const std::wstring& searchPath, const std::wstring& searchPattern, const std::wstring& searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan)
{
    auto fileSearch = ::Search(
        [](const WIN32_FIND_DATAW* /*findData*/, const wchar_t* path) { std::wstring result = L"Path found: '"; result += path; result += L"'\r\n"; OutputDebugStringW(result.c_str()); },
        [](const SearchStatistics& /*searchStatistics*/, double /*progress*/) {},
        [](const SearchStatistics& /*searchStatistics*/) { OutputDebugStringW(L"Search done!\r\n"); },
        [](const wchar_t* errorMessage) { MessageBoxW(nullptr, errorMessage, L"Error", MB_OK | MB_ICONERROR); },
        searchPath.c_str(),
        searchPattern.c_str(),
        searchString.c_str(),
        searchFlags,
        ignoreFilesLargerThan);

    InterlockedIncrement(&s_SearchResultWindowCount);
    ThreadHandleHolder threadHandle = CreateThread(nullptr, 0, [](void* fileSearch) -> DWORD
    {
        {
            SearchResultWindow searchResultWindow;

            for (;;)
            {
                MSG msg;
                auto getMessageResult = GetMessageW(&msg, nullptr, 0, 0);
                Assert(getMessageResult != -1);

                if (getMessageResult == 0 || getMessageResult == -1)
                    break;

                if (!IsDialogMessageW(searchResultWindow.m_Hwnd, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }

            CleanupSearchOperation(static_cast<FileSearcher*>(fileSearch));
        }

        InterlockedDecrement(&s_SearchResultWindowCount);
        WakeByAddressAll(&s_SearchResultWindowCount);

        return 0;
    }, fileSearch, 0, nullptr);
}

SearchResultWindow::SearchResultWindow()
{
    Assert(s_SearchResultWindowClass != 0);

    constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
    constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    auto hwnd = CreateWindowExW(kWindowExStyle, s_SearchResultWindowClass, L"File System Search - Results", kWindowStyle, CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, GetHInstance(), this);
    Assert(m_Hwnd == hwnd);

    auto wasVisible = ShowWindow(m_Hwnd, SW_SHOW);
    Assert(wasVisible == FALSE);
}

SearchResultWindow::~SearchResultWindow()
{
    ReaderWriterLock::WriterLock lock(s_SearchResultWindowsLock);
    s_SearchResultWindows.erase(m_Hwnd);
}

LRESULT SearchResultWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            auto window = static_cast<SearchResultWindow*>(createStruct->lpCreateParams);
            window->OnCreate(hWnd);
            return 0;
        }

        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;

        case WM_CTLCOLORSTATIC:
            return kBackgroundColor;

        case WM_DPICHANGED:
        {
            auto targetRect = reinterpret_cast<RECT*>(lParam);

            auto result = SetWindowPos(hWnd, nullptr, targetRect->left, targetRect->top, targetRect->right - targetRect->left, targetRect->bottom - targetRect->top, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
            Assert(result != FALSE);            

            return 0;
        }

        case WM_GETMINMAXINFO:
        {
            auto dpi = GetDpiForWindow(hWnd);
            auto minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
            minMaxInfo->ptMinTrackSize.x = DipsToPixels(kMinWidth, dpi);
            minMaxInfo->ptMinTrackSize.y = DipsToPixels(kMinHeight, dpi);
            return 0;
        }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SearchResultWindow::OnCreate(HWND hWnd)
{
    {
        ReaderWriterLock::WriterLock lock(s_SearchResultWindowsLock);
        s_SearchResultWindows.emplace(hWnd, this);
    }

    m_Hwnd = hWnd;

    auto dpi = GetDpiForWindow(hWnd);
    auto result = SetWindowPos(hWnd, nullptr, 0, 0, DipsToPixels(kInitialWidth, dpi), DipsToPixels(kInitialHeight, dpi), SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
    Assert(result != FALSE);
}

