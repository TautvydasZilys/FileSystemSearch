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
    std::string text;

    const auto testResultPath = GetPerformanceTestResultForComparisonPath();
    FileHandleHolder file = CreateFile2(testResultPath.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    CHECK(file, std::format(L"Failed to open '{}' for reading: {}", testResultPath, GetLastError()));

    LARGE_INTEGER fileSize;
    auto result = GetFileSizeEx(file, &fileSize);
    CHECK(result, std::format(L"Failed to get file size of '{}': {}", testResultPath, GetLastError()));
    CHECK(fileSize.QuadPart <= std::numeric_limits<DWORD>::max(), std::format(L"File size of '{}' is too large: {}", testResultPath, fileSize.QuadPart));

    text.resize(fileSize.QuadPart);

    DWORD bytesRead;
    result = ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &bytesRead, nullptr);
    CHECK(result && bytesRead == text.size(), std::format(L"Failed to read file '{}': {}", testResultPath, GetLastError()));

    std::string_view textView = text;

    size_t pathCount;
    auto [ptr, err] = std::from_chars(textView.data(), textView.data() + textView.size(), pathCount);
    CHECK(err == std::errc(), std::format(L"Failed to parse path count from '{}'", testResultPath));

    textView.remove_prefix(ptr - textView.data());

    CHECK(textView.size() >= 2 && textView.front() == '\r' && textView[1] == '\n', std::format(L"Expected CRLF after path count in '{}'", testResultPath));
    textView.remove_prefix(2);

    std::vector<std::wstring> foundPaths;
    foundPaths.resize(pathCount);

    for (size_t i = 0; i < pathCount; i++)
    {
        auto lineEnd = textView.find("\r\n");
        CHECK(lineEnd != std::string_view::npos, std::format(L"Failed to find end of line for path {} in '{}'", i, testResultPath));

        std::string_view pathUtf8(textView.data(), lineEnd);

        foundPaths[i].reserve(testDirectory.length() + 1 + pathUtf8.length());
        foundPaths[i] = testDirectory;
        foundPaths[i] += L"\\";
        foundPaths[i] += StringUtils::Utf8ToUtf16(pathUtf8);

        textView.remove_prefix(lineEnd + 2);
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
    FileHandleHolder outputFile = CreateFile2(outputPath.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
    CHECK(outputFile != INVALID_HANDLE_VALUE, std::format(L"Failed to create file '{}': {}", outputPath, GetLastError()));

    CHECK(outputBuffer.size() < std::numeric_limits<DWORD>::max(), std::format(L"Output buffer is too large to write to file '{}'", outputPath));

    DWORD bytesWritten;
    auto writeResult = WriteFile(outputFile, outputBuffer.data(), static_cast<DWORD>(outputBuffer.size()), &bytesWritten, nullptr);
    CHECK(writeResult && bytesWritten == outputBuffer.size(), std::format(L"Failed to write to file '{}': {}", outputPath, GetLastError()));
}