#include "PrecompiledHeader.h"
#include "PerformanceTest.h"
#include "TestLogger.h"

Testing::PreparedPerformanceTestLayout::PreparedPerformanceTestLayout(std::wstring_view testName) :
    m_Layout(testName)
{
}

Testing::PreparedPerformanceTestLayout::~PreparedPerformanceTestLayout()
{
    if (m_Layout)
    {
        PrintToStdout(std::format(L"Cleaning up layout '{}'\r\n", m_Layout->TestName()));
        m_Layout->GetTestDirectory().Remove();
        PrintToStdout(std::format(L"    Layout '{}' cleaned up\r\n", m_Layout->TestName()));
    }
}

Testing::PreparedPerformanceTestLayout::PreparedPerformanceTestLayout(PreparedPerformanceTestLayout&& other) noexcept :
    m_Layout(std::move(other.m_Layout))
{
    other.m_Layout = std::nullopt;
}

Testing::PreparedPerformanceTestLayout& Testing::PreparedPerformanceTestLayout::operator=(PreparedPerformanceTestLayout&& other) noexcept
{
    if (this != &other)
        std::swap(m_Layout, other.m_Layout);

    return *this;
}

Testing::PreparedPerformanceTestLayout Testing::PerformanceTestDataLayout::Prepare() const
{
    PrintToStdout(std::format(L"Preparing layout '{}'\r\n", m_LayoutName));

    PreparedPerformanceTestLayout result(m_LayoutName);
    PrepareLayout(result.GetTestDirectory());

    PrintToStdout(std::format(L"    Layout '{}' prepared\r\n", m_LayoutName));
    return result;
}

void Testing::PerformanceTestDataLayout::PrepareLayout(const TestDirectory& testDirectory) const
{
    std::vector<size_t> counters;
    counters.resize(m_LayoutSizes.size(), 0);

    GenerateLayoutRecursive(testDirectory, counters);
}

static constexpr const wchar_t kFileNameCharacters[] = L"0123abcdABCD";
static constexpr size_t kPrimes[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };

static constexpr wchar_t GetFileNameCharacter(size_t index)
{
    return kFileNameCharacters[index % (std::size(kFileNameCharacters) - 1)];
}

static constexpr std::array<wchar_t, 12> MakeBaseFileName(size_t index)
{
    std::array<wchar_t, 12> result;
    static_assert(std::size(kPrimes) == std::size(result), "Sizes must match!");

    index += 201911;
    for (size_t i = 0; i < result.size(); i++)
    {
        index = (index * kPrimes[i]) % 1004167;
        result[i] = GetFileNameCharacter(index);
    }

    return result;
}

void Testing::PerformanceTestDataLayout::GenerateLayoutRecursive(const TestDirectory& testDirectory, std::span<size_t> counters, size_t depth) const
{
    const size_t fileDepth = m_LayoutSizes.size() - 1;
    for (size_t i = 0; i < m_LayoutSizes[fileDepth]; i++)
    {
        auto fileIndex = counters[fileDepth]++;
        const auto baseFileName = MakeBaseFileName(293 * fileIndex);
        const auto fileExtension = m_TestFileExtensions[fileIndex % m_TestFileExtensions.size()];

        std::wstring fileName(&baseFileName[0], baseFileName.size());
        fileName += fileExtension;

        if (m_TestFiles.size() > 0)
        {
            Testing::TestFile(testDirectory, fileName, m_TestFiles[fileIndex % m_TestFiles.size()].value);
        }
        else
        {
            Testing::TestFile(testDirectory, fileName, std::span<const char>("x", 1));
        }
    }

    if (depth != m_LayoutSizes.size() - 1)
    {
        for (size_t i = 0; i < m_LayoutSizes[depth]; i++)
        {
            auto folderIndex = counters[depth]++;
            auto folderName = MakeBaseFileName(593 * folderIndex);
            auto subDir = testDirectory.SubDirectory(std::wstring_view(&folderName[0], folderName.size()));
            GenerateLayoutRecursive(subDir, counters, depth + 1);
        }
    }
}

bool Testing::PerformanceTestDataLayout::operator<(const PerformanceTestDataLayout& other) const
{
    if (m_LayoutSizes.size() != other.m_LayoutSizes.size())
        return m_LayoutSizes.size() < other.m_LayoutSizes.size();
        
    for (size_t i = 0; i < m_LayoutSizes.size(); i++)
    {
        if (m_LayoutSizes[i] != other.m_LayoutSizes[i])
            return m_LayoutSizes[i] < other.m_LayoutSizes[i];
    }
        
    if (m_TestFiles.size() != other.m_TestFiles.size())
        return m_TestFiles.size() < other.m_TestFiles.size();
        
    for (size_t i = 0; i < m_TestFiles.size(); i++)
    {
        if (m_TestFiles[i] != other.m_TestFiles[i])
            return m_TestFiles[i] < other.m_TestFiles[i];
    }
        
    if (m_TestFileExtensions.size() != other.m_TestFileExtensions.size())
        return m_TestFileExtensions.size() < other.m_TestFileExtensions.size();
        
    for (size_t i = 0; i < m_TestFileExtensions.size(); i++)
    {
        if (m_TestFileExtensions[i] != other.m_TestFileExtensions[i])
            return m_TestFileExtensions[i] < other.m_TestFileExtensions[i];
    }

    return false;
}

