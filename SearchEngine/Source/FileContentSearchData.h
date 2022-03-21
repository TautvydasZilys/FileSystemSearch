#pragma once

#include "NonCopyable.h"

struct FileFindData
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;

	inline FileFindData()
	{
	}

	inline FileFindData(const WIN32_FIND_DATAW& fileFindData) :
		dwFileAttributes(fileFindData.dwFileAttributes),
		ftCreationTime(fileFindData.ftCreationTime),
		ftLastAccessTime(fileFindData.ftLastAccessTime),
		ftLastWriteTime(fileFindData.ftLastWriteTime),
		nFileSizeHigh(fileFindData.nFileSizeHigh),
		nFileSizeLow(fileFindData.nFileSizeLow),
		dwReserved0(fileFindData.dwReserved0),
		dwReserved1(fileFindData.dwReserved1)
	{
	}

	inline WIN32_FIND_DATAW ToWin32FindData(std::wstring_view fileName) const
	{
		WIN32_FIND_DATAW result;

		result.dwFileAttributes = dwFileAttributes;
		result.ftCreationTime = ftCreationTime;
		result.ftLastAccessTime = ftLastAccessTime;
		result.ftLastWriteTime = ftLastWriteTime;
		result.nFileSizeHigh = nFileSizeHigh;
		result.nFileSizeLow = nFileSizeLow;
		result.dwReserved0 = dwReserved0;
		result.dwReserved1 = dwReserved1;

		size_t charactersToCopy;
		if (ARRAYSIZE(result.cFileName) > fileName.size())
		{
			charactersToCopy = fileName.size();
		}
		else
		{
			charactersToCopy = ARRAYSIZE(result.cFileName) - 2;
		}

		memcpy(result.cFileName, fileName.data(), charactersToCopy * sizeof(wchar_t));
		result.cFileName[charactersToCopy] = 0;

		return result;
	}
};

struct FileOpenData : NonCopyable
{
	std::wstring filePath;
	uint64_t fileSize;
	FileFindData fileFindData;

	FileOpenData() :
		fileSize(0)
	{
	}

	FileOpenData(std::wstring&& filePath, uint64_t fileSize, const WIN32_FIND_DATAW& fileFindData) :
		filePath(std::move(filePath)),
		fileSize(fileSize),
		fileFindData(fileFindData)
	{
	}

	FileOpenData(FileOpenData&& other) :
		filePath(std::move(other.filePath)),
		fileSize(other.fileSize),
		fileFindData(other.fileFindData)
	{
	}

	FileOpenData& operator=(FileOpenData&& other)
	{
		filePath = std::move(other.filePath);
		fileSize = other.fileSize;
		fileFindData = other.fileFindData;
		return *this;
	}
};
