#include "PrecompiledHeader.h"
#include "HandleHolder.h"
#include "TestMacros.h"
#include "TestHelpers.h"
#include "PerformanceTest.h"

template <typename SearchString, typename Files>
struct FileContentsPathPerformanceTest : SearchString, Files
{
    static constexpr std::array<size_t, 2> LayoutSizes = { 10, 50 };
    static constexpr SearchFlags SearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents;
};

struct ShortSearchString
{
    static constexpr const wchar_t* SearchString = L"System";
};

struct LongSearchString
{
    static constexpr const wchar_t* SearchString = L"static constexpr std::array<size_t, 5> LayoutSizes = { 4, 4, 4, 5 };";
};

struct UnicodeSearchString
{
    static constexpr const wchar_t* SearchString = L"Gąsdindamas ąsotį gręžiantį žąsiną, žvejys tąsė įsipainiojusį vėžį.";
};

template <CompileTimeStringW FilePath>
consteval auto GetDirectoryName()
{
    constexpr size_t lastBackslash = std::find(FilePath.value, FilePath.value + FilePath.Length, L'\\') - FilePath.value;
    return FilePath.template SubStr<0, lastBackslash>();
}

template <CompileTimeStringW RelativePath>
consteval CompileTimeString<wchar_t, MAX_PATH> GetTestSourceFile()
{
    constexpr auto filePath = CompileTimeString(__FILEW__);
    constexpr size_t lastBackslash = std::find(filePath.value, filePath.value + filePath.Length, L'\\') - filePath.value;
    return filePath.template SubStr<0, lastBackslash>() + L"\\TestData\\" + RelativePath;
}

struct BinaryFile
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { LR"(C:\Windows\System32\Windows.UI.Xaml.dll)" };
};

struct LargeSourceFile
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { GetTestSourceFile<L"CoreCLR-source\\gentree.cpp">() };
};

struct MediumSourceFile
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { GetTestSourceFile<L"CoreCLR-source\\class.cpp">() };
};

struct SmallSourceFile
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { GetTestSourceFile<L"CoreCLR-source\\unwind.cpp">() };
};

#define DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(SearchString, Files) DEFINE_PERFORMANCE_TEST(FileContents_##SearchString##_##Files, FileContentsPathPerformanceTest<SearchString, Files>)

#define DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(SearchString) \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(SearchString, BinaryFile); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(SearchString, LargeSourceFile); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(SearchString, MediumSourceFile); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(SearchString, SmallSourceFile);

DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(ShortSearchString);
DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(LongSearchString);
DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(UnicodeSearchString);
