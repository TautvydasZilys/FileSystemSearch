#pragma once

#include "FileContentSearchData.h"
#include "Utilities/WorkQueue.h"

struct SearchInstructions;
class SearchResultReporter;
class ScopedStackAllocator;
class StringSearcher;

class OverlappedIOReader : ThreadedWorkQueue<OverlappedIOReader, FileOpenData>
{
public:
    OverlappedIOReader(const StringSearcher& stringSearcher, const SearchInstructions& searchInstructions, SearchResultReporter& searchResultReporter);

    void Initialize();
    void DrainWorkQueue();

    inline void CompleteAllWork()
    {
        MyBase::CompleteAllWork();
    }

    inline void ScanFile(FileOpenData fileOpenData)
    {
        PushWorkItem(std::move(fileOpenData));
    }

private:
    typedef ThreadedWorkQueue<OverlappedIOReader, FileOpenData> MyBase;

private:
    void ContentsSearchThread();
    void SearchFileContents(const FileOpenData& searchData, uint8_t* primaryBuffer, uint8_t* secondaryBuffer, ScopedStackAllocator& stackAllocator, HANDLE overlappedEvent);

private:
    SearchResultReporter& m_SearchResultReporter;
    const StringSearcher& m_StringSearcher;
    const size_t m_MaxSearchStringLength;
    std::atomic<bool> m_IsFinished;
};