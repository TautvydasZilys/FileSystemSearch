#include "PrecompiledHeader.h"
#include "PerformanceTest.h"
#include "PerformanceIntegrationTest.h"
#include "StringUtils.h"
#include "Utilities/FileUtilities.h"

constexpr CompileTimeString kDirectoryOfTestResultsForComparison = Testing::GetPerformanceTestDataDirectory() + L"\\TestResultsForComparison";

std::wstring Testing::PerformanceIntegrationTestBase::GetPerformanceTestResultForComparisonPath() const
{
    std::wstring path;
    path.reserve(kDirectoryOfTestResultsForComparison.Length + TestName().size() + 5);

    path = kDirectoryOfTestResultsForComparison.value;
    path.push_back(L'\\');
    path += TestName();
    path += L".txt";

    return path;
}

std::vector<std::wstring> Testing::PerformanceIntegrationTestBase::LoadPerformanceTestResultForComparison(std::wstring_view testDirectory) const
{
    const auto testResultPath = GetPerformanceTestResultForComparisonPath();
    std::string text = ReadWholeFile<std::string>(testResultPath.c_str());

    std::string_view textView = text;

    size_t pathCount;
    auto [ptr, err] = std::from_chars(textView.data(), textView.data() + textView.size(), pathCount);
    CHECK(err == std::errc(), std::format(L"Failed to parse path count from '{}'", testResultPath));

    textView.remove_prefix(ptr - textView.data());

    CHECK(textView.size() >= 1, std::format(L"Expected line break after path count in '{}'", testResultPath));
    if (textView.front() == '\r')
    {
        textView.remove_prefix(1);
        CHECK(textView.size() >= 1 && textView.front() == '\n', std::format(L"Expected LF after CR in '{}'", testResultPath));
        textView.remove_prefix(1);
    }
    else
    {
        CHECK(textView.front() == '\n', std::format(L"Expected line break after path count in '{}'", testResultPath));
        textView.remove_prefix(1);
    }

    std::vector<std::wstring> foundPaths;
    foundPaths.resize(pathCount);

    for (size_t i = 0; i < pathCount; i++)
    {
        auto lineEnd = textView.find('\n');
        CHECK(lineEnd != std::string_view::npos, std::format(L"Failed to find end of line for path {} in '{}'", i, testResultPath));

        std::string_view pathUtf8(textView.data(), lineEnd);
        if (!pathUtf8.empty() && pathUtf8.back() == '\r')
            pathUtf8.remove_suffix(1);

        foundPaths[i].reserve(testDirectory.length() + 1 + pathUtf8.length());
        foundPaths[i] = testDirectory;
        foundPaths[i] += L"\\";
        foundPaths[i] += StringUtils::Utf8ToUtf16(pathUtf8);

        textView.remove_prefix(lineEnd + 1);
    }

    return foundPaths;
}

void Testing::PerformanceIntegrationTestBase::SavePerformanceTestResultForComparison(std::wstring_view testDirectory, const std::vector<std::wstring>& foundPaths) const
{
    CreateDirectoryRecursive(kDirectoryOfTestResultsForComparison.value);

    std::string outputBuffer;

    char pathCountBuffer[24];
    sprintf_s(pathCountBuffer, "%zu", foundPaths.size());

    outputBuffer += pathCountBuffer;
    outputBuffer += "\r\n";

    for (std::wstring_view path : foundPaths)
    {
        CHECK(path.starts_with(testDirectory), std::format(L"Found path '{}' does not start with test directory '{}'", path, testDirectory));

        outputBuffer += StringUtils::Utf16ToUtf8(path.substr(testDirectory.size() + 1));
        outputBuffer += "\r\n";
    }

    const auto outputPath = GetPerformanceTestResultForComparisonPath();
    WriteWholeFile(outputPath.c_str(), outputBuffer);
}