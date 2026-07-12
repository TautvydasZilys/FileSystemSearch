#include "PrecompiledHeader.h"
#include "StringUtils.h"
#include "TestHelpers.h"
#include "TestMacros.h"
#include "TestList.h"

#include <format>
#include <iostream>

static void PrintToStdout(std::string_view text)
{
    CHECK(text.length() <= std::numeric_limits<DWORD>::max(), L"Text is too long to write to stdout!");

    auto stdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD bytesWritten;
    auto writeResult = WriteFile(stdOut, text.data(), static_cast<DWORD>(text.length()), &bytesWritten, nullptr);
    CHECK(writeResult && bytesWritten == text.length(), L"Failed to write to stdout");

    FlushFileBuffers(stdOut);
}

static void PrintToStdout(std::wstring_view wideText)
{
    PrintToStdout(StringUtils::Utf16ToUtf8(wideText));
}

template <typename TestType>
int Testing::RunAllTests()
{
    try
    {
        GlobalTestContext globalTestContext;

        auto test = TestType::GetFirstTest();

        int failureCount = 0;
        int successCount = 0;
        int testCount = 0;

        while (test != nullptr)
        {
            try
            {
                PrintToStdout(std::format(L"Running '{}'\r\n", test->TestName().data()));
                testCount++;
                test->Run();
                successCount++;
                PrintToStdout(L"    PASS!\r\n");
            }
            catch (const TestFailedException& e)
            {
                failureCount++;
                PrintToStdout(std::format(L"    FAILED:\r\n        {}\r\n", e.Text()));
            }

            test = test->GetNextTest();
        }

        PrintToStdout(std::format(L"Ran {} tests: {} succeeded, {} failed.\r\n", testCount, successCount, failureCount));

        return failureCount;
    }
    catch (const TestFailedException& e)
    {
        PrintToStdout(std::format(L"Tests encountered a fatal error outside of any single test: {}\r\n", e.Text()));
        return std::numeric_limits<int>::max();
    }
}

template int Testing::RunAllTests<Testing::FunctionalTest>();
template int Testing::RunAllTests<Testing::PerformanceTestBase>();

void Testing::ReportPerformanceTestResults()
{
    auto test = PerformanceTestBase::GetFirstTest();
    if (test == nullptr)
        return;

    PrintToStdout(std::format(L"\r\nPerformance Test Results:\r\n"));

    while (test != nullptr)
    {
        auto name = test->TestName();
        auto durationMS = 1000.0 * test->GetMedianRunTime();

        PrintToStdout(std::format(L"    {:80}: {:.2f} ms\r\n", name, durationMS));
        test = test->GetNextTest();
    }
}
