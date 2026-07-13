#include "PrecompiledHeader.h"
#include "HandleHolder.h"
#include "TestMacros.h"
#include "PerformanceIntegrationTest.h"

template <const Testing::PerformanceTestDataLayout* Layout, typename SearchString>
struct FileContentsPathPerformanceTest : SearchString
{
    static constexpr const Testing::PerformanceTestDataLayout* PerformanceTestDataLayout = Layout;
    static constexpr SearchFlags SearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents;
};

// Define the binary files layout to be smaller than the source files layout, as our test binary files are very large and we want to limit the test disk footprint and the time it takes to run the tests.
struct BinaryFilesSearchLayout
{
    static constexpr std::array<size_t, 2> LayoutSizes = { 5, 20 };
};

struct SourceFilesSearchLayout
{
    static constexpr std::array<size_t, 2> LayoutSizes = { 30, 20 };
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

template <CompileTimeStringW RelativePath>
consteval CompileTimeString<wchar_t, MAX_PATH> GetTestSourceFile()
{
    return Testing::GetPerformanceTestDataDirectory() + L"\\" + RelativePath;
}

template <typename T, std::size_t... Sizes>
constexpr auto MergeArrays(const std::array<T, Sizes>&... arrays)
{
    std::array<T, (Sizes + ...)> result{};
    auto it = result.begin();
    ((it = std::ranges::copy(arrays, it).out), ...);
    return result;
}

template <size_t N, typename T>
constexpr auto MakeArray(const T& value)
{
    std::array<T, N> result{};
    result.fill(value);
    return result;
}

template <size_t N, typename T>
constexpr auto MakeArray(const std::array<T, 1>& value)
{
    std::array<T, N> result{};
    result.fill(value[0]);
    return result;
}

static constexpr CompileTimeStringW<MAX_PATH> kD3D10Warp = LR"(C:\Windows\System32\d3d10warp.dll)";
static constexpr CompileTimeStringW<MAX_PATH> kDbgEng = LR"(C:\Windows\System32\dbgeng.dll)";
static constexpr CompileTimeStringW<MAX_PATH> kShell32 = LR"(C:\Windows\System32\shell32.dll)";
static constexpr CompileTimeStringW<MAX_PATH> kWindowsStorage = LR"(C:\Windows\System32\Windows.Storage.dll)";
static constexpr CompileTimeStringW<MAX_PATH> kWindowsUIXaml = LR"(C:\Windows\System32\Windows.UI.Xaml.dll)";

struct BinaryFiles
{
    static constexpr auto TestFiles = MergeArrays(
        MakeArray<3>(kD3D10Warp),
        MakeArray<3>(kDbgEng),
        MakeArray<2>(kShell32),
        MakeArray<2>(kWindowsStorage),
        MakeArray<1>(kWindowsUIXaml)
    );
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

struct MixedFilesByCount
{
    static constexpr auto TestFiles = MergeArrays(std::to_array({ kD3D10Warp, kDbgEng, kShell32, kWindowsStorage, kWindowsUIXaml }), LargeSourceFiles::TestFiles, MediumSourceFiles::TestFiles, SmallSourceFiles::TestFiles);
};

struct MixedFilesBySize
{
    static constexpr auto TestFiles = MergeArrays(BinaryFiles::TestFiles, MakeArray<14>(LargeSourceFiles::TestFiles), MakeArray<154>(MediumSourceFiles::TestFiles));
};

#define DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(Files, FileLayout) static const Testing::PerformanceTestDataLayout k##Files##Layout { L#Files, FileLayout::LayoutSizes, Files::TestFiles }

DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(BinaryFiles, BinaryFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(LargeSourceFiles, SourceFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(MediumSourceFiles, SourceFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(SmallSourceFiles, SourceFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(MixedFilesByCount, BinaryFilesSearchLayout);
DEFINE_CONTENT_PERFORMANCE_TEST_DATA_LAYOUT(MixedFilesBySize, SourceFilesSearchLayout);

#define DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(Files, SearchString) DEFINE_PERFORMANCE_INTEGRATION_TEST(FileContents_##Files##_##SearchString, FileContentsPathPerformanceTest<&k##Files##Layout, SearchString>)

#define DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(SearchString) \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(BinaryFiles, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(LargeSourceFiles, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(MediumSourceFiles, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(SmallSourceFiles, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(MixedFilesByCount, SearchString); \
    DEFINE_FILE_CONTENTS_PERFORMANCE_TEST(MixedFilesBySize, SearchString);

DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(ShortSearchString);
DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(LongSearchString);
DEFINE_FILE_CONTENTS_PERFORMANCE_TESTS(UnicodeSearchString);
