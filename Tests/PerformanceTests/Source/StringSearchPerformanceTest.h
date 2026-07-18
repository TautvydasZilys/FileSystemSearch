#pragma once
#include "PerformanceTest.h"

struct TestStringSearcher;

namespace Testing
{
    struct StringSearcherParameters
    {
        const wchar_t* searchString;
        SearchFlags searchFlags;

        inline auto operator<(const StringSearcherParameters& other) const
        {
            if (searchFlags != other.searchFlags)
                return searchFlags < other.searchFlags;

            return wcscmp(searchString, other.searchString) < 0;
        }
    };

    struct StringSearchPerformanceTest : ITest, RegisteredTest<StringSearchPerformanceTest>
    {
        StringSearchPerformanceTest(std::wstring_view testName, const StringSearcherParameters& parameters, const wchar_t* fileToSearch, uint32_t chunkSize) :
            ITest(testName),
            m_Parameters(parameters),
            m_FileToSearch(fileToSearch),
            m_ChunkSize(chunkSize),
            m_MaxSearchStringLength(wcslen(parameters.searchString) * 4)
        {
        }

        // Copy on purpose! Searcher modifies file bytes in place for case-insensitive searches
        void Run(TestStringSearcher* testSearcher, std::vector<uint8_t> fileBytes) const;

        StringSearcherParameters GetSearcherParameters() const
        {
            return m_Parameters;
        }

        const wchar_t* GetFileToSearch() const
        {
            return m_FileToSearch;
        }

        double GetMedianRunTime() const { return m_MedianTime; }

    private:
        StringSearcherParameters m_Parameters;
        const wchar_t* m_FileToSearch;
        uint32_t m_ChunkSize;
        size_t m_MaxSearchStringLength;
        mutable double m_MedianTime = NAN;
    };
}
