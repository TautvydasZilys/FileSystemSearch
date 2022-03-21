#pragma once

#include "NonCopyable.h"
#include "FileContentSearchData.h"

struct SearchResultData : NonCopyable
{
	std::wstring resultPath;
	FileFindData resultFindData;

	SearchResultData()
	{
	}

	SearchResultData(std::wstring&& resultPath, const FileFindData& resultFindData) :
		resultPath(std::move(resultPath)),
		resultFindData(resultFindData)
	{
	}

	SearchResultData(SearchResultData&& other) :
		resultPath(std::move(other.resultPath)),
		resultFindData(other.resultFindData)
	{
	}

	SearchResultData& operator=(SearchResultData&& other)
	{
		resultPath = std::move(other.resultPath);
		resultFindData = other.resultFindData;
		return *this;
	}
};