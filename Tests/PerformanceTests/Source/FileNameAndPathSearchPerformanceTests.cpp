#include "PrecompiledHeader.h"
#include "PerformanceTest.h"

template <const Testing::PerformanceTestDataLayout* Layout, typename SearchString, SearchFlags Flags>
struct FileNamePathPerformanceTest : SearchString
{
    static constexpr const Testing::PerformanceTestDataLayout* PerformanceTestDataLayout = Layout;
    static constexpr SearchFlags SearchFlags = Flags;
};

struct WideSearch
{
    static constexpr std::array<size_t, 2> LayoutSizes = { 20, 20 };
};

struct DeepSearch
{
    static constexpr std::array<size_t, 5> LayoutSizes = { 4, 4, 4, 5 };
};

struct LittleMatches
{
    static constexpr CompileTimeStringW SearchString = L"2bcB";
};

struct LotsOfMatches
{
    static constexpr CompileTimeStringW SearchString = L"C";
};

#define DEFINE_PERFORMANCE_TEST_DATA_LAYOUT(SearchLayout) static const Testing::PerformanceTestDataLayout k##SearchLayout##Layout { L#SearchLayout, SearchLayout::LayoutSizes }

DEFINE_PERFORMANCE_TEST_DATA_LAYOUT(WideSearch);
DEFINE_PERFORMANCE_TEST_DATA_LAYOUT(DeepSearch);

constexpr SearchFlags kFileNameSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively;
constexpr SearchFlags kFilePathSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively;
constexpr SearchFlags kDirectoryNameSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively;
constexpr SearchFlags kDirectoryPathSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively;

#define DEFINE_NAME_PATH_PERFORMANCE_TEST(Category, SearchLayout, SearchString) DEFINE_PERFORMANCE_TEST(Category##_##SearchLayout##_##SearchString, FileNamePathPerformanceTest<&k##SearchLayout##Layout, SearchString, k##Category##SearchFlags>)

#define DEFINE_PERFORMANCE_TEST_SEARCH_STRING_COMBOS(Category, SearchLayout) \
    DEFINE_NAME_PATH_PERFORMANCE_TEST(Category, SearchLayout, LittleMatches); \
    DEFINE_NAME_PATH_PERFORMANCE_TEST(Category, SearchLayout, LotsOfMatches)

#define DEFINE_NAME_PATH_PERFORMANCE_TESTS(Category) \
    DEFINE_PERFORMANCE_TEST_SEARCH_STRING_COMBOS(Category, WideSearch); \
    DEFINE_PERFORMANCE_TEST_SEARCH_STRING_COMBOS(Category, DeepSearch)

DEFINE_NAME_PATH_PERFORMANCE_TESTS(FileName);
DEFINE_NAME_PATH_PERFORMANCE_TESTS(FilePath);
DEFINE_NAME_PATH_PERFORMANCE_TESTS(DirectoryName);
DEFINE_NAME_PATH_PERFORMANCE_TESTS(DirectoryPath);
