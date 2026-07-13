#include "PrecompiledHeader.h"
#include "PerformanceTest.h"
#include "PerformanceIntegrationTest.h"
#include "StringSearcherTestAPI.h"
#include "StringSearchPerformanceTest.h"
#include "TestLogger.h"
#include "Utilities/FileUtilities.h"

static void RunAllPerformanceIntegrationTests(int& failureCount, int& successCount, int& testCount, int& layoutCount)
{
    using namespace Testing;

    std::vector<PerformanceIntegrationTestBase*> allTests;

    {
        auto test = PerformanceIntegrationTestBase::GetFirstTest();
        while (test != nullptr)
        {
            allTests.push_back(test);
            test = test->GetNextTest();
        }
    }

    // First check if there are any layouts that are identical but have different names
    std::sort(allTests.begin(), allTests.end(), [](const PerformanceIntegrationTestBase* a, const PerformanceIntegrationTestBase* b)
    {
        return *a->GetPerformanceTestDataLayout() < *b->GetPerformanceTestDataLayout();
    });

    for (size_t i = 1; i < allTests.size(); i++)
    {
        auto& a = *allTests[i - 1]->GetPerformanceTestDataLayout();
        auto& b = *allTests[i]->GetPerformanceTestDataLayout();
        if (a == b)
            CHECK(a.GetLayoutName() == b.GetLayoutName(), std::format(L"Two performance test layouts are identical but have different names: '{}' and '{}'", a.GetLayoutName(), b.GetLayoutName()));
    }

    // Now check if there are any layouts that have same names but are actually different
    std::sort(allTests.begin(), allTests.end(), [](const PerformanceIntegrationTestBase* a, const PerformanceIntegrationTestBase* b)
    {
        auto& aLayout = *a->GetPerformanceTestDataLayout();
        auto& bLayout = *b->GetPerformanceTestDataLayout();

        if (aLayout.GetLayoutName() == bLayout.GetLayoutName())
            return a->TestName() < b->TestName();

        return aLayout.GetLayoutName() < bLayout.GetLayoutName();
    });

    for (size_t i = 1; i < allTests.size(); i++)
    {
        auto a = allTests[i - 1]->GetPerformanceTestDataLayout();
        auto b = allTests[i]->GetPerformanceTestDataLayout();
        if (a->GetLayoutName() == b->GetLayoutName())
            CHECK(a == b, std::format(L"Two performance test layouts have the same name '{}' but are different", a->GetLayoutName()));
    }

    // Now, run the tests
    const PerformanceTestDataLayout* currentLayout = nullptr;
    std::optional<PreparedPerformanceTestLayout> preparedLayout;

    for (const auto& test : allTests)
    {
        //if (test->TestName().find(L"FileContents_BinaryFiles_LongSearchString_UTF16") == std::wstring_view::npos)
        //    continue;

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
}

struct TestStringSearcherDeleter
{
    void operator()(TestStringSearcher* ptr) const 
    {
        FreeStringSearcher(ptr);
    }
};

static void RunAllStringSearchPerformanceTests(int& failureCount, int& successCount, int& testCount)
{
    using namespace Testing;

    std::vector<StringSearchPerformanceTest*> allTests;

    {
        auto test = StringSearchPerformanceTest::GetFirstTest();
        while (test != nullptr)
        {
            allTests.push_back(test);
            test = test->GetNextTest();
        }
    }

    std::sort(allTests.begin(), allTests.end(), [](const StringSearchPerformanceTest* a, const StringSearchPerformanceTest* b)
    {
        return a->TestName() < b->TestName();
    });

    std::map<StringSearcherParameters, std::unique_ptr<TestStringSearcher, TestStringSearcherDeleter>> stringSearchers;
    std::map<std::wstring_view, std::vector<uint8_t>> fileContents;

    for (const auto& test : allTests)
    {
        auto searcherParameters = test->GetSearcherParameters();
        if (stringSearchers.find(searcherParameters) == stringSearchers.end())
            stringSearchers.emplace(searcherParameters, CreateStringSearcher(searcherParameters.searchString, searcherParameters.searchFlags));

        auto fileToSearch = test->GetFileToSearch();
        if (fileContents.find(fileToSearch) == fileContents.end())
            fileContents.emplace(fileToSearch, ReadWholeFile<std::vector<uint8_t>>(test->GetFileToSearch()));
    }

    for (const auto& test : allTests)
    {
        //if (test->TestName().find(L"StringSearchPerformanceTest_UnicodeSearchString_Utf16IgnoreCaseSearchFlags_LargeSourceFile_DirectStorageReaderChunkSize") == std::wstring_view::npos)
        //    continue;

        try
        {
            PrintToStdout(std::format(L"Running '{}'\r\n", test->TestName()));
            testCount++;
            test->Run(stringSearchers.at(test->GetSearcherParameters()).get(), fileContents.at(test->GetFileToSearch()));
            successCount++;
            PrintToStdout(L"    PASS!\r\n");
        }
        catch (const TestFailedException& e)
        {
            failureCount++;
            PrintToStdout(std::format(L"    FAILED:\r\n        {}\r\n", e.Text()));
        }
    }
}

int Testing::RunAllPerformanceTests()
{
    try
    {
        GlobalTestContext globalTestContext;

        int failureCount = 0;
        int successCount = 0;
        int testCount = 0;
        int layoutCount = 0;
        RunAllStringSearchPerformanceTests(failureCount, successCount, testCount);
        RunAllPerformanceIntegrationTests(failureCount, successCount, testCount, layoutCount);

        PrintToStdout(std::format(L"Ran {} tests across {} layouts: {} succeeded, {} failed.\r\n", testCount, layoutCount, successCount, failureCount));
        return failureCount;
    }
    catch (const TestFailedException& e)
    {
        PrintToStdout(std::format(L"Tests encountered a fatal error outside of any single test: {}\r\n", e.Text()));
        return std::numeric_limits<int>::max();
    }
}

struct TestResult
{
    std::wstring_view testName;
    double medianRunTime;
};

template <typename PerformanceTestType>
void GatherTestResults(std::vector<TestResult>& testResults)
{
    auto test = PerformanceTestType::GetFirstTest();
    while (test != nullptr)
    {
        testResults.emplace_back(test->TestName(), test->GetMedianRunTime());
        test = test->GetNextTest();
    }
}

void Testing::ReportPerformanceTestResults()
{
    PrintToStdout(std::format(L"\r\nPerformance Test Results:\r\n"));

    std::vector<TestResult> testResults;
    GatherTestResults<StringSearchPerformanceTest>(testResults);
    GatherTestResults<PerformanceIntegrationTestBase>(testResults);

    if (testResults.empty())
    {
        PrintToStdout(std::format(L"    No performance tests were run.\r\n"));
        return;
    }

    std::sort(testResults.begin(), testResults.end(), [](const TestResult& a, const TestResult& b)
    {
        return a.testName < b.testName;
    });

    for (auto& test : testResults)
    {
        auto name = test.testName;
        if (isnan(test.medianRunTime))
            continue;

        auto durationMS = 1000.0 * test.medianRunTime;
        PrintToStdout(std::format(L"    {:100}: {:.2f} ms\r\n", name, durationMS));
    }
}
