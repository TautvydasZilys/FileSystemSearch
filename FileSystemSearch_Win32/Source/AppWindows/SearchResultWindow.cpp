#include "PrecompiledHeader.h"
#include "SearchResultWindow.h"

#include "CriticalSection.h"
#include "ReaderWriterLock.h"
#include "SearchEngine.h"
#include "Utilities/Control.h"
#include "Utilities/DCHolder.h"
#include "Utilities/FontCache.h"
#include "Utilities/WndClassHolder.h"

constexpr intptr_t kBackgroundColor = COLOR_WINDOW + 1;

static WndClassHolder s_SearchResultWindowClass;

static ReaderWriterLock s_SearchResultWindowsLock;
static std::map<HWND, SearchResultWindow*> s_SearchResultWindows;

static uint64_t s_SearchResultWindowCount;

constexpr int kInitialWidth = 1004;
constexpr int kInitialHeight = 542;
constexpr int kMinWidth = 500;
constexpr int kMinHeight = 300;
constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW | WS_EX_COMPOSITED;
constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
constexpr SIZE kSearchResultWindowMargins = { 20, 5 };
constexpr SIZE kProgressBarMargin = { 20, 8 };
constexpr int kStatisticsMarginFromEdgeX = 30;
constexpr int kStatisticsMarginBetweenElementsX = 40;
constexpr int kProgressBarHeight = 21;

struct SearchResultWindowArguments
{
    const FontCache& fontCache;
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

void SearchResultWindow::Spawn(const FontCache& fontCache, std::wstring&& searchPath, std::wstring&& searchPattern, std::wstring&& searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan)
{
    auto threadArgs = new SearchResultWindowArguments
    {
        .fontCache = fontCache,
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
            SearchResultWindow searchResultWindow(std::unique_ptr<SearchResultWindowArguments>(static_cast<SearchResultWindowArguments*>(args)));
            searchResultWindow.RunMessageLoop();
        }

        InterlockedDecrement(&s_SearchResultWindowCount);
        WakeByAddressAll(&s_SearchResultWindowCount);

        return 0;
    }, threadArgs, 0, nullptr);
}