bool Testing::PerformanceTestDataLayout::operator==(const PerformanceTestDataLayout& other) const
{
    if (m_LayoutSizes.size() != other.m_LayoutSizes.size())
        return false;

    for (size_t i = 0; i < m_LayoutSizes.size(); i++)
    {
        if (m_LayoutSizes[i] != other.m_LayoutSizes[i])
            return false;
    }

    if (m_TestFiles.size() != other.m_TestFiles.size())
        return false;

    for (size_t i = 0; i < m_TestFiles.size(); i++)
    {
        if (m_TestFiles[i] != other.m_TestFiles[i])
            return false;
    }

    if (m_TestFileExtensions.size() != other.m_TestFileExtensions.size())
        return false;

    for (size_t i = 0; i < m_TestFileExtensions.size(); i++)
    {
        if (m_TestFileExtensions[i] != other.m_TestFileExtensions[i])
            return false;
    }

    return true;
}

int Testing::RunAllPerformanceTests()
{
    try
    {
        GlobalTestContext globalTestContext;

        std::vector<PerformanceTestBase*> allTests;

        {
            auto test = PerformanceTestBase::GetFirstTest();
            while (test != nullptr)
            {
                allTests.push_back(test);
                test = test->GetNextTest();
            }
        }

        // First check if there are any layouts that have same names but are actually different
        std::sort(allTests.begin(), allTests.end(), [](const PerformanceTestBase* a, const PerformanceTestBase* b)
        {
            return a->GetPerformanceTestDataLayout()->GetLayoutName() < b->GetPerformanceTestDataLayout()->GetLayoutName();
        });

        for (size_t i = 1; i < allTests.size(); i++)
        {
            auto a = allTests[i - 1]->GetPerformanceTestDataLayout();
            auto b = allTests[i]->GetPerformanceTestDataLayout();
            if (a->GetLayoutName() == b->GetLayoutName())
                CHECK(a == b, std::format(L"Two performance test layouts have the same name '{}' but are different", a->GetLayoutName()));
        }

        // Now check if there are any layouts that are identical but have different names
        std::sort(allTests.begin(), allTests.end(), [](const PerformanceTestBase* a, const PerformanceTestBase* b)
        {
            auto& aLayout = *a->GetPerformanceTestDataLayout();
            auto& bLayout = *b->GetPerformanceTestDataLayout();

            if (aLayout == bLayout)
                return a->TestName() < b->TestName();

            return aLayout < bLayout;
        });

        for (size_t i = 1; i < allTests.size(); i++)
        {
            auto a = allTests[i - 1]->GetPerformanceTestDataLayout();
            auto b = allTests[i]->GetPerformanceTestDataLayout();
            if (a == b)
                CHECK(a->GetLayoutName() == b->GetLayoutName(), std::format(L"Two performance test layouts are identical but have different names: '{}' and '{}'", a->GetLayoutName(), b->GetLayoutName()));
        }

        // Now, run the tests
        const PerformanceTestDataLayout* currentLayout = nullptr;
        std::optional<PreparedPerformanceTestLayout> preparedLayout;

        int failureCount = 0;
        int successCount = 0;
        int testCount = 0;
        int layoutCount = 0;

        for (const auto& test : allTests)
        {
            if (test->GetPerformanceTestDataLayout() != currentLayout)
            {
                auto newLayout = test->GetPerformanceTestDataLayout();
                preparedLayout = std::nullopt; // Clear the previous layout before prepared new one to use less disk space
                preparedLayout = newLayout->Prepare();
                currentLayout = newLayout;
                layoutCount++;
            }

            CHECK(preparedLayout, L"No layout has been prepared!");

            try
            {
                PrintToStdout(std::format(L"Running '{}'\r\n", test->TestName()));
                testCount++;
                test->Run(*preparedLayout);
                successCount++;
                PrintToStdout(L"    PASS!\r\n");
            }
            catch (const TestFailedException& e)
            {
                failureCount++;
                PrintToStdout(std::format(L"    FAILED:\r\n        {}\r\n", e.Text()));
            }
        }

        preparedLayout = std::nullopt;

        PrintToStdout(std::format(L"Ran {} tests across {} layouts: {} succeeded, {} failed.\r\n", testCount, layoutCount, successCount, failureCount));
        return failureCount;
    }
    catch (const TestFailedException& e)
    {
        PrintToStdout(std::format(L"Tests encountered a fatal error outside of any single test: {}\r\n", e.Text()));
        return std::numeric_limits<int>::max();
    }
}

void Testing::ReportPerformanceTestResults()
{
    PrintToStdout(std::format(L"\r\nPerformance Test Results:\r\n"));

    std::vector<PerformanceTestBase*> allTests;

    {
        auto test = PerformanceTestBase::GetFirstTest();
        while (test != nullptr)
        {
            allTests.push_back(test);
            test = test->GetNextTest();
        }
    }

    if (allTests.empty())
    {
        PrintToStdout(std::format(L"    No performance tests were run.\r\n"));
        return;
    }

    std::sort(allTests.begin(), allTests.end(), [](const PerformanceTestBase* a, const PerformanceTestBase* b)
    {
        return a->TestName() < b->TestName();
    });

    for (auto test : allTests)
    {
        auto name = test->TestName();
        auto durationMS = 1000.0 * test->GetMedianRunTime();

        PrintToStdout(std::format(L"    {:100}: {:.2f} ms\r\n", name, durationMS));
    }
}
