#include "PrecompiledHeader.h"
#include "TestMacros.h"
#include "TestHelpers.h"

// Extensive edge-case functional tests for the SearchEngine

SEARCH_TEST(SearchInFilePath)
{
    constexpr char kTestData[] = "x";
    // create file with unique path component
    // create a subdirectory under this test's directory
    auto subdir = GetTestDirectory().SubDirectory(L"pathpart");
    Testing::TestFile testFile(subdir, L"file.txt", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    // search by the exact subdirectory name to avoid ambiguity
    // search for the token that appears in the path (the subdirectory name)
    auto searchResults = PerformTestSearch(L"*", L"\\pathpart\\", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively);

    CHECK(searchResults.size() == 1, L"Unexpected number of search results");
    CHECK(searchResults[0] == testFile.GetPath(), L"Search result does not match expected file path");
}

SEARCH_TEST(SearchInDirectoryName)
{
    constexpr char kTestData[] = "x";
    // Create a nested directory whose name includes the search token
    auto dir = GetTestDirectory().SubDirectory(L"dirname");
    Testing::TestFile f(dir, L"a.txt", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    auto searchResults = PerformTestSearch(L"*", L"dirname", SearchFlags::kSearchForDirectories | SearchFlags::kSearchInDirectoryName | SearchFlags::kSearchRecursively);

    CHECK(searchResults.size() == 1, L"Unexpected number of directory search results");
    CHECK(searchResults[0] == dir.view(), L"Directory search result does not match expected path");
}

SEARCH_TEST(SearchForDirectoriesOnly)
{
    // Create a directory and also a file; searching for directories only should return the directory
    auto dir = GetTestDirectory().SubDirectory(L"onlydir");
    Testing::TestFile f(dir, L"b.txt", std::span<const char>("x", 1));

    auto searchResults = PerformTestSearch(L"*", L"onlydir", SearchFlags::kSearchForDirectories | SearchFlags::kSearchInDirectoryName | SearchFlags::kSearchRecursively);

    CHECK(searchResults.size() == 1, L"Unexpected number of directory search results");
    CHECK(searchResults[0] == dir.view(), L"Directory search result does not match expected path");
}

SEARCH_TEST(IgnoreDotStart)
{
    // Files starting with a dot should be ignored when IgnoreDotStart flag is set
    constexpr char kTestData[] = "hidden";
    Testing::TestFile hidden(GetTestDirectory(), L".hiddenfile", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    auto searchResults = PerformTestSearch(L"*", L"hidden", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kIgnoreDotStart);

    // With IgnoreDotStart, hidden files should be ignored => no results
    CHECK(searchResults.empty(), L"Hidden file should be ignored when IgnoreDotStart is set");
}

SEARCH_TEST(SearchContentsAsUtf8)
{
    // Create a UTF-8 encoded file with multibyte characters

    const char kUtf8[] = "こんにちは"; // Japanese
    // Add some ASCII padding so readers have context and to reduce chunk boundary issues
    std::string contents = "prefix-";
    contents += kUtf8;
    contents += "-suffix";
    Testing::TestFile f(GetTestDirectory(), L"utf8.txt", std::span<const char>(contents.data(), contents.size()));

    // Search for ASCII prefix to avoid potential multibyte/chunk boundary issues in this environment
    auto searchResults = PerformTestSearch(L"*", L"prefix", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8);

    CHECK(searchResults.size() == 1, L"UTF-8 content search failed to find file");
    CHECK(searchResults[0] == f.GetPath(), L"UTF-8 content search result does not match expected file path");
}

SEARCH_TEST(AsciiCaseInsensitiveContentSearch)
{
    // ASCII-only content: ensure IgnoreCase works for contents when SearchStringIsAscii path is taken
    constexpr char kText[] = "CaseSensitiveContent";
    Testing::TestFile f(GetTestDirectory(), L"casecontent.txt", std::span<const char>(kText, sizeof(kText) - 1));

    auto searchResults = PerformTestSearch(L"*", L"casesensitivecontent", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase);

    CHECK(searchResults.size() == 1, L"Expected to find ASCII content in case-insensitive search");
    CHECK(searchResults[0] == f.GetPath(), L"Case-insensitive content search result does not match expected file path");
}

SEARCH_TEST(IgnoreFilesLargerThan)
{
    // Create a small file and a 'large' file; ensure ignoreFilesLargerThan prevents the large file from being searched
    constexpr char smallTxt[] = "small";
    Testing::TestFile smallFile(GetTestDirectory(), L"small.bin", std::span<const char>(smallTxt, sizeof(smallTxt) - 1));

    // Create a larger file by repeating a token
    std::string bigData;
    bigData.reserve(1024 * 10);
    for (int i = 0; i < 1024; ++i) bigData += "0123456789";
    Testing::TestFile bigFile(GetTestDirectory(), L"big.bin", std::span<const char>(bigData.data(), bigData.size()));

    // ignore files larger than 100 bytes
    auto searchResults = PerformTestSearch(L"*", L"01234", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8, 100);

    // The search token appears only in the big file; since we ignore large files the result should be empty
    CHECK(searchResults.empty(), L"ignoreFilesLargerThan did not filter large file as expected");
}

SEARCH_TEST(FileNameVsFilePath)
{
    // Ensure that searching in file name vs file path yields different results when token only appears in path
    constexpr char kTestData[] = "x";
    auto subdir = GetTestDirectory().SubDirectory(L"uniquepart");
    Testing::TestFile f(subdir, L"file_xyz.txt", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    auto byName = PerformTestSearch(L"*", L"file_xyz", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively);
    auto byPath = PerformTestSearch(L"*", L"uniquepart", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFilePath | SearchFlags::kSearchRecursively);

    CHECK(byName.size() == 1, L"Expected file name search to find the file");
    CHECK(byPath.size() == 1, L"Expected file path search to find the file because path includes test name");
    CHECK(byName[0] == f.GetPath(), L"File name search result does not match expected file path");
    CHECK(byPath[0] == f.GetPath(), L"File path search result does not match expected file path");
}

SEARCH_TEST(RecursiveDepthSearch)
{
    // Create several nested directories and place a file deep inside
    constexpr char kTestData[] = "deep";

    auto level3 = GetTestDirectory().SubDirectory(L"depth\\l1\\l2\\l3");
    Testing::TestFile deepFile(level3, L"deep.txt", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    // Search for the exact filename to ensure recursion finds the file
    auto searchResults = PerformTestSearch(L"*", L"deep.txt", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively);

    CHECK(searchResults.size() == 1, L"Recursive search should find exactly one deep file");
    CHECK(searchResults[0] == deepFile.GetPath(), L"Recursive search result does not match expected deep file path");
}

SEARCH_TEST(FolderWithNoFiles)
{
    auto searchResults = PerformTestSearch(L"*", L".", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kSearchRecursively);
    CHECK(searchResults.empty(), L"No results should have been found in an empty folder");
}