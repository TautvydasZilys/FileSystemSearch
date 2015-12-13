#pragma once

#include "PathUtils.h"

enum class FileSystemEnumerationFlags
{
	kEnumerateNone = 0,
	kEnumerateFiles = 1 << 0,
	kEnumerateDirectories = 1 << 1
};

MAKE_BIT_OPERATORS_FOR_ENUM_CLASS(FileSystemEnumerationFlags)

template <typename Allocator, typename EnumerationCallback>
inline void EnumerateFileSystem(const std::wstring& searchPath, const wchar_t* searchPattern, size_t searchPatternLength, FileSystemEnumerationFlags enumerationFlags, Allocator& allocator, EnumerationCallback onFileEnumerated)
{
	WIN32_FIND_DATAW findData;
	auto tempSearchPath = PathUtils::CombinePathsTemporary(searchPath, searchPattern, searchPatternLength, allocator);
	FINDEX_SEARCH_OPS searchOp = (enumerationFlags & FileSystemEnumerationFlags::kEnumerateFiles) == FileSystemEnumerationFlags::kEnumerateNone ? FindExSearchLimitToDirectories : FindExSearchNameMatch;

	auto findHandle = FindFirstFileExW(tempSearchPath, FindExInfoBasic, &findData, searchOp, nullptr, FIND_FIRST_EX_LARGE_FETCH);

	if (findHandle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (findData.cFileName[0] == L'.' && (findData.cFileName[1] == L'\0' || (findData.cFileName[1] == L'.' && findData.cFileName[2] == L'\0')))
			continue;
		
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (enumerationFlags & FileSystemEnumerationFlags::kEnumerateDirectories) == FileSystemEnumerationFlags::kEnumerateNone)
			continue;

		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && (enumerationFlags & FileSystemEnumerationFlags::kEnumerateFiles) == FileSystemEnumerationFlags::kEnumerateNone)
			continue;
		
		onFileEnumerated(findData);
	}
	while (FindNextFileW(findHandle, &findData) != FALSE);

	FindClose(findHandle);
}