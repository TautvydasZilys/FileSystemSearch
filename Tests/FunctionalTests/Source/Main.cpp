#include "PrecompiledHeader.h"
#include "TestList.h"

int main()
{
    SetThreadDescription(GetCurrentThread(), L"Functional Test Main Thread");
    return Testing::RunAllFunctionalTests();
}