#include "PrecompiledHeader.h"
#include "HandleHolder.h"
#include "TestMacros.h"
#include "TestHelpers.h"
#include "PerformanceTest.h"

template <const Testing::PerformanceTestDataLayout* Layout, typename SearchString>
struct FileContentsPathPerformanceTest : SearchString
{
    static constexpr const Testing::PerformanceTestDataLayout* PerformanceTestDataLayout = Layout;
    static constexpr SearchFlags SearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents;
};

// Define the binary files layout to be smaller than the source files layout, as our test binary files are very large and we want to limit the test disk footprint.
struct BinaryFilesSearchLayout
{
    static constexpr std::array<size_t, 2> LayoutSizes = { 5, 100 };
};

struct SourceFilesSearchLayout
{
    static constexpr std::array<size_t, 2> LayoutSizes = { 30, 100 };
};

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

struct BinaryFiles
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { LR"(C:\Windows\System32\Windows.UI.Xaml.dll)" };
};

struct LargeSourceFiles
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { GetTestSourceFile<L"CoreCLR-source\\gentree.cpp">() };
};

struct MediumSourceFiles
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { GetTestSourceFile<L"CoreCLR-source\\class.cpp">() };
};

struct SmallSourceFiles
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { GetTestSourceFile<L"CoreCLR-source\\unwind.cpp">() };
};

#define DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(Files, FileLayout) static const Testing::PerformanceTestDataLayout k##Files##Layout { L#Files, FileLayout::LayoutSizes, Files::TestFiles }

DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(BinaryFiles, BinaryFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(LargeSourceFiles, SourceFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(MediumSourceFiles, SourceFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(SmallSourceFiles, SourceFilesSearchLayout);

#define DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(Files, SearchString) DEFINE_PERFORMANCE_TEST(FileContents_##Files##_##SearchString, FileContentsPathPerformanceTest<&k##Files##Layout, SearchString>)

#define DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(SearchString) \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(BinaryFiles, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(LargeSourceFiles, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(MediumSourceFiles, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(SmallSourceFiles, SearchString);

DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(ShortSearchString);
DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(LongSearchString);
DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(UnicodeSearchString);
