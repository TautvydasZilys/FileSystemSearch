#pragma once
#include "TestMacros.h"

void CreateDirectoryRecursive(const std::wstring& path);

void DeleteDirectoryRecursive(std::wstring path);

template <typename Container>
Container ReadWholeFile(const wchar_t* path)
{
    FileHandleHolder file = CreateFile2(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    CHECK(file, std::format(L"Failed to open '{}' for reading: {}", path, GetLastError()));

    LARGE_INTEGER fileSize;
    auto result = GetFileSizeEx(file, &fileSize);
    CHECK(result, std::format(L"Failed to get file size of '{}': {}", path, GetLastError()));
    CHECK(fileSize.QuadPart <= std::numeric_limits<DWORD>::max(), std::format(L"File size of '{}' is too large: {}", path, fileSize.QuadPart));

    Container fileContents;
    fileContents.resize(fileSize.QuadPart);

    DWORD bytesRead;
    result = ReadFile(file, fileContents.data(), static_cast<DWORD>(fileContents.size()), &bytesRead, nullptr);
    CHECK(result && bytesRead == fileContents.size(), std::format(L"Failed to read file '{}': {}", path, GetLastError()));

    return fileContents;
}