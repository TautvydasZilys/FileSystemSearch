#pragma once

struct FileContentSearchData
{
	std::wstring filePath;
	uint64_t fileSize;

	FileContentSearchData() :
		fileSize(0)
	{
	}

	FileContentSearchData(std::wstring&& filePath, uint64_t fileSize) :
		filePath(std::move(filePath)),
		fileSize(fileSize)
	{
	}

	FileContentSearchData(const FileContentSearchData&) = delete;
	FileContentSearchData& operator=(const FileContentSearchData&) = delete;

	FileContentSearchData(FileContentSearchData&& other) :
		filePath(std::move(other.filePath)),
		fileSize(other.fileSize)
	{
	}

	FileContentSearchData& operator=(FileContentSearchData&& other)
	{
		std::swap(filePath, other.filePath);
		std::swap(fileSize, other.fileSize);
		return *this;
	}
};
