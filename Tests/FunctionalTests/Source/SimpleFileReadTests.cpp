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