#include "PrecompiledHeader.h"
#include "StringSearchPerformanceTest.h"
#include "SearchStrings.h"

constexpr SearchFlags Utf8SearchFlags = SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8;
constexpr SearchFlags Utf8IgnoreCaseSearchFlags = SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase;
constexpr SearchFlags Utf16SearchFlags = SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf16;
constexpr SearchFlags Utf16IgnoreCaseSearchFlags = SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase;
constexpr SearchFlags Utf8Utf16SearchFlags = SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16;
constexpr SearchFlags Utf8Utf16IgnoreCaseSearchFlags = SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase;

constexpr uint32_t OverlappedReaderChunkSize = 5 * 1024 * 1024; // 5 MB
constexpr uint32_t DirectStorageReaderChunkSize = 128 * 1024; // 128 KB

struct LargeBinaryFile
{
    static constexpr CompileTimeStringW<MAX_PATH> SearchString = WindowsUIXaml;
};

struct MediumBinaryFile
{
    static constexpr CompileTimeStringW<MAX_PATH> SearchString = D3D10Warp;
};

struct LargeSourceFile
{
    static constexpr CompileTimeStringW<MAX_PATH> SearchString = CoreCLRGenTree;
};

struct MediumSourceFile
{
    static constexpr CompileTimeStringW<MAX_PATH> SearchString = CoreCLRClass;
};

struct SmallSourceFile
{
    static constexpr CompileTimeStringW<MAX_PATH> SearchString = CoreCLRUnwind;
};

#define DEFINE_STRING_SEARCHER_PARAMETERS(searchString, searchFlags) static constexpr Testing::StringSearcherParameters kSearcherParameters_##searchString##_##searchFlags = { searchString::SearchString.value, searchFlags };

#define DEFINE_STRING_SEARCH_PERFORMANCE_TEST(searchString, searchFlags, fileToSearch, chunkSize) \
    static Testing::StringSearchPerformanceTest kStringSearchPerformanceTest_##searchString##_##searchFlags##_##fileToSearch##_##chunkSize##_instance(L"StringSearchPerformanceTest_" L#searchString L"_" L#searchFlags L"_" L#fileToSearch L"_" L#chunkSize, kSearcherParameters_##searchString##_##searchFlags, fileToSearch::SearchString.value, chunkSize);

#define DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_FLAGS_AND_FILE(searchString, searchFlags, fileToSearch) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TEST(searchString, searchFlags, fileToSearch, OverlappedReaderChunkSize) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TEST(searchString, searchFlags, fileToSearch, DirectStorageReaderChunkSize)

#define DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, searchFlags) \
    DEFINE_STRING_SEARCHER_PARAMETERS(searchString, searchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_FLAGS_AND_FILE(searchString, searchFlags, LargeBinaryFile) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_FLAGS_AND_FILE(searchString, searchFlags, MediumBinaryFile) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_FLAGS_AND_FILE(searchString, searchFlags, LargeSourceFile)

#define DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING(searchString) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf8SearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf8IgnoreCaseSearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf16SearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf16IgnoreCaseSearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf8Utf16SearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf8Utf16IgnoreCaseSearchFlags)

#define DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_NO_UTF8_IGNORE_CASE(searchString) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf8SearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf16SearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf16IgnoreCaseSearchFlags) \
    DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_AND_FLAGS(searchString, Utf8Utf16SearchFlags)

DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING(ShortSearchString);
DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING(LongSearchString);
DEFINE_STRING_SEARCH_PERFORMANCE_TESTS_FOR_SEARCH_STRING_NO_UTF8_IGNORE_CASE(UnicodeSearchString);
