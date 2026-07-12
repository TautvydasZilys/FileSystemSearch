#include "PrecompiledHeader.h"
#include "PerformanceTest.h"

int main()
{
     auto failCount = Testing::RunAllPerformanceTests();

     if (failCount == 0)
         Testing::ReportPerformanceTestResults();

     return failCount;
}