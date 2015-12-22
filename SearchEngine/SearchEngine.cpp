#include "PrecompiledHeader.h"
#include "FileSearcher.h"

extern "C" __declspec(dllexport) FileSearcher* Search(
	FoundPathCallback foundPathCallback,
	ProgressUpdated progressUpdatedCallback,
	SearchDoneCallback searchDoneCallback,
	const wchar_t* searchPath,
	const wchar_t* searchPattern,
	const wchar_t* searchString,
	SearchFlags searchFlags,
	uint64_t ignoreFilesLargerThan)
{
	return FileSearcher::BeginSearch(SearchInstructions(foundPathCallback, progressUpdatedCallback, searchDoneCallback, searchPath, searchPattern, searchString, searchFlags, ignoreFilesLargerThan));
}

extern "C" __declspec(dllexport) void CleanupSearchOperation(FileSearcher* searcher)
{
	searcher->Cleanup();
}