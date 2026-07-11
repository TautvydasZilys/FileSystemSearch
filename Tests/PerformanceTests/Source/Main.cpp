#include "PrecompiledHeader.h"
#include "TestList.h"

int main()
{
     auto failCount = Testing::RunAllTests<Testing::PerformanceTestBase>();

     if (failCount == 0)
         Testing::ReportPerformanceTestResults();

     return failCount;
}