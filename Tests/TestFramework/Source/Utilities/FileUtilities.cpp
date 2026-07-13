#include "PrecompiledHeader.h"
#include "FileUtilities.h"
#include "TestLogger.h"
#include "TestMacros.h"

void CreateDirectoryRecursive(const std::wstring& path)
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

void DeleteDirectoryRecursive(std::wstring path)
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
            if (!deleteResult)
                Testing::PrintToStdout(std::format(L"Failed to delete '{}': {}", path, GetLastError()));
        }
    } while (FindNextFileW(findHandle, &findData) != FALSE);

    path.resize(folderPathLength - 1);

    auto removeResult = RemoveDirectoryW(path.c_str());
    if (!removeResult)
        Testing::PrintToStdout(std::format(L"Failed to remove directory '{}': {}", path, GetLastError()));
}
