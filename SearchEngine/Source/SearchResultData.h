#pragma once

#include "NonCopyable.h"

struct SearchResultData : NonCopyable
{
	std::wstring resultPath;
	WIN32_FIND_DATAW resultFindData;

	SearchResultData()
	{
	}

	SearchResultData(std::wstring&& resultPath, const WIN32_FIND_DATAW& resultFindData) :
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