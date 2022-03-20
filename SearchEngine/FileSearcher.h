#pragma once

#include "FileContentSearchData.h"
#include "FileReadWorkQueue.h"
#include "HandleHolder.h"
#include "SearchResultReporter.h"
#include "StringSearcher.h"
#include "WorkQueue.h"

class ScopedStackAllocator;

class FileSearcher
{
public:
	static FileSearcher* BeginSearch(SearchInstructions&& searchInstructions);
	void Cleanup();

private:
	FileSearcher(SearchInstructions&& searchInstructions);

	void AddRef();
	void Release();

	void Search();
	void SearchFileSystem();
	void OnDirectoryFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	void OnFileFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	bool SearchInFileName(const std::wstring& directory, const WIN32_FIND_DATAW& findData, bool searchInPath, ScopedStackAllocator& stackAllocator);

private:
	const SearchInstructions m_SearchInstructions;
	StringSearcher m_StringSearcher;

	SearchResultReporter m_SearchResultReporter;
	FileReadWorkQueue m_FileReadWorkQueue;

	ThreadHandleHolder m_FileSystemSearchThread;
	uint32_t m_RefCount;

	std::atomic<bool> m_FinishedSearchingFileSystem;
	std::atomic<bool> m_IsFinished;
	bool m_FailedInit;
};
