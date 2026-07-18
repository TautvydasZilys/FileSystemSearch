#include "PrecompiledHeader.h"
#include "StringSearchPerformanceTest.h"
#include "StringSearcherTestAPI.h"

void Testing::StringSearchPerformanceTest::Run(TestStringSearcher* testSearcher, std::vector<uint8_t> fileBytes) const
{
    const size_t kIterationCount = 100;

    m_MedianTime = RunPerformanceTestIterations(kIterationCount, [] {}, [&]
    {
        size_t offset = 0;

        while (offset < fileBytes.size())
        {
            uint32_t bytesToSearch = static_cast<uint32_t>(std::min(static_cast<size_t>(m_ChunkSize), fileBytes.size() - offset));
            if (SearchFileContents(testSearcher, fileBytes.data() + offset, bytesToSearch))
                return;

            offset += bytesToSearch;
            if (offset != fileBytes.size())
            {
                CHECK(offset >= m_MaxSearchStringLength, L"Offset is less than the maximum search string length");
                offset -= m_MaxSearchStringLength;
            }
        }
    }, [](size_t) {});
}
