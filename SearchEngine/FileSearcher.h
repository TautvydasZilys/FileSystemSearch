#pragma once

#include "FileContentSearchData.h"
#include "HandleHolder.h"
#include "OrdinalStringSearcher.h"
#include "ScopedStackAllocator.h"
#include "SearchInstructions.h"
#include "SearchResultData.h"
#include "UnicodeUtf16StringSearcher.h"
#include "WorkQueue.h"

class FileSearcher
{
private:
	SearchInstructions m_SearchInstructions;
	SearchStatistics m_SearchStatistics;
	std::string m_Utf8SearchString;

	WorkQueue<FileSearcher, FileContentSearchData> m_FileContentSearchWorkQueue;
	WorkQueue<FileSearcher, SearchResultData> m_SearchResultDispatchWorkQueue;

	Microsoft::WRL::ComPtr<IDStorageFactory> m_DStorageFactory;

	volatile bool m_FinishedSearchingFileSystem;
	volatile bool m_IsFinished;
	bool m_SearchStringIsAscii;
	bool m_FailedInit;

	OrdinalStringSearcher<char> m_OrdinalUtf8Searcher;
	UnicodeUtf16StringSearcher m_UnicodeUtf16Searcher;
	OrdinalStringSearcher<wchar_t> m_OrdinalUtf16Searcher;

	HandleHolder m_FileSystemSearchThread;
	uint32_t m_RefCount;

	LARGE_INTEGER m_SearchStart;
	LARGE_INTEGER m_PerformanceFrequency;

	void AddRef();
	void Release();

	void Search();
	void SearchFileSystem();
	void OnDirectoryFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	void OnFileFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	bool SearchInFileName(const std::wstring& directory, const WIN32_FIND_DATAW& findData, bool searchInPath, ScopedStackAllocator& stackAllocator);
	bool SearchForString(const wchar_t* str, size_t length, ScopedStackAllocator& stackAllocator);

	void InitializeFileContentSearchThread(WorkQueue<FileSearcher, FileContentSearchData>& contentSearchWorkQueue);
	void InitializeSearchResultDispatcherWorkerThread(WorkQueue<FileSearcher, SearchResultData>& workQueue);

	double GetTotalSearchTimeInSeconds();
	void ReportProgress();
	void DispatchSearchResult(const WIN32_FIND_DATAW& findData, std::wstring&& path);

	void SearchFileContents(const FileContentSearchData& searchData, uint8_t* primaryBuffer, uint8_t* secondaryBuffer, ScopedStackAllocator& stackAllocator, IDStorageQueue* dstorageQueue, IDStorageStatusArray* dstorageStatusArray, uint64_t& readIndex);
	bool PerformFileContentSearch(uint8_t* fileBytes, uint32_t bufferLength, ScopedStackAllocator& stackAllocator);
	void AddToScannedFileSize(int64_t size);

	FileSearcher(SearchInstructions&& searchInstructions);

public:
	static FileSearcher* BeginSearch(SearchInstructions&& searchInstructions);
	void Cleanup();
};
