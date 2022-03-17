#pragma once

#include "NonCopyable.h"

struct FileOpenData : NonCopyable
{
	std::wstring filePath;
	uint64_t fileSize;
	WIN32_FIND_DATAW fileFindData;

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

struct FileReadData : FileOpenData
{
	ComPtr<IDStorageFile> file;

	FileReadData() {}

	FileReadData(FileReadData&& other) :
		FileOpenData(std::move(other)),
		file(std::move(other.file))
	{
	}

	FileReadData(FileOpenData&& other) :
		FileOpenData(std::move(other))
	{
	}

	FileReadData& operator=(FileReadData&& other)
	{
		static_cast<FileOpenData&>(*this) = std::move(other);
		file = std::move(other.file);
		return *this;
	}
};

struct FileReadStateData : FileReadData
{
	uint32_t readsInProgress;
	uint32_t chunksRead;

	FileReadStateData() :
		readsInProgress(0),
		chunksRead(0)
	{
	}

	FileReadStateData(FileReadData&& other) :
		FileReadData(std::move(other)),
		readsInProgress(0),
		chunksRead(0)
	{
	}

	FileReadStateData(FileReadStateData&& other) :
		FileReadData(std::move(other)),
		readsInProgress(other.readsInProgress),
		chunksRead(other.chunksRead)
	{
	}

	FileReadStateData& operator=(FileReadStateData&& other)
	{
		static_cast<FileReadData&>(*this) = std::move(other);
		chunksRead = other.chunksRead;
		return *this;
	}
};
