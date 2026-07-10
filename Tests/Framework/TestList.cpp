#include "PrecompiledHeader.h"
#include "StringUtils.h"
#include "TestHelpers.h"
#include "TestMacros.h"
#include "TestList.h"

#include <format>
#include <iostream>

static Testing::ITest* s_FirstRegisteredTest;
static Testing::ITest* s_LastRegisteredTest;

void Testing::RegisterTest(ITest* test)
{
    if (s_FirstRegisteredTest == nullptr)
        s_FirstRegisteredTest = test;

    if (s_LastRegisteredTest != nullptr)
        s_LastRegisteredTest->next = test;

    s_LastRegisteredTest = test;
}

static void PrintToStdout(std::string_view text)
{
    CHECK(text.length() <= std::numeric_limits<DWORD>::max(), L"Text is too long to write to stdout!");

    DWORD bytesWritten;
    auto writeResult = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text.data(), static_cast<DWORD>(text.length()), &bytesWritten, nullptr);
    CHECK(writeResult && bytesWritten == text.length(), L"Failed to write to stdout");
}

static void PrintToStdout(std::wstring_view wideText)
{
    PrintToStdout(StringUtils::Utf16ToUtf8(wideText));
}

int Testing::RunAllTests()
{
    try
    {
        GlobalTestContext globalTestContext;

        auto test = s_FirstRegisteredTest;

        int failureCount = 0;
        int successCount = 0;
        int testCount = 0;

        while (test != nullptr)
        {
            try
            {
                testCount++;
                test->Run();
                successCount++;
            }
            catch (const TestFailedException& e)
            {
                failureCount++;
                PrintToStdout(std::format(L"'{}' FAILED:\n    {}\r\n", test->TestName().data(), e.Text()));
            }

            test = test->next;
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
