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
	uint32_t m_RefCount;
	SearchInstructions m_SearchInstructions;
	std::string m_Utf8SearchString;

	OrdinalStringSearcher<wchar_t> m_OrdinalUtf16Searcher;
	OrdinalStringSearcher<char> m_OrdinalUtf8Searcher;
	UnicodeUtf16StringSearcher m_UnicodeUtf16Searcher;
	bool m_SearchStringIsAscii;

	WorkQueue<FileSearcher, FileContentSearchData> m_FileContentSearchWorkQueue;
	WorkQueue<FileSearcher, SearchResultData> m_SearchResultDispatchWorkQueue;

	void AddRef();
	void Release();

	void Search();
	void OnDirectoryFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	void OnFileFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	bool SearchInFileName(const std::wstring& directory, const WIN32_FIND_DATAW& findData, bool searchInPath, ScopedStackAllocator& stackAllocator);
	bool SearchForString(const wchar_t* str, size_t length, ScopedStackAllocator& stackAllocator);

	void InitializeFileContentSearchThread(WorkQueue<FileSearcher, FileContentSearchData>& contentSearchWorkQueue);
	void InitializeSearchResultDispatcherWorkerThread(WorkQueue<FileSearcher, SearchResultData>& workQueue);
	void DispatchSearchResult(const WIN32_FIND_DATAW& findData, std::wstring&& path);

	void SearchFileContents(const FileContentSearchData& searchData, uint8_t* primaryBuffer, uint8_t* secondaryBuffer, ScopedStackAllocator& stackAllocator);
	bool PerformFileContentSearch(uint8_t* fileBytes, uint32_t bufferLength, ScopedStackAllocator& stackAllocator);

	FileSearcher(SearchInstructions&& searchInstructions);
	~FileSearcher();

public:
	static void Search(SearchInstructions&& searchInstructions);
};

