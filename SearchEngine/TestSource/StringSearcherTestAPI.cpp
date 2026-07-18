#include "PrecompiledHeader.h"
#include "StringSearch/StringSearcher.h"
#include "StringSearcherTestAPI.h"
#include "Utilities/ScopedStackAllocator.h"

#if INCLUDE_TESTS

struct TestStringSearcher
{
	TestStringSearcher(const wchar_t* searchString, SearchFlags searchFlags) :
		m_SearchInstructions(nullptr, nullptr, nullptr, nullptr, L"", L"", searchString, searchFlags, 0, nullptr),
		m_StringSearcher(m_SearchInstructions)
	{
	}
	
	bool PerformFileContentSearch(uint8_t* fileBytes, uint32_t bufferLength) const
	{
        return m_StringSearcher.PerformFileContentSearch(fileBytes, bufferLength, m_Allocator);
	}

private:
	mutable ScopedStackAllocator m_Allocator;
    SearchInstructions m_SearchInstructions;
    StringSearcher m_StringSearcher;
};

extern "C" TestStringSearcher* CreateStringSearcher(const wchar_t* searchString, SearchFlags searchFlags)
{
	return new TestStringSearcher(searchString, searchFlags);
}

extern "C" bool SearchFileContents(TestStringSearcher* stringSearcher, uint8_t* fileBytes, uint32_t byteCount)
{
    return stringSearcher->PerformFileContentSearch(fileBytes, byteCount);
}

extern "C" void FreeStringSearcher(TestStringSearcher* stringSearcher)
{
	delete stringSearcher;
}

#endif