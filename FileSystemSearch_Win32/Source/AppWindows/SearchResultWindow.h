#pragma once

#include "HandleHolder.h"
#include "NonCopyable.h"
#include "SearchEngineTypes.h"
#include "Utilities/EventQueue.h"
#include "Utilities/HwndHolder.h"
#include "Utilities/WindowPosition.h"

class ExplorerWindow;
class FontCache;
struct SearchResultWindowArguments;

class SearchResultWindow : NonCopyable
{
public:
    struct StaticInitializer : NonCopyable
    {
        StaticInitializer();
        ~StaticInitializer();
    };

    static void Spawn(const FontCache& fontCache, std::wstring&& searchPath, std::wstring&& searchPattern, std::wstring&& searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan);

private:
    enum class SearcherCleanupState
    {
        NotCleanedUp,
        CleanupInProgress,
        CleanedUp
    };

    enum StatisticsItems
    {
        kDirectoriesEnumerated,
        kFilesEnumerated,
        kFileContentsSearched,
        kTotalEnumeratedFilesSize,
        kTotalContentsSearchedFilesSize,
        kResultsFound,
        kSearchTime,
        kStatisticsCount,
    };

    static constexpr std::array<std::wstring_view, kStatisticsCount> kStatisticsLabels =
    {
        L"Directories enumerated: ",
        L"Files enumerated: ",
        L"File contents searched: ",
        L"Total enumerated files size: ",
        L"Total contents searched files size: ",
        L"Results found: ",
        L"Search time: ",
    };

private:
    SearchResultWindow(std::unique_ptr<SearchResultWindowArguments> args);
    ~SearchResultWindow();

    void RunMessageLoop();

    operator HWND() const { return m_Hwnd;}
    void OnCreate(HWND hWnd);
    void OnResize(SIZE windowSize, uint32_t dpi);

    SIZE CalculateHeaderSize(HDC hdc, int windowWidth, int marginX);
    std::array<WindowPosition, kStatisticsCount> CalculateStatisticsPositions(HDC hdc, int windowWidth, int marginFromEdgeX, int marginBetweenElementsX);
    static int CalculateStatisticsYCoordinate(const WindowPosition& lastStatisticPosition, int windowHeight, int marginY);

    void RepositionHeader(SIZE margin, SIZE headerSize);
    void RepositionStatistics(const std::array<WindowPosition, kStatisticsCount>& positions);
    int RepositionProgressBar(int windowWidth, uint32_t dpi, int statisticsY);
    void RepositionExplorerBrowser(SIZE margin, int windowWidth, int headerHeight, int progressBarY);

    void OnStatisticsUpdate(const SearchStatistics& searchStatistics, double progress);
    void UpdateStatisticsText(const SearchStatistics& searchStatistics);
    
    void OnFileFound(const WIN32_FIND_DATAW& findData, const wchar_t* path);
    void OnProgressUpdate(const SearchStatistics& searchStatistics, double progress);
    void OnSearchDone(const SearchStatistics& searchStatistics);
    void OnSearchError(const wchar_t* errorMessage);

    void CleanupSearchOperationIfNeeded();

    static LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    const FontCache& m_FontCache;
    EventQueue m_CallbackQueue;
    HwndHolder m_Hwnd;

    HwndHolder m_HeaderTextBlock;
    std::wstring m_HeaderText;

    HwndHolder m_ExplorerBrowserHost;
    ExplorerWindow* m_ExplorerWindow;

    HwndHolder m_ProgressBar;

    std::array<HwndHolder, kStatisticsCount> m_StatisticsTextBlocks;
    std::array<std::wstring, kStatisticsCount> m_StatisticsText;
    int m_StatisticsY;
    bool m_HasDeterminateProgress;

    FileSearcher* m_Searcher;
    SearcherCleanupState m_SearcherCleanupState;
    bool m_IsTearingDown;
};