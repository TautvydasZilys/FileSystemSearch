#pragma once

#include "SearchEngineTypes.h"
#include "TestMacros.h"
#include "TestHelpers.h"
#include "Utilities/CompileTimeString.h"

namespace Testing
{
    int RunAllPerformanceTests();
    void ReportPerformanceTestResults();

    inline consteval auto GetPerformanceTestDataDirectory()
    {
        constexpr auto filePath = CompileTimeString(__FILEW__);
        constexpr size_t lastBackslash = (std::find(std::rbegin(filePath.value), std::rend(filePath.value), L'\\').base() - 1) - filePath.value;
        return filePath.template SubStr<0, lastBackslash>() + L"\\TestData";
    }

    template <typename IterationInit, typename IterationRun, typename IterationCleanup>
    inline double RunPerformanceTestIterations(size_t iterationCount, IterationInit&& initFunc, IterationRun&& runFunc, IterationCleanup&& cleanupFunc)
    {
        std::vector<double> measurements;
        measurements.resize(iterationCount);

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);

        for (size_t i = 0; i < iterationCount; i++)
        {
            LARGE_INTEGER start, end;
            initFunc();

            if constexpr (std::is_same_v<std::invoke_result_t<IterationRun>, void>)
            {
                QueryPerformanceCounter(&start);
                runFunc();
                QueryPerformanceCounter(&end);

                cleanupFunc(i);
            }
            else
            {
                QueryPerformanceCounter(&start);
                auto result = runFunc();
                QueryPerformanceCounter(&end);

                cleanupFunc(i, result);
            }

            measurements[i] = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
        }

        std::sort(measurements.begin(), measurements.end());

        if (iterationCount % 2 == 0)
            return (measurements[iterationCount / 2 - 1] + measurements[iterationCount / 2]) / 2.0;

        return measurements[iterationCount / 2];
    }
}
