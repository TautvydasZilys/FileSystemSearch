#pragma once

struct SearchStatistics
{
	uint64_t directoriesEnumerated;
	uint64_t filesEnumerated;
	uint64_t fileContentsSearched;
	uint64_t resultsFound;
	uint64_t totalFileSize;
	int64_t scannedFileSize;
	double searchTimeInSeconds;
};

typedef void(__stdcall* FoundPathCallback)(void* context, const WIN32_FIND_DATAW& findData, const wchar_t* path);
typedef void(__stdcall* SearchProgressUpdated)(void* context, const SearchStatistics& searchStatistics, double progress);
typedef void(__stdcall* SearchDoneCallback)(void* context, const SearchStatistics& searchStatistics);
typedef void(__stdcall* ErrorCallback)(void* context, const wchar_t* errorMessage);

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
	EnumValue(IgnoreCase,            1 << 10) \
	EnumValue(IgnoreDotStart,        1 << 11) \
	EnumValue(UseDirectStorage,      1 << 12) \
	EnumValue(SearchStringIsAscii,   1 << 31) // Internal

enum class SearchFlags
{
#define EnumValue(name, value) k##name = value,
	SearchFlagsEnumDefinition
#undef EnumValue
};

MAKE_BIT_OPERATORS_FOR_ENUM_CLASS(SearchFlags)

class FileSearcher;