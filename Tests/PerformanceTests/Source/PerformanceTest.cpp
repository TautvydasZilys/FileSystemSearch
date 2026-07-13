#include "PrecompiledHeader.h"
#include "PerformanceTest.h"
#include "PerformanceIntegrationTest.h"
#include "TestLogger.h"

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

int Testing::RunAllPerformanceTests()
{
    try
    {
        GlobalTestContext globalTestContext;

        int failureCount = 0;
        int successCount = 0;
        int testCount = 0;
        int layoutCount = 0;
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

void Testing::ReportPerformanceTestResults()
{
    PrintToStdout(std::format(L"\r\nPerformance Test Results:\r\n"));

    std::vector<PerformanceIntegrationTestBase*> allTests;

    {
        auto test = PerformanceIntegrationTestBase::GetFirstTest();
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

    std::sort(allTests.begin(), allTests.end(), [](const PerformanceIntegrationTestBase* a, const PerformanceIntegrationTestBase* b)
    {
        return a->TestName() < b->TestName();
    });

    for (auto test : allTests)
    {
        auto name = test->TestName();
        if (isnan(test->GetMedianRunTime()))
            continue;

        auto durationMS = 1000.0 * test->GetMedianRunTime();

        PrintToStdout(std::format(L"    {:100}: {:.2f} ms\r\n", name, durationMS));
    }
}
