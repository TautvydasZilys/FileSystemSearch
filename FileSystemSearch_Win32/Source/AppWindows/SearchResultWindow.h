#pragma once

#include "HandleHolder.h"
#include "NonCopyable.h"
#include "SearchEngineTypes.h"
#include "Utilities/EventQueue.h"
#include "Utilities/HwndHolder.h"

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

private:
    SearchResultWindow(std::unique_ptr<SearchResultWindowArguments> args);
    ~SearchResultWindow();

    void RunMessageLoop();

    operator HWND() const { return m_Hwnd;}
    void OnCreate(HWND hWnd);
    void RepositionControls(int windowWidth, int windowHeight, uint32_t dpi);
    
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

    FileSearcher* m_Searcher;
    SearcherCleanupState m_SearcherCleanupState;
    bool m_IsTearingDown;

};