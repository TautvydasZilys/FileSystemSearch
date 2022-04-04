#pragma once

#include "SearchInstructions.h"
#include "SearchResultData.h"
#include "Utilities/WorkQueue.h"

class SearchResultReporter : ThreadedWorkQueue<SearchResultReporter, SearchResultData>
{
public:
    SearchResultReporter(const SearchInstructions& searchInstructions);

    void ReportProgress(bool finishedScanningFileSystem);
    void DispatchSearchResult(const FileFindData& findData, std::wstring&& path);
    void FinishSearch();

    inline void DrainWorkQueue() { MyBase::DrainWorkQueue(); }
    inline void CompleteAllWork() { MyBase::CompleteAllWork(); }

    inline void AddToScannedFileSize(int64_t size) { InterlockedAdd64(&m_SearchStatistics.scannedFileSize, size); }
    inline void AddToScannedFileCount() { InterlockedIncrement(&m_SearchStatistics.fileContentsSearched); }
    inline void OnDirectoryEnumeratedThreadUnsafe() { m_SearchStatistics.directoriesEnumerated++; }
    inline void OnFileEnumeratedThreadUnsafe() { m_SearchStatistics.filesEnumerated++; }
    inline void OnTotalFileSizeAddedThreadUnsafe(uint64_t value) { m_SearchStatistics.totalFileSize += value; }

private:
    typedef ThreadedWorkQueue<SearchResultReporter, SearchResultData> MyBase;
    friend class MyBase;

    double GetTotalSearchTimeInSeconds();
    void InitializeSearchResultDispatcherWorkerThread();

private:
    SearchStatistics m_SearchStatistics;
    FoundPathCallback m_FoundPathCallback;
    SearchProgressUpdated m_ProgressCallback;
    SearchDoneCallback m_DoneCallback;
    void* m_CallbackContext;

    LARGE_INTEGER m_SearchStart;
    LARGE_INTEGER m_PerformanceFrequency;
};
