#pragma once
#include "TestMacros.h"

void CreateDirectoryRecursive(const std::wstring& path);

void DeleteDirectoryRecursive(std::wstring path);

template <typename Container>
Container ReadFileToEnd(HANDLE file, const wchar_t* pathForErrorMessages)
{
    static_assert(sizeof(Container::value_type) == 1, "Container must be a container of bytes");

    LARGE_INTEGER fileSize;
    auto result = GetFileSizeEx(file, &fileSize);
    CHECK(result, std::format(L"Failed to get file size of '{}': {}", pathForErrorMessages, GetLastError()));
    CHECK(fileSize.QuadPart <= std::numeric_limits<DWORD>::max(), std::format(L"File size of '{}' is too large: {}", pathForErrorMessages, fileSize.QuadPart));

    Container fileContents;
    fileContents.resize(fileSize.QuadPart);

    DWORD bytesRead;
    result = ReadFile(file, fileContents.data(), static_cast<DWORD>(fileContents.size()), &bytesRead, nullptr);
    CHECK(result && bytesRead == fileContents.size(), std::format(L"Failed to read file '{}': {}", pathForErrorMessages, GetLastError()));

    return fileContents;
}

template <typename Container>
Container ReadWholeFile(const wchar_t* path)
{
    FileHandleHolder file = CreateFile2(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    CHECK(file, std::format(L"Failed to open '{}' for reading: {}", path, GetLastError()));
    return ReadFileToEnd<Container>(file, path);
}

template <typename Container>
void WriteWholeFile(HANDLE file, const wchar_t* pathForErrorMessages, const Container& contents)
{
    static_assert(sizeof(Container::value_type) == 1, "Container must be a container of bytes");
    CHECK(contents.size() < std::numeric_limits<DWORD>::max(), std::format(L"Output buffer is too large to write to file '{}'", pathForErrorMessages));

    DWORD bytesWritten;
    auto writeResult = WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &bytesWritten, nullptr);
    CHECK(writeResult && bytesWritten == contents.size(), std::format(L"Failed to write to file '{}': {}", pathForErrorMessages, GetLastError()));
}

template <typename Container>
void WriteWholeFile(const wchar_t* path, const Container& contents)
{
    FileHandleHolder file = CreateFile2(path, GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
    CHECK(file != INVALID_HANDLE_VALUE, std::format(L"Failed to open file '{}' for writing: {}", path, GetLastError()));
    return WriteWholeFile(file, path, contents);
}