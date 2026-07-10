#include "PrecompiledHeader.h"
#include "TestHelpers.h"
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
                std::wcout << std::format(L"'{}' FAILED:\n    {}", test->TestName().data(), e.Text()) << std::endl;
            }

            test = test->next;
        }

        std::wcout << std::format(L"Ran {} tests: {} succeeded, {} failed.", testCount, successCount, failureCount) << std::endl;

        return failureCount;
    }
    catch (const TestFailedException& e)
    {
        std::wcout << std::format(L"Tests encountered a fatal error outside of any single test: {}", e.Text()) << std::endl;
        return std::numeric_limits<int>::max();
    }
}
