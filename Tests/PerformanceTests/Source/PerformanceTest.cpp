#include "PrecompiledHeader.h"
#include "PerformanceTest.h"
#include "StringUtils.h"
#include "TestLogger.h"
#include "Utilities/FileUtilities.h"

Testing::PreparedPerformanceTestLayout::PreparedPerformanceTestLayout(std::wstring_view testName, std::span<const size_t> layoutSizes, std::span<const CompileTimeString<wchar_t, MAX_PATH>> testFiles, std::span<const std::wstring_view> testFileExtensions) :
    m_Layout(testName)
{
    PrintToStdout(std::format(L"Preparing layout '{}'\r\n", m_Layout->TestName()));

    std::vector<size_t> counters;
    counters.resize(layoutSizes.size(), 0);

    GenerateLayoutRecursive(m_Layout->GetTestDirectory(), layoutSizes, testFiles, testFileExtensions, counters);

    PrintToStdout(std::format(L"    Layout '{}' prepared\r\n", m_Layout->TestName()));
}

Testing::PreparedPerformanceTestLayout::~PreparedPerformanceTestLayout()
{
    if (m_Layout)
    {
        PrintToStdout(std::format(L"Cleaning up layout '{}'\r\n", m_Layout->TestName()));

        for (auto& file : m_GeneratedFiles)
        {
            const auto& path = file.GetPath();
            auto deleteResult = DeleteFileW(path.c_str());
            if (!deleteResult)
                PrintToStdout(std::format(L"Failed to delete '{}': {}", path, GetLastError()));
        }

        m_Layout->GetTestDirectory().Remove();
        PrintToStdout(std::format(L"    Layout '{}' cleaned up\r\n", m_Layout->TestName()));
    }
}

Testing::PreparedPerformanceTestLayout::PreparedPerformanceTestLayout(PreparedPerformanceTestLayout&& other) noexcept :
    m_Layout(std::move(other.m_Layout)),
    m_GeneratedFiles(std::move(other.m_GeneratedFiles))
{
    other.m_Layout = std::nullopt;
}

Testing::PreparedPerformanceTestLayout& Testing::PreparedPerformanceTestLayout::operator=(PreparedPerformanceTestLayout&& other) noexcept
{
    if (this != &other)
    {
        std::swap(m_Layout, other.m_Layout);
        std::swap(m_GeneratedFiles, other.m_GeneratedFiles);
    }

    return *this;
}

