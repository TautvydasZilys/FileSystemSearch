#include "PrecompiledHeader.h"
#include "FileSearcher.h"
#include "SearchEngine.h"

extern "C" FileSearcher* Search(
	FoundPathCallback foundPathCallback,
	SearchProgressUpdated progressUpdatedCallback,
	SearchDoneCallback searchDoneCallback,
	ErrorCallback errorCallback,
	const wchar_t* searchPath,
	const wchar_t* searchPattern,
	const wchar_t* searchString,
	SearchFlags searchFlags,
	uint64_t ignoreFilesLargerThan)
{
	return FileSearcher::BeginSearch(SearchInstructions(foundPathCallback, progressUpdatedCallback, searchDoneCallback, errorCallback, searchPath, searchPattern, searchString, searchFlags, ignoreFilesLargerThan));
}

extern "C" void CleanupSearchOperation(FileSearcher* searcher)
{
	searcher->Cleanup();
}