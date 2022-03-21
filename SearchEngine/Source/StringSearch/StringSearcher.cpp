#include "PrecompiledHeader.h"
#include "StringSearcher.h"
#include "Utilities/PathUtils.h"
#include "Utilities/ScopedStackAllocator.h"
#include "Utilities/StringUtils.h"

StringSearcher::StringSearcher(const SearchInstructions& searchInstructions) :
	m_SearchInstructions(searchInstructions)
{
	// Sanity checks
	if (searchInstructions.searchString.length() == 0)
		__fastfail(1);

	if (searchInstructions.searchString.length() > std::numeric_limits<int32_t>::max())
		__fastfail(1);

	if (searchInstructions.SearchStringIsAscii() || !searchInstructions.IgnoreCase())
		m_OrdinalUtf16Searcher.Initialize(searchInstructions.searchString.c_str(), searchInstructions.searchString.length());
	else
		m_UnicodeUtf16Searcher.Initialize(searchInstructions.searchString.c_str(), searchInstructions.searchString.length());

	if (searchInstructions.SearchInFileContents() && searchInstructions.SearchContentsAsUtf8())
		m_OrdinalUtf8Searcher.Initialize(searchInstructions.utf8SearchString.c_str(), searchInstructions.utf8SearchString.length());
}

bool StringSearcher::SearchForString(std::wstring_view str, ScopedStackAllocator& stackAllocator) const
{
	if (m_SearchInstructions.IgnoreCase())
	{
		if (m_SearchInstructions.SearchStringIsAscii())
		{
			auto memory = stackAllocator.Allocate(sizeof(wchar_t) * str.length());
			auto lowerCase = static_cast<wchar_t*>(memory);
			StringUtils::ToLowerAscii(str.data(), lowerCase, str.length());
			return m_OrdinalUtf16Searcher.HasSubstring(lowerCase, lowerCase + str.length());
		}
		else
		{
			return m_UnicodeUtf16Searcher.HasSubstring(str.begin(), str.end());
		}
	}

	return m_OrdinalUtf16Searcher.HasSubstring(str.begin(), str.end());
}

bool StringSearcher::PerformFileContentSearch(uint8_t* fileBytes, uint32_t bufferLength, ScopedStackAllocator& stackAllocator) const
{
	if (m_SearchInstructions.SearchContentsAsUtf16())
	{
		if (SearchForString(std::wstring_view(reinterpret_cast<const wchar_t*>(fileBytes), bufferLength / sizeof(wchar_t)), stackAllocator))
			return true;
	}

	if (!m_SearchInstructions.SearchContentsAsUtf8())
		return false;

	if (!m_SearchInstructions.SearchStringIsAscii() && m_SearchInstructions.IgnoreCase())
		__fastfail(1); // Not implemented

	if (m_SearchInstructions.IgnoreCase())
		StringUtils::ToLowerAscii(fileBytes, fileBytes, bufferLength);

	return m_OrdinalUtf8Searcher.HasSubstring(fileBytes, fileBytes + bufferLength);
}
