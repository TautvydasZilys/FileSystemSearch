#include "PrecompiledHeader.h"
#include "HandleHolder.h"
#include "TestMacros.h"
#include "PerformanceIntegrationTest.h"
#include "SearchStrings.h"

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

struct BinaryFiles
{
    static constexpr auto TestFiles = MergeArrays(
        MakeArray<3>(D3D10Warp),
        MakeArray<3>(DbgEng),
        MakeArray<2>(Shell32),
        MakeArray<2>(WindowsStorage),
        MakeArray<1>(WindowsUIXaml)
    );
};

struct LargeSourceFiles
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { CoreCLRGenTree };
};

struct MediumSourceFiles
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { CoreCLRClass };
};

struct SmallSourceFiles
{
    static constexpr std::array<CompileTimeString<wchar_t, MAX_PATH>, 1> TestFiles = { CoreCLRUnwind };
};

struct MixedFilesByCount
{
    static constexpr auto TestFiles = MergeArrays(std::to_array({ D3D10Warp, DbgEng, Shell32, WindowsStorage, WindowsUIXaml }), LargeSourceFiles::TestFiles, MediumSourceFiles::TestFiles, SmallSourceFiles::TestFiles);
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
