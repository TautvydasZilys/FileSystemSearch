#pragma once

#include "HandleHolder.h"
#include "OrdinalStringSearcher.h"
#include "ScopedStackAllocator.h"
#include "UnicodeUtf16StringSearcher.h"

typedef void(__stdcall *FoundPathCallback)(const wchar_t* path);
typedef void(__stdcall *SearchProgressUpdated)(double progress);
typedef void(__stdcall *SearchDoneCallback)();

#define SearchFlagsEnumDefinition \
	EnumValue(SearchForFiles,         1 << 0) \
	EnumValue(SearchInFileName,       1 << 1) \
	EnumValue(SearchInFilePath,      1 << 2) \
	EnumValue(SearchInFileContents,  1 << 3) \
	EnumValue(SearchContentsAsUtf8,  1 << 4) \
	EnumValue(SearchContentsAsUtf16, 1 << 5) \
	EnumValue(SearchForDirectories,  1 << 6) \
	EnumValue(SearchInDirectoryPath, 1 << 7) \
	EnumValue(SearchInDirectoryName, 1 << 8) \
	EnumValue(SearchRecursively,     1 << 9) \
	EnumValue(IgnoreCase,            1 << 10)

enum class SearchFlags
{
#define EnumValue(name, value) k##name = value,
	SearchFlagsEnumDefinition
#undef EnumValue
};

MAKE_BIT_OPERATORS_FOR_ENUM_CLASS(SearchFlags)

struct SearchInstructions
{
	FoundPathCallback onFoundPath;
	SearchProgressUpdated onProgressUpdated;
	SearchDoneCallback onDone;

	std::wstring searchPath;
	std::wstring searchPattern;
	std::wstring searchString;
	std::string utf8SearchString;

	SearchFlags searchFlags;
	uint64_t ignoreFilesLargerThan;

	SearchInstructions(FoundPathCallback foundPathCallback, SearchProgressUpdated progressUpdatedCallback, SearchDoneCallback searchDoneCallback, const wchar_t* searchPath, const wchar_t* searchPattern, const wchar_t* searchString,
			SearchFlags searchFlags, uint64_t ignoreFilesLargerThan) :
		onFoundPath(foundPathCallback),
		onProgressUpdated(progressUpdatedCallback),
		onDone(searchDoneCallback),
		searchPath(searchPath),
		searchPattern(searchPattern),
		searchString(searchString),
		searchFlags(searchFlags),
		ignoreFilesLargerThan(ignoreFilesLargerThan)
	{
	}

	SearchInstructions(SearchInstructions&& other) :
		onFoundPath(other.onFoundPath),
		onProgressUpdated(other.onProgressUpdated),
		onDone(other.onDone),
		searchPath(std::move(other.searchPath)),
		searchPattern(std::move(other.searchPattern)),
		searchString(std::move(other.searchString)),
		utf8SearchString(std::move(other.utf8SearchString)),
		searchFlags(other.searchFlags),
		ignoreFilesLargerThan(other.ignoreFilesLargerThan)		
	{
	}


#define EnumValue(name, value) \
	inline bool name() const \
	{ \
		return static_cast<std::underlying_type_t<SearchFlags>>(searchFlags & SearchFlags::k##name) != 0; \
	}

	SearchFlagsEnumDefinition
#undef EnumValue
};


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

	uint32_t m_WorkerThreadCount;
	HandleHolder m_WorkSemaphore;
	SLIST_HEADER* m_WorkList;
	std::vector<HandleHolder> m_WorkerThreadHandles;

	void AddRef();
	void Release();

	void InitializeContentSearchThreads();

	void Search();
	void OnDirectoryFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	void OnFileFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator);
	bool SearchInFileName(const std::wstring& directory, const wchar_t* fileName, bool searchInPath, ScopedStackAllocator& stackAllocator);
	bool SearchForString(const wchar_t* str, size_t length, ScopedStackAllocator& stackAllocator);

	void WorkerThreadLoop();
	void SearchFileContents(const struct WorkerJob* job, uint8_t* primaryBuffer, uint8_t* secondaryBuffer, ScopedStackAllocator& stackAllocator);
	bool PerformFileContentSearch(uint8_t* fileBytes, uint32_t bufferLength, ScopedStackAllocator& stackAllocator);

	FileSearcher(SearchInstructions&& searchInstructions);
	~FileSearcher();

public:
	static void Search(SearchInstructions&& searchInstructions);
};

