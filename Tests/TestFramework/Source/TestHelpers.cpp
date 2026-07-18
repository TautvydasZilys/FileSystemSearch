#include "PrecompiledHeader.h"
#include "TestHelpers.h"
#include "TestLogger.h"
#include "TestMacros.h"
#include "Utilities/FileUtilities.h"

static Testing::GlobalTestContext* s_GlobalTestContext;

Testing::GlobalTestContext::GlobalTestContext()
{
    CHECK(s_GlobalTestContext == nullptr, L"Global test context already exists");
    s_GlobalTestContext = this;

    std::wstring tempPath;
    tempPath.resize(MAX_PATH);

    auto tempPathLength = GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data());
    CHECK(tempPathLength != 0, L"Failed to get TEMP path");

    if (tempPathLength >= tempPath.size())
    {
        tempPath.resize(tempPathLength + 1);
        tempPathLength = GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data());
    }

    tempPath.resize(tempPathLength);

    m_GlobalTestDirectory = std::move(tempPath);
    m_GlobalTestDirectory += L"FileSystemSearchTests";

    if (GetFileAttributesW(m_GlobalTestDirectory.c_str()) != INVALID_FILE_ATTRIBUTES)
        DeleteDirectoryRecursive(m_GlobalTestDirectory);

    auto createResult = CreateDirectoryW(m_GlobalTestDirectory.c_str(), nullptr);
    CHECK(createResult, std::format(L"Failed to create directory '{}'", m_GlobalTestDirectory));
}

Testing::GlobalTestContext::~GlobalTestContext()
{
    DeleteDirectoryRecursive(m_GlobalTestDirectory);

    if (s_GlobalTestContext != this)
        __debugbreak();

    s_GlobalTestContext = nullptr;
}

Testing::TestDirectory::TestDirectory(std::wstring_view testName) :
    TestDirectory(s_GlobalTestContext->GetGlobalTestDirectory(), testName)
{
}

void Testing::TestDirectory::Remove() const
{
    DeleteDirectoryRecursive(m_Path);
}

Testing::TestDirectory::TestDirectory(std::wstring_view parentDir, std::wstring_view name)
{
    m_Path.reserve(parentDir.size() + name.size() + 1);

    m_Path = parentDir;

    if (m_Path.back() != L'\\')
        m_Path.push_back(L'\\');

    m_Path += name;

    CreateDirectoryRecursive(m_Path);
}

Testing::TestFile::TestFile(const TestDirectory& testDirectory, std::wstring_view fileName)
{
    m_Path.reserve(testDirectory.view().size() + fileName.size() + 1);
    m_Path = testDirectory.view();

    if (m_Path.back() != L'\\')
        m_Path.push_back(L'\\');

    m_Path += fileName;
}

Testing::TestFile::TestFile(const TestDirectory& testDirectory, std::wstring_view fileName, std::span<const char> fileContents) :
    TestFile(testDirectory, fileName)
{
    WriteWholeFile(m_Path.c_str(), fileContents);
}

Testing::TestFile::TestFile(const TestDirectory& testDirectory, std::wstring_view fileName, const wchar_t* sourceFilePath) :
    TestFile(testDirectory, fileName)
{
    auto result = CopyFileW(sourceFilePath, m_Path.c_str(), FALSE);
    CHECK(result, std::format(L"Failed to copy file from '{}' to '{}': {}", sourceFilePath, m_Path, GetLastError()));
}

template <typename BaseClass>
std::vector<std::wstring> Testing::SearchTestImpl<BaseClass>::PerformTestSearch(const wchar_t* searchPattern, const wchar_t* searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan) const
{
    struct TestContext
    {
        Event<EventType::ManualReset> doneEvent;
        std::vector<std::wstring> foundPaths;
        std::vector<std::wstring> errors;
    } testContext;

    auto foundPathCallback = [](void* context, const WIN32_FIND_DATAW&, const wchar_t* path)
    {
        static_cast<TestContext*>(context)->foundPaths.emplace_back(path);
    };

    auto searchDoneCallback = [](void* context, const SearchStatistics&)
    {
        static_cast<TestContext*>(context)->doneEvent.Set();
    };

    auto errorCallback = [](void* context, const wchar_t* errorMessage)
    {
        static_cast<TestContext*>(context)->errors.emplace_back(errorMessage);
    };

    auto searcher = ::Search(
        foundPathCallback,
        [](void*, const SearchStatistics&, double) {},
        searchDoneCallback,
        errorCallback,
        GetTestDirectory().c_str(),
        searchPattern,
        searchString,
        searchFlags | m_ExtraSearchFlags,
        ignoreFilesLargerThan,
        &testContext);

    auto waitResult = WaitForSingleObject(testContext.doneEvent, INFINITE);
    CHECK(waitResult == WAIT_OBJECT_0, L"Failed to wait for search operation to complete");

    CleanupSearchOperation(searcher);

    CHECK(testContext.errors.empty(), [](const std::vector<std::wstring>& errors)
    {
        std::wstring combinedErrors = L"Search operation encountered errors:";
        for (const auto& error : errors)
        {
            combinedErrors += L"\r\n";
            combinedErrors += error;
        }

        return combinedErrors;
    }(testContext.errors));

    return std::move(testContext.foundPaths);
}

template Testing::SearchTestImpl<Testing::ITest>;
template Testing::SearchTestImpl<Testing::FunctionalTest>;