void Testing::PreparedPerformanceTestLayout::InvalidateTestFileCache() const
{
    // Format must be L"\\.\C:"
    const wchar_t volumePath[7] = { L'\\', L'\\', L'.', L'\\', m_Layout->GetTestDirectory().view().front(), L':', L'\0'};

    {
        // The kernel supposedly drops directory and metadata caches trying to prepare for an exclusive lock, even if security check fails
        // Source: https://stackoverflow.com/questions/7405868/how-can-i-invalidate-the-file-system-cache
        FileHandleHolder hVolume = CreateFileW(volumePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    }

    for (auto& file : m_GeneratedFiles)
    {
        // Opening the file with FILE_FLAG_NO_BUFFERING triggers an immediate purge of the file's cache pages
        FileHandleHolder fileHandle = CreateFileW(file.GetPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
        CHECK(fileHandle != INVALID_HANDLE_VALUE, std::format(L"Failed to open '{}' for cache invalidation: {}", file.GetPath(), GetLastError()));
    }
}

static constexpr const wchar_t kFileNameCharacters[] = L"0123abcdABCD";
static constexpr size_t kPrimes[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };

static constexpr wchar_t GetFileNameCharacter(size_t index)
{
    return kFileNameCharacters[index % (std::size(kFileNameCharacters) - 1)];
}

static constexpr std::array<wchar_t, 12> MakeBaseFileName(size_t index)
{
    std::array<wchar_t, 12> result;
    static_assert(std::size(kPrimes) == std::size(result), "Sizes must match!");

    index += 201911;
    for (size_t i = 0; i < result.size(); i++)
    {
        index = (index * kPrimes[i]) % 1004167;
        result[i] = GetFileNameCharacter(index);
    }

    return result;
}

void Testing::PreparedPerformanceTestLayout::GenerateLayoutRecursive(const TestDirectory& testDirectory, std::span<const size_t> layoutSizes, std::span<const CompileTimeString<wchar_t, MAX_PATH>> testFiles, std::span<const std::wstring_view> testFileExtensions, std::span<size_t> counters, size_t depth)
{
    const size_t fileDepth = layoutSizes.size() - 1;
    for (size_t i = 0; i < layoutSizes[fileDepth]; i++)
    {
        auto fileIndex = counters[fileDepth]++;
        const auto baseFileName = MakeBaseFileName(293 * fileIndex);
        const auto fileExtension = testFileExtensions[fileIndex % testFileExtensions.size()];

        std::wstring fileName(&baseFileName[0], baseFileName.size());
        fileName += fileExtension;

        if (testFiles.size() > 0)
        {
            m_GeneratedFiles.emplace_back(testDirectory, fileName, testFiles[fileIndex % testFiles.size()].value);
        }
        else
        {
            m_GeneratedFiles.emplace_back(testDirectory, fileName, std::span<const char>("x", 1));
        }
    }

    if (depth != layoutSizes.size() - 1)
    {
        for (size_t i = 0; i < layoutSizes[depth]; i++)
        {
            auto folderIndex = counters[depth]++;
            auto folderName = MakeBaseFileName(593 * folderIndex);
            auto subDir = testDirectory.SubDirectory(std::wstring_view(&folderName[0], folderName.size()));
            GenerateLayoutRecursive(subDir, layoutSizes, testFiles, testFileExtensions, counters, depth + 1);
        }
    }
}

Testing::PreparedPerformanceTestLayout Testing::PerformanceTestDataLayout::Prepare() const
{
    return PreparedPerformanceTestLayout(m_LayoutName, m_LayoutSizes, m_TestFiles, m_TestFileExtensions);
}

bool Testing::PerformanceTestDataLayout::operator<(const PerformanceTestDataLayout& other) const
{
    if (m_LayoutSizes.size() != other.m_LayoutSizes.size())
        return m_LayoutSizes.size() < other.m_LayoutSizes.size();
        
    for (size_t i = 0; i < m_LayoutSizes.size(); i++)
    {
        if (m_LayoutSizes[i] != other.m_LayoutSizes[i])
            return m_LayoutSizes[i] < other.m_LayoutSizes[i];
    }
        
    if (m_TestFiles.size() != other.m_TestFiles.size())
        return m_TestFiles.size() < other.m_TestFiles.size();
        
    for (size_t i = 0; i < m_TestFiles.size(); i++)
    {
        if (m_TestFiles[i] != other.m_TestFiles[i])
            return m_TestFiles[i] < other.m_TestFiles[i];
    }
        
    if (m_TestFileExtensions.size() != other.m_TestFileExtensions.size())
        return m_TestFileExtensions.size() < other.m_TestFileExtensions.size();
        
    for (size_t i = 0; i < m_TestFileExtensions.size(); i++)
    {
        if (m_TestFileExtensions[i] != other.m_TestFileExtensions[i])
            return m_TestFileExtensions[i] < other.m_TestFileExtensions[i];
    }

    return false;
}

bool Testing::PerformanceTestDataLayout::operator==(const PerformanceTestDataLayout& other) const
{
    if (m_LayoutSizes.size() != other.m_LayoutSizes.size())
        return false;

    for (size_t i = 0; i < m_LayoutSizes.size(); i++)
    {
        if (m_LayoutSizes[i] != other.m_LayoutSizes[i])
            return false;
    }

    if (m_TestFiles.size() != other.m_TestFiles.size())
        return false;

    for (size_t i = 0; i < m_TestFiles.size(); i++)
    {
        if (m_TestFiles[i] != other.m_TestFiles[i])
            return false;
    }

    if (m_TestFileExtensions.size() != other.m_TestFileExtensions.size())
        return false;

    for (size_t i = 0; i < m_TestFileExtensions.size(); i++)
    {
        if (m_TestFileExtensions[i] != other.m_TestFileExtensions[i])
            return false;
    }

    return true;
}

constexpr CompileTimeString kDirectoryOfTestResultsForComparison = Testing::GetPerformanceTestDataDirectory() + L"\\TestResultsForComparison";

std::wstring Testing::PerformanceTestBase::GetPerformanceTestResultForComparisonPath() const
{
    std::wstring path;
    path.reserve(kDirectoryOfTestResultsForComparison.Length + TestName().size() + 5);

    path = kDirectoryOfTestResultsForComparison.value;
    path.push_back(L'\\');
    path += TestName();
    path += L".txt";

    return path;
}

std::vector<std::wstring> Testing::PerformanceTestBase::LoadPerformanceTestResultForComparison(std::wstring_view testDirectory) const
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

void Testing::PerformanceTestBase::SavePerformanceTestResultForComparison(std::wstring_view testDirectory, const std::vector<std::wstring>& foundPaths) const
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

int Testing::RunAllPerformanceTests()
{
    try
    {
        GlobalTestContext globalTestContext;

        std::vector<PerformanceTestBase*> allTests;

        {
            auto test = PerformanceTestBase::GetFirstTest();
            while (test != nullptr)
            {
                allTests.push_back(test);
                test = test->GetNextTest();
            }
        }

        // First check if there are any layouts that are identical but have different names
        std::sort(allTests.begin(), allTests.end(), [](const PerformanceTestBase* a, const PerformanceTestBase* b)
        {
            return *a->GetPerformanceTestDataLayout() < *b->GetPerformanceTestDataLayout();
        });

        for (size_t i = 1; i < allTests.size(); i++)
        {
            auto& a = *allTests[i - 1]->GetPerformanceTestDataLayout();
            auto& b = *allTests[i]->GetPerformanceTestDataLayout();
            if (a == b)
                CHECK(a.GetLayoutName() == b.GetLayoutName(), std::format(L"Two performance test layouts are identical but have different names: '{}' and '{}'", a.GetLayoutName(), b.GetLayoutName()));
        }

        // Now check if there are any layouts that have same names but are actually different
        std::sort(allTests.begin(), allTests.end(), [](const PerformanceTestBase* a, const PerformanceTestBase* b)
        {
            auto& aLayout = *a->GetPerformanceTestDataLayout();
            auto& bLayout = *b->GetPerformanceTestDataLayout();

            if (aLayout.GetLayoutName() == bLayout.GetLayoutName())
                return a->TestName() < b->TestName();

            return aLayout.GetLayoutName() < bLayout.GetLayoutName();
        });

        for (size_t i = 1; i < allTests.size(); i++)
        {
            auto a = allTests[i - 1]->GetPerformanceTestDataLayout();
            auto b = allTests[i]->GetPerformanceTestDataLayout();
            if (a->GetLayoutName() == b->GetLayoutName())
                CHECK(a == b, std::format(L"Two performance test layouts have the same name '{}' but are different", a->GetLayoutName()));
        }

        // Now, run the tests
        const PerformanceTestDataLayout* currentLayout = nullptr;
        std::optional<PreparedPerformanceTestLayout> preparedLayout;

        int failureCount = 0;
        int successCount = 0;
        int testCount = 0;
        int layoutCount = 0;

        for (const auto& test : allTests)
        {
            //if (test->TestName().find(L"FileContents_BinaryFiles_LongSearchString_UTF16") == std::wstring_view::npos)
            //    continue;

            if (test->GetPerformanceTestDataLayout() != currentLayout)
            {
                auto newLayout = test->GetPerformanceTestDataLayout();
                preparedLayout = std::nullopt; // Clear the previous layout before prepared new one to use less disk space
                preparedLayout = newLayout->Prepare();
                currentLayout = newLayout;
                layoutCount++;
            }

            CHECK(preparedLayout, L"No layout has been prepared!");

            try
            {
                PrintToStdout(std::format(L"Running '{}'\r\n", test->TestName()));
                testCount++;
                test->Run(*preparedLayout);
                successCount++;
                PrintToStdout(L"    PASS!\r\n");
            }
            catch (const TestFailedException& e)
            {
                failureCount++;
                PrintToStdout(std::format(L"    FAILED:\r\n        {}\r\n", e.Text()));
            }
        }

        preparedLayout = std::nullopt;

        PrintToStdout(std::format(L"Ran {} tests across {} layouts: {} succeeded, {} failed.\r\n", testCount, layoutCount, successCount, failureCount));
        return failureCount;
    }
    catch (const TestFailedException& e)
    {
        PrintToStdout(std::format(L"Tests encountered a fatal error outside of any single test: {}\r\n", e.Text()));
        return std::numeric_limits<int>::max();
    }
}

void Testing::ReportPerformanceTestResults()
{
    PrintToStdout(std::format(L"\r\nPerformance Test Results:\r\n"));

    std::vector<PerformanceTestBase*> allTests;

    {
        auto test = PerformanceTestBase::GetFirstTest();
        while (test != nullptr)
        {
            allTests.push_back(test);
            test = test->GetNextTest();
        }
    }

    if (allTests.empty())
    {
        PrintToStdout(std::format(L"    No performance tests were run.\r\n"));
        return;
    }

    std::sort(allTests.begin(), allTests.end(), [](const PerformanceTestBase* a, const PerformanceTestBase* b)
    {
        return a->TestName() < b->TestName();
    });

    for (auto test : allTests)
    {
        auto name = test->TestName();
        if (isnan(test->GetMedianRunTime()))
            continue;

        auto durationMS = 1000.0 * test->GetMedianRunTime();

        PrintToStdout(std::format(L"    {:100}: {:.2f} ms\r\n", name, durationMS));
    }
}
