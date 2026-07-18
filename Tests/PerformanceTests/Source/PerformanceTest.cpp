#include "PrecompiledHeader.h"
#include "PerformanceTest.h"
#include "PerformanceIntegrationTest.h"
#include "StringSearcherTestAPI.h"
#include "StringSearchPerformanceTest.h"
#include "StringUtils.h"
#include "TestLogger.h"
#include "Utilities/FileUtilities.h"

template <typename T> requires std::derived_from<T, Testing::RegisteredTest<T>> && std::derived_from<T, Testing::ITest>
static std::vector<T*> GatherAllTests()
{
    std::vector<T*> allTests;

    {
        auto test = T::GetFirstTest();
        while (test != nullptr)
        {
            allTests.push_back(test);
            test = test->GetNextTest();
        }
    }

    std::sort(allTests.begin(), allTests.end(), [](const T* a, const T* b)
    {
        return a->TestName() < b->TestName();
    });

    for (size_t i = 1; i < allTests.size(); i++)
        CHECK(allTests[i - 1]->TestName() != allTests[i]->TestName(), std::format(L"Two tests have the same name: '{}' and '{}'", allTests[i - 1]->TestName(), allTests[i]->TestName()));
    
    return allTests;
}

static void RunAllPerformanceIntegrationTests(int& failureCount, int& successCount, int& testCount, int& layoutCount)
{
    using namespace Testing;

    auto allTests = GatherAllTests<PerformanceIntegrationTestBase>();

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

    auto allTests = GatherAllTests<StringSearchPerformanceTest>();
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
    std::string testName;
    std::wstring_view testNameUtf16;
    double baselineMedianRunTimeMS;
    double actualMedianRunTimeMS;
};

template <typename PerformanceTestType>
void GatherTestResults(std::vector<TestResult>& testResults)
{
    auto test = PerformanceTestType::GetFirstTest();
    while (test != nullptr)
    {
        testResults.emplace_back(StringUtils::Utf16ToUtf8(test->TestName()), test->TestName(), NAN, test->GetMedianRunTime() * 1000.0);
        test = test->GetNextTest();
    }
}

void Testing::ReportPerformanceTestResults()
{
    using std::operator""sv;

    std::vector<TestResult> testResults;
    GatherTestResults<StringSearchPerformanceTest>(testResults);
    GatherTestResults<PerformanceIntegrationTestBase>(testResults);

    if (testResults.empty())
    {
        PrintToStdout(std::format(L"No performance tests were run.\r\n"));
        return;
    }

    std::sort(testResults.begin(), testResults.end(), [](const TestResult& a, const TestResult& b)
    {
        return a.testName < b.testName;
    });

    constexpr auto kBaselineFilePath = GetPerformanceTestDataDirectory() + L"\\Baseline.txt";
    constexpr auto kTestComparisonFilePath = GetPerformanceTestDataDirectory() + L"\\Comparison.txt";

    FileHandleHolder baselineFile = CreateFile2(kBaselineFilePath.value, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS, nullptr);    
    auto baselineText = ReadFileToEnd<std::string>(baselineFile, kBaselineFilePath.value);

    size_t lineNumber = 0;
    for (auto line : std::views::split(baselineText, '\n'))
    {
        lineNumber++;
        if (line.empty())
            continue;

        std::string_view lineView(line.begin(), line.end());
        auto commaPos = lineView.find(',');
        if (commaPos == std::string_view::npos || commaPos == 0 || commaPos == lineView.size() - 1)
        {
            PrintToStdout(std::format("Error parsing '{}': Invalid line format at line {}: '{}'\r\n", StringUtils::Utf16ToUtf8(kBaselineFilePath.value), lineNumber, lineView));
            continue;
        }

        auto testName = lineView.substr(0, commaPos);
        auto timeStr = lineView.substr(commaPos + 1);

        double baselineTimeMS;
        auto [ptr, err] = std::from_chars(timeStr.data(), timeStr.data() + timeStr.size(), baselineTimeMS);
        if (err != std::errc())
        {
            PrintToStdout(std::format("Error parsing '{}': Invalid number format at line {}: '{}'\r\n", StringUtils::Utf16ToUtf8(kBaselineFilePath.value), lineNumber, timeStr));
            continue;
        }

        auto it = std::lower_bound(testResults.begin(), testResults.end(), testName, [](const TestResult& a, std::string_view b)
        {
            return a.testName < b;
        });
        
        if (it != testResults.end() && it->testName == testName)
        {
            if (!isnan(it->baselineMedianRunTimeMS))
                PrintToStdout(std::format("WARNING: duplicate baseline for test '{}': {} ms and {} ms\r\n", it->testName, it->baselineMedianRunTimeMS, baselineTimeMS));

            it->baselineMedianRunTimeMS = baselineTimeMS;
        }
        else
        {
            PrintToStdout(std::format("WARNING: baseline for test '{}' in '{}' does not match any test that was run. Was the test deleted?\r\n", testName, StringUtils::Utf16ToUtf8(kBaselineFilePath.value)));
        }
    }

    std::string baselineOutput;
    std::string comparisonOutput;

    for (auto& test : testResults)
    {
        // Only change baseline if it's a new test or we have a significant (>5%) performance difference. 
        // This is to avoid changing the baseline for small fluctuations in performance that may be due to other factors.
        bool changeBaseline = false;
        if (isnan(test.actualMedianRunTimeMS))
        {
            if (isnan(test.baselineMedianRunTimeMS))
                continue;
        }
        else if (isnan(test.baselineMedianRunTimeMS))
        {
            changeBaseline = true;
            comparisonOutput += std::format("{:150}: {} ms (new test)\r\n", test.testName, test.actualMedianRunTimeMS);
        }
        else
        {
            if (test.baselineMedianRunTimeMS < test.actualMedianRunTimeMS)
            {
                auto slowdownPercentage = (test.actualMedianRunTimeMS - test.baselineMedianRunTimeMS) / test.baselineMedianRunTimeMS * 100.0;
                if (slowdownPercentage > 5.0)
                {
                    changeBaseline = true;
                    comparisonOutput += std::format("{:150}: {} ms (slower than baseline by {} ms, {:.2f}%)\r\n", test.testName, test.actualMedianRunTimeMS, (test.actualMedianRunTimeMS - test.baselineMedianRunTimeMS), slowdownPercentage);
                }
            }
            else
            {
                auto speedupPercentage = (test.baselineMedianRunTimeMS - test.actualMedianRunTimeMS) / test.baselineMedianRunTimeMS * 100.0;
                if (speedupPercentage > 5.0)
                {
                    changeBaseline = true;
                    comparisonOutput += std::format("{:150}: {} ms (faster than baseline by {} ms, {:.2f}%)\r\n", test.testName, test.actualMedianRunTimeMS, (test.baselineMedianRunTimeMS - test.actualMedianRunTimeMS), speedupPercentage);
                }
            }
        }

        baselineOutput += test.testName;
        baselineOutput += std::format(",{:.14f}\r\n", changeBaseline ? test.actualMedianRunTimeMS : test.baselineMedianRunTimeMS);
    }

    SetFilePointer(baselineFile, 0, nullptr, FILE_BEGIN);
    WriteWholeFile(baselineFile, kBaselineFilePath.value, baselineOutput);
    SetEndOfFile(baselineFile);

    WriteWholeFile(kTestComparisonFilePath.value, comparisonOutput);
}
