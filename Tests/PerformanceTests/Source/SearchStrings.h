#pragma once
#include "Utilities/CompileTimeString.h"
#include "PerformanceTest.h"

struct ShortSearchString
{
    static constexpr CompileTimeStringW SearchString = L"System";
};

struct LongSearchString
{
    static constexpr CompileTimeStringW SearchString = L"static constexpr std::array<size_t, 5> LayoutSizes = { 4, 4, 4, 5 };";
};

struct UnicodeSearchString
{
    static constexpr CompileTimeStringW SearchString = L"Gąsdindamas ąsotį gręžiantį žąsiną, žvejys tąsė įsipainiojusį vėžį.";
};

template <CompileTimeStringW RelativePath>
consteval CompileTimeString<wchar_t, MAX_PATH> GetTestSourceFile()
{
    return Testing::GetPerformanceTestDataDirectory() + L"\\" + RelativePath;
}

static constexpr CompileTimeStringW<MAX_PATH> D3D10Warp = LR"(C:\Windows\System32\d3d10warp.dll)";
static constexpr CompileTimeStringW<MAX_PATH> DbgEng = LR"(C:\Windows\System32\dbgeng.dll)";
static constexpr CompileTimeStringW<MAX_PATH> Shell32 = LR"(C:\Windows\System32\shell32.dll)";
static constexpr CompileTimeStringW<MAX_PATH> WindowsStorage = LR"(C:\Windows\System32\Windows.Storage.dll)";
static constexpr CompileTimeStringW<MAX_PATH> WindowsUIXaml = LR"(C:\Windows\System32\Windows.UI.Xaml.dll)";
static constexpr CompileTimeStringW<MAX_PATH> CoreCLRGenTree = GetTestSourceFile<L"CoreCLR-source\\gentree.cpp">();
static constexpr CompileTimeStringW<MAX_PATH> CoreCLRClass = GetTestSourceFile<L"CoreCLR-source\\class.cpp">();
static constexpr CompileTimeStringW<MAX_PATH> CoreCLRUnwind = GetTestSourceFile<L"CoreCLR-source\\unwind.cpp">();