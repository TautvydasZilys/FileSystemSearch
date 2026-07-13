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
}
