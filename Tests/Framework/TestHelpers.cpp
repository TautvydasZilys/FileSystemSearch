#include "PrecompiledHeader.h"
#include "TestHelpers.h"
#include "TestMacros.h"

static Testing::GlobalTestContext* s_GlobalTestContext;

static void CreateDirectoryRecursive(const std::wstring& path)
{
    auto attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        CHECK(attributes & FILE_ATTRIBUTE_DIRECTORY, std::format(L"Failed to create directory '{}' because this path already exists but is not a directory", path));
        return;        
    }

    auto lastSlash = path.find_last_of(L'\\');
    if (lastSlash != std::wstring::npos)
    {
        auto parent = path.substr(0, lastSlash);
        CreateDirectoryRecursive(parent);
    }

    auto createResult = CreateDirectoryW(path.c_str(), nullptr);
    CHECK(createResult, std::format(L"Failed to create directory '{}'", path));
}

static void DeleteDirectoryRecursive(std::wstring path)
{
    if (path.back() != L'\\')
        path.push_back(L'\\');

    auto folderPathLength = path.size();

    path.push_back('*');

    WIN32_FIND_DATAW findData;
    FindHandleHolder findHandle = FindFirstFileExW(path.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (findHandle == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (findData.cFileName[0] == L'.' && (findData.cFileName[1] == L'\0' || (findData.cFileName[1] == L'.' && findData.cFileName[2] == L'\0')))
            continue;

        path.resize(folderPathLength);
        path += findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            DeleteDirectoryRecursive(path);
        }
        else
        {
            auto deleteResult = DeleteFileW(path.c_str());
            CHECK(deleteResult, std::format(L"Failed to delete '{}'", path));
        }
    } while (FindNextFileW(findHandle, &findData) != FALSE);

    path.resize(folderPathLength - 1);

    auto removeResult = RemoveDirectoryW(path.c_str());
    CHECK(removeResult, std::format(L"Failed to remove directory '{}'", path));
}

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

Testing::TestDirectory::TestDirectory(std::wstring_view parentDir, std::wstring_view name)
{
    m_Path.reserve(parentDir.size() + name.size() + 1);

    m_Path = parentDir;

    if (m_Path.back() != L'\\')
        m_Path.push_back(L'\\');

    m_Path += name;

    CreateDirectoryRecursive(m_Path);
}

Testing::TestFile::TestFile(const TestDirectory& testDirectory, std::wstring_view fileName, std::span<const char> fileContents)
{
    m_Path.reserve(testDirectory.view().size() + fileName.size() + 1);    
    m_Path = testDirectory.view();

    if (m_Path.back() != L'\\')
        m_Path.push_back(L'\\');

    m_Path += fileName;

    FileHandleHolder file = CreateFile2(m_Path.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
    CHECK(file, std::format(L"Failed to open file '{}' for writing", m_Path));

    if (fileContents.size() > std::numeric_limits<DWORD>::max())
        __debugbreak(); // TO DO: add support for writing files over 4 GB

    DWORD bytesWritten;
    auto writeResult = WriteFile(file, fileContents.data(), static_cast<DWORD>(fileContents.size()), &bytesWritten, nullptr);
    CHECK(writeResult && bytesWritten == fileContents.size(), std::format(L"Failed to write to file '{}'", m_Path));
}

std::vector<std::wstring> Testing::SearchTest::PerformTestSearch(const wchar_t* searchPattern, const wchar_t* searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan) const
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
        searchFlags,
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
