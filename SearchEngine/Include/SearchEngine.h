#pragma once

#include "SearchEngineTypes.h"

class FileSearcher;

extern "C" EXPORT_SEARCHENGINE FileSearcher * Search(
	FoundPathCallback foundPathCallback,
	SearchProgressUpdated progressUpdatedCallback,
	SearchDoneCallback searchDoneCallback,
	ErrorCallback errorCallback,
	const wchar_t* searchPath,
	const wchar_t* searchPattern,
	const wchar_t* searchString,
	SearchFlags searchFlags,
	uint64_t ignoreFilesLargerThan);

extern "C" EXPORT_SEARCHENGINE void CleanupSearchOperation(FileSearcher* searcher);