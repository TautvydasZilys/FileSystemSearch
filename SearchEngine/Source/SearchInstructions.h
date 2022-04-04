#pragma once

#include "SearchEngineTypes.h"
#include "Utilities/StringUtils.h"

struct SearchInstructions
{
	FoundPathCallback onFoundPath;
	SearchProgressUpdated onProgressUpdated;
	SearchDoneCallback onDone;
	ErrorCallback onError;

	std::wstring searchPath;
	std::wstring searchPattern;
	std::wstring searchString;
	std::string utf8SearchString;

	SearchFlags searchFlags;
	uint64_t ignoreFilesLargerThan;

	void* callbackContext;

	SearchInstructions(FoundPathCallback foundPathCallback, SearchProgressUpdated progressUpdatedCallback, SearchDoneCallback searchDoneCallback, ErrorCallback errorCallback, const wchar_t* searchPath, const wchar_t* searchPattern, const wchar_t* searchString,
		SearchFlags searchFlags, uint64_t ignoreFilesLargerThan, void* callbackContext) :
		onFoundPath(foundPathCallback),
		onProgressUpdated(progressUpdatedCallback),
		onDone(searchDoneCallback),
		onError(errorCallback),
		searchPath(searchPath),
		searchPattern(searchPattern),
		searchString(searchString),
		searchFlags(searchFlags),
		ignoreFilesLargerThan(ignoreFilesLargerThan),
		callbackContext(callbackContext)
	{
		if (StringUtils::IsAscii(this->searchString))
		{
			this->searchFlags |= SearchFlags::kSearchStringIsAscii;

			if (IgnoreCase())
				StringUtils::ToLowerAsciiInline(this->searchString);
		}

		if (SearchInFileContents() && SearchContentsAsUtf8())
			this->utf8SearchString = StringUtils::Utf16ToUtf8(this->searchString);
	}

	SearchInstructions(SearchInstructions&& other):
		onFoundPath(other.onFoundPath),
		onProgressUpdated(other.onProgressUpdated),
		onDone(other.onDone),
		onError(other.onError),
		searchPath(std::move(other.searchPath)),
		searchPattern(std::move(other.searchPattern)),
		searchString(std::move(other.searchString)),
		utf8SearchString(std::move(other.utf8SearchString)),
		searchFlags(other.searchFlags),
		ignoreFilesLargerThan(other.ignoreFilesLargerThan),
		callbackContext(other.callbackContext)
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
