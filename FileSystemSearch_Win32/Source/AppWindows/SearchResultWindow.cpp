#include "PrecompiledHeader.h"
#include "SearchResultWindow.h"

#include "CriticalSection.h"
#include "ReaderWriterLock.h"
#include "SearchEngine.h"
#include "SearchResultsView.h"
#include "Utilities/Control.h"
#include "Utilities/DCHolder.h"
#include "Utilities/FontCache.h"
#include "Utilities/WndClassHolder.h"

constexpr intptr_t kBackgroundColor = COLOR_WINDOW + 1;

static WndClassHolder s_SearchResultWindowClass;
static WndClassHolder s_ExplorerBrowserHostWindowClass;

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

    classDescription.lpfnWndProc = DefWindowProcW;
    classDescription.lpszClassName = L"SearchResultExplorerBrowserHost";

    s_ExplorerBrowserHostWindowClass = RegisterClassExW(&classDescription);
    Assert(s_ExplorerBrowserHostWindowClass != 0);

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
    s_ExplorerBrowserHostWindowClass = 0;
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
    m_ExplorerWindow(nullptr),
    m_StatisticsY(0),
    m_HasDeterminateProgress(false),
    m_SearcherCleanupState(SearcherCleanupState::NotCleanedUp),
    m_SearchStatistics({}),
    m_Progress(0.0),
    m_Initialized(false),
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

    m_Initialized = true;
    WakeByAddressAll(&m_Initialized);
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

    if (m_ExplorerWindow != nullptr)
    {
        DestroyView(m_ExplorerWindow);
        m_ExplorerWindow = nullptr;
    }
}

