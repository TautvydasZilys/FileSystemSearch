#include "PrecompiledHeader.h"
#include "PerformanceTest.h"
#include "StringUtils.h"
#include "TestHelpers.h"
#include "TestMacros.h"
#include "TestList.h"
#include "TestLogger.h"

#include <format>
#include <iostream>

int Testing::RunAllFunctionalTests()
{
    try
    {
        GlobalTestContext globalTestContext;

        auto test = FunctionalTest::GetFirstTest();

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
