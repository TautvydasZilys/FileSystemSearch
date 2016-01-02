#pragma once

struct SearchStatistics
{
	uint64_t directoriesEnumerated;
	uint64_t filesEnumerated;
	uint64_t fileContentsSearched;
	uint64_t resultsFound;
	uint64_t totalFileSize;
	volatile int64_t scannedFileSize;
	double searchTimeInSeconds;
};

typedef void(__stdcall *FoundPathCallback)(const WIN32_FIND_DATAW* findData, const wchar_t* path);
typedef void(__stdcall *SearchProgressUpdated)(const SearchStatistics& searchStatistics, double progress);
typedef void(__stdcall *SearchDoneCallback)(const SearchStatistics& searchStatistics);

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
