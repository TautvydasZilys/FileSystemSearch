#pragma once

#include "FileContentSearchData.h"

struct DirectStorageFileReadData : FileOpenData
{
	ComPtr<IDStorageFile> file;

	DirectStorageFileReadData() {}

	DirectStorageFileReadData(DirectStorageFileReadData&& other) :
		FileOpenData(std::move(other)),
		file(std::move(other.file))
	{
	}

	DirectStorageFileReadData(FileOpenData&& other) :
		FileOpenData(std::move(other))
	{
	}

	DirectStorageFileReadData& operator=(DirectStorageFileReadData&& other)
	{
		static_cast<FileOpenData&>(*this) = std::move(other);
		file = std::move(other.file);
		return *this;
	}
};

struct DirectStorageFileReadStateData : DirectStorageFileReadData
{
	uint32_t chunksRead;
	uint16_t readsInProgress;
	bool dispatchedToResults;
	uint64_t totalScannedSize;

	DirectStorageFileReadStateData() :
		chunksRead(0),
		readsInProgress(0),
		dispatchedToResults(false),
		totalScannedSize(0)
	{
	}

	DirectStorageFileReadStateData(DirectStorageFileReadData&& other) :
		DirectStorageFileReadData(std::move(other)),
		chunksRead(0),
		readsInProgress(0),
		dispatchedToResults(false),
		totalScannedSize(0)
	{
	}

	DirectStorageFileReadStateData(DirectStorageFileReadStateData&& other) :
		DirectStorageFileReadData(std::move(other)),
		chunksRead(other.chunksRead),
		readsInProgress(other.readsInProgress),
		dispatchedToResults(other.dispatchedToResults),
		totalScannedSize(other.totalScannedSize)
	{
	}

	DirectStorageFileReadStateData& operator=(DirectStorageFileReadStateData&& other)
	{
		static_cast<DirectStorageFileReadData&>(*this) = std::move(other);
		chunksRead = other.chunksRead;
		readsInProgress = other.readsInProgress;
		dispatchedToResults = other.dispatchedToResults;
		totalScannedSize = other.totalScannedSize;
		return *this;
	}
};

struct SlotSearchData
{
	uint32_t size;
	uint16_t slot;
	bool found;

	SlotSearchData(uint16_t slot, uint32_t size) :
		size(size),
		slot(slot),
		found(false)
	{
	}
};
