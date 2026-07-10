#include "PrecompiledHeader.h"
#include "TestMacros.h"
#include "TestHelpers.h"

SEARCH_TEST(SimpleFileRead)
{
    constexpr char kTestData[] = "Hello, world!";
    Testing::TestFile testFile(GetTestDirectory(), L"test.txt", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    auto searchResults = Testing::PerformTestSearch(GetTestDirectory(), L"*", L"llo, w", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf8);

    CHECK(searchResults.errors.empty(), L"Search operation encountered errors");
    CHECK(searchResults.foundPaths.size() == 1, L"Unexpected number of search results");
    CHECK(searchResults.foundPaths[0] == testFile.GetPath(), L"Search result does not match expected file path");
}

SEARCH_TEST(SearchInFileName)
{
    constexpr char kTestData[] = "x";
    Testing::TestFile testFile(GetTestDirectory(), L"hello_search.txt", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    auto searchResults = Testing::PerformTestSearch(GetTestDirectory(), L"*", L"hello_search", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName);

    CHECK(searchResults.errors.empty(), L"Search operation encountered errors");
    CHECK(searchResults.foundPaths.size() == 1, L"Unexpected number of search results");
    CHECK(searchResults.foundPaths[0] == testFile.GetPath(), L"Search result does not match expected file path");
}

SEARCH_TEST(RecursiveFileSearch)
{
    constexpr char kTestData[] = "nested";
    auto nestedDir = GetTestDirectory().SubDirectory(L"subdir");

    Testing::TestFile nestedFile(nestedDir, L"nested.txt", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    auto searchResults = Testing::PerformTestSearch(GetTestDirectory(), L"*", L"nested", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively);

    CHECK(searchResults.errors.empty(), L"Search operation encountered errors");
    CHECK(searchResults.foundPaths.size() == 1, L"Unexpected number of search results");
    CHECK(searchResults.foundPaths[0] == nestedFile.GetPath(), L"Search result does not match expected file path");
}

SEARCH_TEST(Utf16ContentSearch)
{
    // Prepare UTF-16LE encoded content (wchar_t on Windows is UTF-16)
    constexpr wchar_t kUtf16Text[] = L"ąčęėįšų";
    const char* data = reinterpret_cast<const char*>(kUtf16Text);
    size_t byteCount = (wcslen(kUtf16Text)) * sizeof(wchar_t);

    Testing::TestFile testFile(GetTestDirectory(), L"utf16.txt", std::span<const char>(data, byteCount));

    auto searchResults = Testing::PerformTestSearch(GetTestDirectory(), L"*", L"čęėįš", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileContents | SearchFlags::kSearchContentsAsUtf16);

    CHECK(searchResults.errors.empty(), L"Search operation encountered errors");
    CHECK(searchResults.foundPaths.size() == 1, L"Unexpected number of search results");
    CHECK(searchResults.foundPaths[0] == testFile.GetPath(), L"Search result does not match expected file path");
}

SEARCH_TEST(CaseInsensitiveFileNameSearch)
{
    constexpr char kTestData[] = "x";
    Testing::TestFile testFile(GetTestDirectory(), L"CaseTest.TXT", std::span<const char>(kTestData, sizeof(kTestData) - 1));

    auto searchResults = Testing::PerformTestSearch(GetTestDirectory(), L"*", L"casetest", SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kIgnoreCase);

    CHECK(searchResults.errors.empty(), L"Search operation encountered errors");
    CHECK(searchResults.foundPaths.size() == 1, L"Unexpected number of search results");
    CHECK(searchResults.foundPaths[0] == testFile.GetPath(), L"Search result does not match expected file path");
}
