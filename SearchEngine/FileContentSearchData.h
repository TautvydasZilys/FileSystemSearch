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
	uint32_t chunksRead;
	uint16_t readsInProgress;
	bool dispatchedToResults;
	uint64_t totalScannedSize;

	FileReadStateData() :
		chunksRead(0),
		readsInProgress(0),
		dispatchedToResults(false),
		totalScannedSize(0)
	{
	}

	FileReadStateData(FileReadData&& other) :
		FileReadData(std::move(other)),
		chunksRead(0),
		readsInProgress(0),
		dispatchedToResults(false),
		totalScannedSize(0)
	{
	}

	FileReadStateData(FileReadStateData&& other) :
		FileReadData(std::move(other)),
		chunksRead(other.chunksRead),
		readsInProgress(other.readsInProgress),
		dispatchedToResults(other.dispatchedToResults),
		totalScannedSize(other.totalScannedSize)
	{
	}

	FileReadStateData& operator=(FileReadStateData&& other)
	{
		static_cast<FileReadData&>(*this) = std::move(other);
		chunksRead = other.chunksRead;
		readsInProgress = other.readsInProgress;
		dispatchedToResults = other.dispatchedToResults;
		totalScannedSize = other.totalScannedSize;
		return *this;
	}
};

struct SlotSearchData
{
	uint16_t slot;
	bool found;

	SlotSearchData(uint16_t slot) :
		slot(slot),
		found(false)
	{
	}
};
