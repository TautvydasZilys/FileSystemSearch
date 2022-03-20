#pragma once

#include "NonCopyable.h"
#include "OrdinalStringSearcher.h"
#include "SearchInstructions.h"
#include "SearchResultData.h"
#include "WorkQueue.h"
#include "UnicodeUtf16StringSearcher.h"

class ScopedStackAllocator;

class StringSearcher : NonCopyable
{
public:
	StringSearcher(const SearchInstructions& searchInstructions);

	bool SearchForString(std::wstring_view str, ScopedStackAllocator& stackAllocator) const;
	bool PerformFileContentSearch(uint8_t* fileBytes, uint32_t bufferLength, ScopedStackAllocator& stackAllocator) const;

private:
	const SearchInstructions& m_SearchInstructions;

	OrdinalStringSearcher<char> m_OrdinalUtf8Searcher;
	UnicodeUtf16StringSearcher m_UnicodeUtf16Searcher;
	OrdinalStringSearcher<wchar_t> m_OrdinalUtf16Searcher;
};