void SearchResultWindow::RunMessageLoop()
{
    const auto callbackQueueEvent = m_CallbackQueue.GetCallbackAvailableEvent();

    for (;;)
    {
        auto waitResult = MsgWaitForMultipleObjectsEx(1, &callbackQueueEvent, INFINITE, QS_ALLINPUT | QS_ALLPOSTMESSAGE, MWMO_INPUTAVAILABLE);

        if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_OBJECT_0 + 1)
        {
            m_CallbackQueue.InvokeCallbacks();

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

static SearchResultWindow* GetWindowInstance(HWND hWnd)
{
    ReaderWriterLock::ReaderLock lock(s_SearchResultWindowsLock);
    auto it = s_SearchResultWindows.find(hWnd);
    if (it != s_SearchResultWindows.end())
    {
        return it->second;
    }

    Assert(false);
    return nullptr;
}

LRESULT CALLBACK SearchResultWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

        case WM_SETTINGCHANGE:
        {
            if (wParam == 0 && lParam != 0 && wcscmp(L"intl", reinterpret_cast<const wchar_t*>(lParam)) == 0)
            {
                SearchResultWindow* window = GetWindowInstance(hWnd);
                if (window != nullptr)
                {
                    window->OnLocaleChange();
                    return 0;
                }
            }

            break;
        }

        case WM_SIZE:
        {
            if (wParam != SIZE_MINIMIZED)
            {
                SearchResultWindow* window = GetWindowInstance(hWnd);
                if (window != nullptr)
                {
                    window->OnResize({ LOWORD(lParam), HIWORD(lParam) }, GetDpiForWindow(hWnd));
                    return 0;
                }
            }

            break;
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

    m_ExplorerBrowserHost = ChildWindow(s_ExplorerBrowserHostWindowClass, WS_CLIPCHILDREN).Create(m_Hwnd);
    m_ExplorerWindow = CreateDpiAwareView(m_ExplorerBrowserHost, 0, 0);
    InitializeView(m_ExplorerWindow);

    m_ProgressBar = ProgressBar().Create(m_Hwnd);
    SendMessageW(m_ProgressBar, PBM_SETMARQUEE, TRUE, 0);

    UpdateStatisticsText();

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
        DCHolder dcForMeasurement(m_Hwnd);
        DCObjectSelector fontSelector(dcForMeasurement, font);
        headerTextSize = CalculateHeaderSize(dcForMeasurement, windowSize.cx, margin.cx);
        statisticsPositions = CalculateStatisticsPositions(dcForMeasurement, windowSize.cx, statisticsMarginFromEdgeX, statisticsMarginBetweenElementsX);
    }

    m_StatisticsY = CalculateStatisticsYCoordinate(statisticsPositions.back(), windowSize.cy, margin.cy);
    AdjustWindowPositions(statisticsPositions, { statisticsMarginFromEdgeX, m_StatisticsY });

    RepositionHeader(margin, headerTextSize);
    RepositionStatistics(statisticsPositions);

    auto progressBarY = RepositionProgressBar(windowSize.cx, dpi, m_StatisticsY);
    RepositionExplorerBrowser(margin, windowSize.cx, headerTextSize.cy, progressBarY);
}

void SearchResultWindow::OnLocaleChange()
{
    m_NumberFormatter.RefreshLocaleInfo();
    OnStatisticsUpdate();
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

int SearchResultWindow::RepositionProgressBar(int windowWidth, uint32_t dpi, int statisticsY)
{
    auto margin = DipsToPixels(kProgressBarMargin, dpi);
    int x = margin.cx;
    int width = windowWidth - 2 * margin.cx;

    int height = DipsToPixels(kProgressBarHeight, dpi);
    int y = statisticsY - margin.cy - height;

    SetWindowPos(m_ProgressBar, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
    return y;
}

void SearchResultWindow::RepositionExplorerBrowser(SIZE margin, int windowWidth, int headerHeight, int progressBarY)
{
    auto x = margin.cx;
    auto width = windowWidth - 2 * x;

    auto headerEndY = margin.cy + headerHeight;
    auto y = headerEndY + margin.cy;
    auto height = progressBarY - margin.cy - y;
    SetWindowPos(m_ExplorerBrowserHost, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);    
    ResizeView(m_ExplorerWindow, width, height);
}

void SearchResultWindow::OnStatisticsUpdate()
{
    UpdateStatisticsText();

    for (size_t i = 0; i < kStatisticsCount; i++)
        SetWindowTextW(m_StatisticsTextBlocks[i], m_StatisticsText[i].c_str());

    if (!isnan(m_Progress))
    {
        if (!m_HasDeterminateProgress)
        {
            m_HasDeterminateProgress = true;

            auto progressStyle = GetWindowLongPtrW(m_ProgressBar, GWL_STYLE);
            SetWindowLongPtrW(m_ProgressBar, GWL_STYLE, progressStyle & ~PBS_MARQUEE);

            SendMessageW(m_ProgressBar, PBM_SETRANGE32, 0, std::numeric_limits<int>::max());
        }

        SendMessageW(m_ProgressBar, PBM_SETPOS, static_cast<int>(m_Progress * std::numeric_limits<int>::max()), 0);
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
        DCHolder dcForMeasurement(m_Hwnd);
        DCObjectSelector fontSelector(dcForMeasurement, font);
        statisticsPositions = CalculateStatisticsPositions(dcForMeasurement, windowSize.cx, statisticsMarginFromEdgeX, statisticsMarginBetweenElementsX);
    }

    auto statisticsY = CalculateStatisticsYCoordinate(statisticsPositions.back(), windowSize.cy, margin.cy);
    if (m_StatisticsY != statisticsY)
    {
        m_StatisticsY = statisticsY;

        {
            DCHolder dcForMeasurement(m_Hwnd);
            DCObjectSelector fontSelector(dcForMeasurement, font);
            headerTextSize = CalculateHeaderSize(dcForMeasurement, windowSize.cx, margin.cx);
        }

        RepositionHeader(margin, headerTextSize);
        auto progressBarY = RepositionProgressBar(windowSize.cx, dpi, m_StatisticsY);
        RepositionExplorerBrowser(margin, windowSize.cx, headerTextSize.cy, progressBarY);
    }

    AdjustWindowPositions(statisticsPositions, { statisticsMarginFromEdgeX, statisticsY });
    RepositionStatistics(statisticsPositions);
}

static const wchar_t* NormalizeSize(double& size)
{
    Assert(size >= 1024);

    size /= 1024.0;
    if (size < 1024)
        return L" KB";

    size /= 1024.0;
    if (size < 1024)
        return L" MB";

    size /= 1024.0;
    if (size < 1024)
        return L" GB";

    size /= 1024.0;
    return L" TB";
}

static void AppendFileSize(const NumberFormatter& numberFormatter, std::wstring& dest, uint64_t size)
{
    if (size < 1024)
    {
        numberFormatter.AppendInteger(dest, size);
        dest += L" bytes";
    }
    else
    {
        double normalizedSize = static_cast<double>(size);
        auto unit = NormalizeSize(normalizedSize);
        numberFormatter.AppendFloat(dest, normalizedSize, 3);
        dest += unit;
    }
}

void SearchResultWindow::UpdateStatisticsText()
{
    m_StatisticsText[kDirectoriesEnumerated].resize(kStatisticsLabels[kDirectoriesEnumerated].size());
    m_NumberFormatter.AppendInteger(m_StatisticsText[kDirectoriesEnumerated], m_SearchStatistics.directoriesEnumerated);

    m_StatisticsText[kFilesEnumerated].resize(kStatisticsLabels[kFilesEnumerated].size());
    m_NumberFormatter.AppendInteger(m_StatisticsText[kFilesEnumerated], m_SearchStatistics.filesEnumerated);

    m_StatisticsText[kFileContentsSearched].resize(kStatisticsLabels[kFileContentsSearched].size());
    m_NumberFormatter.AppendInteger(m_StatisticsText[kFileContentsSearched], m_SearchStatistics.fileContentsSearched);

    m_StatisticsText[kTotalEnumeratedFilesSize].resize(kStatisticsLabels[kTotalEnumeratedFilesSize].size());
    AppendFileSize(m_NumberFormatter, m_StatisticsText[kTotalEnumeratedFilesSize], m_SearchStatistics.totalFileSize);

    m_StatisticsText[kTotalContentsSearchedFilesSize].resize(kStatisticsLabels[kTotalContentsSearchedFilesSize].size());
    AppendFileSize(m_NumberFormatter, m_StatisticsText[kTotalContentsSearchedFilesSize], m_SearchStatistics.scannedFileSize);

    m_StatisticsText[kResultsFound].resize(kStatisticsLabels[kResultsFound].size());
    m_NumberFormatter.AppendInteger(m_StatisticsText[kResultsFound], m_SearchStatistics.resultsFound);

    m_StatisticsText[kSearchTime].resize(kStatisticsLabels[kSearchTime].size());
    m_NumberFormatter.AppendFloat(m_StatisticsText[kSearchTime], m_SearchStatistics.searchTimeInSeconds, 3);
    m_StatisticsText[kSearchTime] += L" seconds";
}

void SearchResultWindow::OnFileFound(const WIN32_FIND_DATAW& findData, const wchar_t* path)
{
    decltype(m_Initialized) _false = false;

    while (!m_Initialized)
        WaitOnAddress(&m_Initialized, &_false, sizeof(m_Initialized), INFINITE);

    AddItemToView(m_ExplorerWindow, &findData, path);
}

void SearchResultWindow::OnProgressUpdate(const SearchStatistics& searchStatistics, double progress)
{
    m_CallbackQueue.InvokeAsync([this, searchStatistics, progress]()
    {
        m_SearchStatistics = searchStatistics;
        m_Progress = progress;

        OnStatisticsUpdate();
    });
}

void SearchResultWindow::OnSearchDone(const SearchStatistics& searchStatistics)
{
    m_CallbackQueue.InvokeAsync([this, searchStatistics]()
    {
        if (!m_IsTearingDown)
        {
            m_SearchStatistics = searchStatistics;
            m_Progress = 1.0;

            OnStatisticsUpdate();
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
