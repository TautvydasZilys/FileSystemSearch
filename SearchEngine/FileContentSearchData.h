#pragma once

struct FileContentSearchData
{
	std::wstring filePath;
	uint64_t fileSize;
	WIN32_FIND_DATAW fileFindData;

	FileContentSearchData() :
		fileSize(0)
	{
	}

	FileContentSearchData(std::wstring&& filePath, uint64_t fileSize, const WIN32_FIND_DATAW& fileFindData) :
		filePath(std::move(filePath)),
		fileSize(fileSize),
		fileFindData(fileFindData)
	{
	}

	FileContentSearchData(const FileContentSearchData&) = delete;
	FileContentSearchData& operator=(const FileContentSearchData&) = delete;

	FileContentSearchData(FileContentSearchData&& other) :
		filePath(std::move(other.filePath)),
		fileSize(other.fileSize),
		fileFindData(other.fileFindData)
	{
	}

	FileContentSearchData& operator=(FileContentSearchData&& other)
	{
		filePath = std::move(other.filePath);
		fileSize = other.fileSize;
		fileFindData = other.fileFindData;
		return *this;
	}
};
