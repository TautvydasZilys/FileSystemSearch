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
constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

struct SearchArguments
{
    std::wstring searchPath;
    std::wstring searchPattern;
    std::wstring searchString;
    SearchFlags searchFlags;
    uint64_t ignoreFilesLargerThan;
};

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

void SearchResultWindow::Spawn(std::wstring&& searchPath, std::wstring&& searchPattern, std::wstring&& searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan)
{
    auto threadArgs = new SearchArguments
    {
        .searchPath = std::move(searchPath),
        .searchPattern = std::move(searchPattern),
        .searchString = std::move(searchString),
        .searchFlags = searchFlags,
        .ignoreFilesLargerThan = ignoreFilesLargerThan
    };

    InterlockedIncrement(&s_SearchResultWindowCount);
    ThreadHandleHolder threadHandle = CreateThread(nullptr, 0, [](void* args) -> DWORD
    {
        SetThreadDescription(GetCurrentThread(), L"FSS Search Result Window Thread");

        {
            SearchResultWindow searchResultWindow(std::unique_ptr<SearchArguments>(static_cast<SearchArguments*>(args)));
            searchResultWindow.RunMessageLoop();
        }

        InterlockedDecrement(&s_SearchResultWindowCount);
        WakeByAddressAll(&s_SearchResultWindowCount);

        return 0;
    }, threadArgs, 0, nullptr);
}

SearchResultWindow::SearchResultWindow(std::unique_ptr<SearchArguments> args) :
    m_SearcherCleanupState(SearcherCleanupState::NotCleanedUp),
    m_IsTearingDown(false)
{
    Assert(s_SearchResultWindowClass != 0);

    m_Searcher = ::Search(
        [](void* _this, const WIN32_FIND_DATAW& findData, const wchar_t* path) { static_cast<SearchResultWindow*>(_this)->OnFileFound(findData, path); },
        [](void* _this, const SearchStatistics& searchStatistics, double progress) { static_cast<SearchResultWindow*>(_this)->OnProgressUpdate(searchStatistics, progress); },
        [](void* _this, const SearchStatistics& searchStatistics) { static_cast<SearchResultWindow*>(_this)->OnSearchDone(searchStatistics); },
        [](void* _this, const wchar_t* errorMessage) { static_cast<SearchResultWindow*>(_this)->OnSearchError(errorMessage); },
        args->searchPath.c_str(),
        args->searchPattern.c_str(),
        args->searchString.c_str(),
        args->searchFlags,
        args->ignoreFilesLargerThan,
        this);

    auto hwnd = CreateWindowExW(kWindowExStyle, s_SearchResultWindowClass, L"File System Search - Results", kWindowStyle, CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, GetHInstance(), this);
    Assert(m_Hwnd == hwnd);

    auto wasVisible = ShowWindow(m_Hwnd, SW_SHOW);
    Assert(wasVisible == FALSE);
}

SearchResultWindow::~SearchResultWindow()
{
    m_IsTearingDown = true;

    CleanupSearchOperationIfNeeded();

    {
        ReaderWriterLock::WriterLock lock(s_SearchResultWindowsLock);
        s_SearchResultWindows.erase(m_Hwnd);
    }

    while (m_SearcherCleanupState != SearcherCleanupState::CleanedUp)
    {
        auto inProgress = SearcherCleanupState::CleanupInProgress;
        WaitOnAddress(&m_SearcherCleanupState, &inProgress, sizeof(inProgress), INFINITE);
    }
}

void SearchResultWindow::RunMessageLoop()
{
    const auto callbackQueueEvent = m_CallbackQueue.GetCallbackAvailableEvent();

    for (;;)
    {
        auto waitResult = MsgWaitForMultipleObjectsEx(1, &callbackQueueEvent, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);

        if (waitResult == WAIT_OBJECT_0)
        {
            m_CallbackQueue.InvokeCallbacks();
        }
        else if (waitResult == WAIT_OBJECT_0 + 1)
        {
            MSG msg;
            auto getMessageResult = GetMessageW(&msg, nullptr, 0, 0);
            Assert(getMessageResult != -1);

            if (getMessageResult == 0 || getMessageResult == -1)
                break;

            if (!IsDialogMessageW(m_Hwnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
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
            ShowWindow(hWnd, SW_HIDE);
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

void SearchResultWindow::OnFileFound(const WIN32_FIND_DATAW& findData, const wchar_t* path)
{
    m_CallbackQueue.InvokeAsync([findData, path { std::wstring(path) }]()
    {
        __debugbreak();
    });
}

void SearchResultWindow::OnProgressUpdate(const SearchStatistics& searchStatistics, double progress)
{
    m_CallbackQueue.InvokeAsync([searchStatistics, progress]()
    {
        __debugbreak();
    });
}

void SearchResultWindow::OnSearchDone(const SearchStatistics& searchStatistics)
{
    m_CallbackQueue.InvokeAsync([this, searchStatistics]()
    {
        if (!m_IsTearingDown)
        {
            // Don't bother with UI updates if we're tearing down, as nobody will see the result.
            __debugbreak();
        }

        CleanupSearchOperationIfNeeded();
    });
}

void SearchResultWindow::OnSearchError(const wchar_t* errorMessage)
{
    m_CallbackQueue.InvokeAsync([this, errorMessage { std::wstring(errorMessage) }]()
    {
        MessageBoxW(m_Hwnd, errorMessage.c_str(), L"Error", MB_OK | MB_ICONERROR);
        CleanupSearchOperationIfNeeded();
    });

}

void SearchResultWindow::CleanupSearchOperationIfNeeded()
{
    if (m_SearcherCleanupState != SearcherCleanupState::NotCleanedUp)
        return;

    m_SearcherCleanupState = SearcherCleanupState::CleanupInProgress;

    QueueUserWorkItem([](void* arg) -> DWORD
    {
        auto _this = static_cast<SearchResultWindow*>(arg);
        CleanupSearchOperation(_this->m_Searcher);
        _this->m_Searcher = nullptr;
        _this->m_SearcherCleanupState = SearcherCleanupState::CleanedUp;
        WakeByAddressAll(&_this->m_SearcherCleanupState);
        return 0;
    }, this, WT_EXECUTELONGFUNCTION);
}