SearchResultWindow::SearchResultWindow(std::unique_ptr<SearchResultWindowArguments> args) :
    m_FontCache(args->fontCache),
    m_StatisticsY(0),
    m_HasDeterminateProgress(false),
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

    m_HeaderText = L"Results for \"";
    m_HeaderText += args->searchString;
    m_HeaderText += L"\"";
    auto windowTitle = m_HeaderText + L" - FileSystemSearch";

    m_HeaderText += L" in \"";
    m_HeaderText += args->searchPath;

    if (m_HeaderText.back() != L'\\' && m_HeaderText.back() != L'/')
        m_HeaderText += '\\';

    m_HeaderText += args->searchPattern;
    m_HeaderText += L"\":";

    for (size_t i = 0; i < kStatisticsCount; i++)
    {
        m_StatisticsText[i].reserve(kStatisticsLabels[i].size() + 30);
        m_StatisticsText[i] = kStatisticsLabels[i];
    }

    auto hwnd = CreateWindowExW(kWindowExStyle, s_SearchResultWindowClass, windowTitle.c_str() , kWindowStyle, CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, GetHInstance(), this);
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
        auto waitResult = MsgWaitForMultipleObjectsEx(1, &callbackQueueEvent, INFINITE, QS_ALLINPUT | QS_ALLPOSTMESSAGE, MWMO_INPUTAVAILABLE);

        if (waitResult == WAIT_OBJECT_0)
        {
            m_CallbackQueue.InvokeCallbacks();
        }
        else if (waitResult == WAIT_OBJECT_0 + 1)
        {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                    return;

                if (!IsDialogMessageW(m_Hwnd, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
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

        case WM_SIZE:
        {
            SearchResultWindow* window = nullptr;

            {
                ReaderWriterLock::ReaderLock lock(s_SearchResultWindowsLock);
                auto it = s_SearchResultWindows.find(hWnd);
                if (it != s_SearchResultWindows.end())
                {
                    window = it->second;
                }
                else
                {
                    Assert(false);
                }
            }

            if (window != nullptr)
                window->OnResize({ LOWORD(lParam), HIWORD(lParam) }, GetDpiForWindow(hWnd));

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
    m_HeaderTextBlock = TextBlock(m_HeaderText).Create(m_Hwnd);
    m_ProgressBar = ProgressBar().Create(m_Hwnd);
    SendMessageW(m_ProgressBar, PBM_SETMARQUEE, TRUE, 0);

    SearchStatistics searchStatistics = {};
    UpdateStatisticsText(searchStatistics);

    for (size_t i = 0; i < kStatisticsCount; i++)
        m_StatisticsTextBlocks[i] = TextBlock(m_StatisticsText[i]).Create(m_Hwnd);

    auto dpi = GetDpiForWindow(hWnd);
    auto windowWidth = DipsToPixels(kInitialWidth, dpi);
    auto windowHeight = DipsToPixels(kInitialHeight, dpi);
    auto result = SetWindowPos(hWnd, nullptr, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
    Assert(result != FALSE);
}

SIZE SearchResultWindow::CalculateHeaderSize(HDC hdc, int windowWidth, int marginX)
{
    return MeasureTextSize(hdc, m_HeaderText, windowWidth - 2 * marginX).size;
}

std::array<WindowPosition, SearchResultWindow::kStatisticsCount> SearchResultWindow::CalculateStatisticsPositions(HDC hdc, int windowWidth, int marginFromEdgeX, int marginBetweenElementsX)
{
    return CalculateWrappingTextBlockPositions(hdc, m_StatisticsText, windowWidth - 2 * marginFromEdgeX, marginBetweenElementsX);
}

int SearchResultWindow::CalculateStatisticsYCoordinate(const WindowPosition& lastStatisticPosition, int windowHeight, int marginY)
{
    return windowHeight - marginY - lastStatisticPosition.y - lastStatisticPosition.cy;
}

void SearchResultWindow::OnResize(SIZE windowSize, uint32_t dpi)
{
    auto font = m_FontCache.GetFontForDpi(dpi);
    SendMessageW(m_HeaderTextBlock, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    for (HWND textBlock : m_StatisticsTextBlocks)
        SendMessageW(textBlock, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    SIZE headerTextSize;
    std::array<WindowPosition, kStatisticsCount> statisticsPositions;
    SIZE margin = DipsToPixels(kSearchResultWindowMargins, dpi);
    int statisticsMarginFromEdgeX = DipsToPixels(kStatisticsMarginFromEdgeX, dpi);
    int statisticsMarginBetweenElementsX = DipsToPixels(kStatisticsMarginBetweenElementsX, dpi);

    {
        DCHolder dcForMeasurement(m_Hwnd, font);
        headerTextSize = CalculateHeaderSize(dcForMeasurement, windowSize.cx, margin.cx);
        statisticsPositions = CalculateStatisticsPositions(dcForMeasurement, windowSize.cx, statisticsMarginFromEdgeX, statisticsMarginBetweenElementsX);
    }

    m_StatisticsY = CalculateStatisticsYCoordinate(statisticsPositions.back(), windowSize.cy, margin.cy);
    AdjustWindowPositions(statisticsPositions, { statisticsMarginFromEdgeX, m_StatisticsY });

    RepositionHeader(margin, headerTextSize);
    RepositionStatistics(statisticsPositions);
    RepositionProgressBar(windowSize.cx, dpi, m_StatisticsY);
}

void SearchResultWindow::RepositionHeader(SIZE margin, SIZE headerSize)
{
    auto result = SetWindowPos(m_HeaderTextBlock, nullptr, margin.cx, margin.cy, headerSize.cx, headerSize.cy, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
    Assert(result != FALSE);
}

void SearchResultWindow::RepositionStatistics(const std::array<WindowPosition, kStatisticsCount>& positions)
{
    for (size_t i = 0; i < kStatisticsCount; i++)
    {
        auto result = SetWindowPos(m_StatisticsTextBlocks[i], nullptr, positions[i].x, positions[i].y, positions[i].cx, positions[i].cy, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
        Assert(result != FALSE);
    }
}

void SearchResultWindow::RepositionProgressBar(int windowWidth, uint32_t dpi, int statisticsY)
{
    auto margin = DipsToPixels(kProgressBarMargin, dpi);
    int x = margin.cx;
    int width = windowWidth - 2 * margin.cx;

    int height = DipsToPixels(kProgressBarHeight, dpi);
    int y = statisticsY - margin.cy - height;

    SetWindowPos(m_ProgressBar, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
}

void SearchResultWindow::OnStatisticsUpdate(const SearchStatistics& searchStatistics, double progress)
{
    UpdateStatisticsText(searchStatistics);

    for (size_t i = 0; i < kStatisticsCount; i++)
        SetWindowTextW(m_StatisticsTextBlocks[i], m_StatisticsText[i].c_str());

    if (!isnan(progress))
    {
        if (!m_HasDeterminateProgress)
        {
            m_HasDeterminateProgress = true;

            auto progressStyle = GetWindowLongPtrW(m_ProgressBar, GWL_STYLE);
            SetWindowLongPtrW(m_ProgressBar, GWL_STYLE, progressStyle & ~PBS_MARQUEE);

            SendMessageW(m_ProgressBar, PBM_SETRANGE32, 0, std::numeric_limits<int>::max());
        }

        SendMessageW(m_ProgressBar, PBM_SETPOS, static_cast<int>(progress * std::numeric_limits<int>::max()), 0);
    }

    RECT windowRect;
    auto result = GetClientRect(m_Hwnd, &windowRect);
    Assert(result != FALSE);

    SIZE windowSize = { windowRect.right, windowRect.bottom };

    auto dpi = GetDpiForWindow(m_Hwnd);
    auto font = m_FontCache.GetFontForDpi(dpi);

    SIZE headerTextSize;
    std::array<WindowPosition, kStatisticsCount> statisticsPositions;
    SIZE margin = DipsToPixels(kSearchResultWindowMargins, dpi);
    int statisticsMarginFromEdgeX = DipsToPixels(kStatisticsMarginFromEdgeX, dpi);
    int statisticsMarginBetweenElementsX = DipsToPixels(kStatisticsMarginBetweenElementsX, dpi);

    {
        DCHolder dcForMeasurement(m_Hwnd, font);
        statisticsPositions = CalculateStatisticsPositions(dcForMeasurement, windowSize.cx, statisticsMarginFromEdgeX, statisticsMarginBetweenElementsX);
    }

    auto statisticsY = CalculateStatisticsYCoordinate(statisticsPositions.back(), windowSize.cy, margin.cy);
    if (m_StatisticsY != statisticsY)
    {
        m_StatisticsY = statisticsY;

        {
            DCHolder dcForMeasurement(m_Hwnd, font);
            headerTextSize = CalculateHeaderSize(dcForMeasurement, windowSize.cx, margin.cx);
        }

        RepositionHeader(margin, headerTextSize);
        RepositionProgressBar(windowSize.cx, dpi, m_StatisticsY);
    }

    AdjustWindowPositions(statisticsPositions, { statisticsMarginFromEdgeX, statisticsY });
    RepositionStatistics(statisticsPositions);
}

template <size_t N>
static int FormatFileSize(wchar_t (&buffer)[N], double size)
{
    if (size < 1024)
        return swprintf_s(buffer, L"%.0f bytes", size);

    size /= 1024.0;
    if (size < 1024)
        return swprintf_s(buffer, L"%.3f KB", size);

    size /= 1024.0;
    if (size < 1024)
        return swprintf_s(buffer, L"%.3f MB", size);

    size /= 1024.0;
    if (size < 1024)
        return swprintf_s(buffer, L"%.3f GB", size);

    size /= 1024.0;
    return swprintf_s(buffer, L"%.3f TB", size);
}

void SearchResultWindow::UpdateStatisticsText(const SearchStatistics& searchStatistics)
{
    wchar_t buffer[30] = {};

    const uint64_t statisticsNumbers[] =
    {
        searchStatistics.directoriesEnumerated,
        searchStatistics.filesEnumerated,
        searchStatistics.fileContentsSearched,
        searchStatistics.totalFileSize,
        static_cast<uint64_t>(searchStatistics.scannedFileSize),
        searchStatistics.resultsFound
    };

    static_assert(ARRAYSIZE(statisticsNumbers) < kStatisticsCount, "Array overrun");

    auto count = swprintf_s(buffer, L"%llu", searchStatistics.directoriesEnumerated);
    m_StatisticsText[kDirectoriesEnumerated].resize(kStatisticsLabels[kDirectoriesEnumerated].size());
    m_StatisticsText[kDirectoriesEnumerated].append(buffer, count);

    count = swprintf_s(buffer, L"%llu", searchStatistics.filesEnumerated);
    m_StatisticsText[kFilesEnumerated].resize(kStatisticsLabels[kFilesEnumerated].size());
    m_StatisticsText[kFilesEnumerated].append(buffer, count);

    count = swprintf_s(buffer, L"%llu", searchStatistics.fileContentsSearched);
    m_StatisticsText[kFileContentsSearched].resize(kStatisticsLabels[kFileContentsSearched].size());
    m_StatisticsText[kFileContentsSearched].append(buffer, count);

    count = FormatFileSize(buffer, static_cast<double>(searchStatistics.totalFileSize));
    m_StatisticsText[kTotalEnumeratedFilesSize].resize(kStatisticsLabels[kTotalEnumeratedFilesSize].size());
    m_StatisticsText[kTotalEnumeratedFilesSize].append(buffer, count);

    count = FormatFileSize(buffer, static_cast<double>(searchStatistics.scannedFileSize));
    m_StatisticsText[kTotalContentsSearchedFilesSize].resize(kStatisticsLabels[kTotalContentsSearchedFilesSize].size());
    m_StatisticsText[kTotalContentsSearchedFilesSize].append(buffer, count);

    count = swprintf_s(buffer, L"%llu", searchStatistics.resultsFound);
    m_StatisticsText[kResultsFound].resize(kStatisticsLabels[kResultsFound].size());
    m_StatisticsText[kResultsFound].append(buffer, count);

    if (searchStatistics.searchTimeInSeconds < 1000000)
    {
        swprintf_s(buffer, L"%.3f seconds", searchStatistics.searchTimeInSeconds);
    }
    else
    {
        swprintf_s(buffer, L"%.3e seconds", searchStatistics.searchTimeInSeconds);
    }

    m_StatisticsText[kSearchTime].resize(kStatisticsLabels[kSearchTime].size());
    m_StatisticsText[kSearchTime] += buffer;
}

void SearchResultWindow::OnFileFound(const WIN32_FIND_DATAW& findData, const wchar_t* path)
{
    m_CallbackQueue.InvokeAsync([findData, path { std::wstring(path) }]()
    {
        //__debugbreak();
    });
}

void SearchResultWindow::OnProgressUpdate(const SearchStatistics& searchStatistics, double progress)
{
    m_CallbackQueue.InvokeAsync([this, searchStatistics, progress]()
    {
        OnStatisticsUpdate(searchStatistics, progress);
    });
}

void SearchResultWindow::OnSearchDone(const SearchStatistics& searchStatistics)
{
    m_CallbackQueue.InvokeAsync([this, searchStatistics]()
    {
        if (!m_IsTearingDown)
        {
            OnStatisticsUpdate(searchStatistics, 1.0);
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
