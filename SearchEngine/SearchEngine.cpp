#include "PrecompiledHeader.h"
#include "FileSearcher.h"

extern "C" __declspec(dllexport) void Search(
	FoundPathCallback foundPathCallback,
	SearchProgressUpdated progressUpdatedCallback,
	SearchDoneCallback searchDoneCallback,
	const wchar_t* searchPath,
	const wchar_t* searchPattern,
	const wchar_t* searchString,
	SearchFlags searchFlags,
	uint64_t ignoreFilesLargerThan)
{
	FileSearcher::Search(SearchInstructions(foundPathCallback, progressUpdatedCallback, searchDoneCallback, searchPath, searchPattern, searchString, searchFlags, ignoreFilesLargerThan));
}
