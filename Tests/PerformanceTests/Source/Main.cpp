#include "PrecompiledHeader.h"
#include "PerformanceTest.h"

int main()
{
    SetThreadDescription(GetCurrentThread(), L"Performance Test Main Thread");

     auto failCount = Testing::RunAllPerformanceTests();

     if (failCount == 0)
         Testing::ReportPerformanceTestResults();

     return failCount;
}