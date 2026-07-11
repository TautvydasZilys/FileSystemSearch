#include "PrecompiledHeader.h"
#include "PerformanceTest.h"

template <typename SearchLayout, typename SearchString, SearchFlags Flags>
struct FileNamePathPerformanceTest : SearchLayout, SearchString
{
    static constexpr SearchFlags SearchFlags = Flags;
};

struct WideSearch
{
    static constexpr std::array<size_t, 2> LayoutSizes = { 20, 100 };
};

struct DeepSearch
{
    static constexpr std::array<size_t, 5> LayoutSizes = { 4, 4, 4, 4, 5 };
};

struct LittleMatches
{
    static constexpr const wchar_t* SearchString = L"2bcB";
};

struct LotsOfMatches
{
    static constexpr const wchar_t* SearchString = L"C";
};

constexpr SearchFlags kFileNameSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively;
constexpr SearchFlags kFilePathSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively;
constexpr SearchFlags kDirectoryNameSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively;
constexpr SearchFlags kDirectoryPathSearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively;

#define DEFINE_NAME_PATH_PERFORMANCE_TEST(Category, SearchLayout, SearchString) DEFINE_PERFORMANCE_TEST(Category##_##SearchLayout##_##SearchString, FileNamePathPerformanceTest<SearchLayout, SearchString, k##Category##SearchFlags>)

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
