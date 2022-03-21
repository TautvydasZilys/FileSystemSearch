#include "PrecompiledHeader.h"
#include "OverlappedIOReader.h"
#include "SearchInstructions.h"
#include "SearchResultReporter.h"
#include "StringSearch/StringSearcher.h"
#include "Utilities/ScopedStackAllocator.h"

const size_t kFileReadBufferSize = 5 * 1024 * 1024; // 5 MB

OverlappedIOReader::OverlappedIOReader(const StringSearcher& stringSearcher, const SearchInstructions& searchInstructions, SearchResultReporter& searchResultReporter) :
	m_SearchResultReporter(searchResultReporter),
	m_StringSearcher(stringSearcher),
	m_MaxSearchStringLength(std::max(searchInstructions.utf8SearchString.length(), searchInstructions.searchString.length() * sizeof(wchar_t)))
{
}

void OverlappedIOReader::Initialize()
{
	SYSTEM_INFO systemInfo;
	GetNativeSystemInfo(&systemInfo);

	MyBase::Initialize<&OverlappedIOReader::ContentsSearchThread>(this, systemInfo.dwNumberOfProcessors);
}

void OverlappedIOReader::DrainWorkQueue()
{
	m_IsFinished = true;
	MyBase::DrainWorkQueue();
}

void OverlappedIOReader::ContentsSearchThread()
{
	SetThreadDescription(GetCurrentThread(), L"FSS Overlapped I/O Reader Thread");

	ScopedStackAllocator stackAllocator;
	std::unique_ptr<uint8_t[]> fileReadBuffers[2] =
	{
		std::unique_ptr<uint8_t[]>(new uint8_t[kFileReadBufferSize]),
		std::unique_ptr<uint8_t[]>(new uint8_t[kFileReadBufferSize]),
	};

	DoWork([this, &fileReadBuffers, &stackAllocator](const FileOpenData& searchData)
	{
		SearchFileContents(searchData, fileReadBuffers[0].get(), fileReadBuffers[1].get(), stackAllocator);
		m_SearchResultReporter.AddToScannedFileCount();
	});
}

static inline bool InitiateFileRead(HANDLE fileHandle, uint64_t fileOffset, uint64_t fileSize, uint8_t*& primaryBuffer, uint8_t*& secondaryBuffer, OVERLAPPED& overlapped, HANDLE overlappedEvent)
{
	ZeroMemory(&overlapped, sizeof(overlapped));

	overlapped.hEvent = overlappedEvent;
	overlapped.Offset = static_cast<uint32_t>(fileOffset & 0xFFFFFFFF);
	overlapped.OffsetHigh = static_cast<uint32_t>(fileOffset >> 32);

	uint32_t bytesToRead = static_cast<uint32_t>(std::min(fileSize - fileOffset, kFileReadBufferSize));

	auto readResult = ReadFile(fileHandle, primaryBuffer, bytesToRead, nullptr, &overlapped);
	Assert(readResult != FALSE || GetLastError() == ERROR_IO_PENDING);

	std::swap(primaryBuffer, secondaryBuffer);
	return readResult != FALSE || GetLastError() == ERROR_IO_PENDING;
}

void OverlappedIOReader::SearchFileContents(const FileOpenData& searchData, uint8_t* primaryBuffer, uint8_t* secondaryBuffer, ScopedStackAllocator& stackAllocator)
{
	const DWORD kFileSharingFlags = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE; // We really don't want to step on anyones toes
	uint64_t fileOffset = 0;

	FileHandleHolder fileHandle = CreateFileW(searchData.filePath.c_str(), GENERIC_READ, kFileSharingFlags, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		m_SearchResultReporter.AddToScannedFileSize(searchData.fileSize);
		return;
	}

	EventHandleHolder overlappedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	Assert(overlappedEvent != nullptr);

	OVERLAPPED overlapped;
	if (!InitiateFileRead(fileHandle, fileOffset, searchData.fileSize, primaryBuffer, secondaryBuffer, overlapped, overlappedEvent))
	{
		m_SearchResultReporter.AddToScannedFileSize(searchData.fileSize);
		return;
	}

	auto waitResult = WaitForSingleObject(overlappedEvent, INFINITE);
	Assert(waitResult == WAIT_OBJECT_0);

	uint32_t bytesRead = static_cast<uint32_t>(overlapped.InternalHigh);
	fileOffset += bytesRead;
	if (fileOffset != searchData.fileSize)
		fileOffset -= m_MaxSearchStringLength;

	while (!m_IsFinished && searchData.fileSize - fileOffset > 0)
	{
		if (!InitiateFileRead(fileHandle, fileOffset, searchData.fileSize, primaryBuffer, secondaryBuffer, overlapped, overlappedEvent))
		{
			m_SearchResultReporter.AddToScannedFileSize(searchData.fileSize - fileOffset);
			return;
		}

		if (m_StringSearcher.PerformFileContentSearch(primaryBuffer, bytesRead, stackAllocator))
		{
			m_SearchResultReporter.AddToScannedFileSize(bytesRead + searchData.fileSize - fileOffset);
			m_SearchResultReporter.DispatchSearchResult(searchData.fileFindData, searchData.filePath.c_str());
			return;
		}
		else
		{
			m_SearchResultReporter.AddToScannedFileSize(bytesRead);
		}

		waitResult = WaitForSingleObject(overlappedEvent, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);

		bytesRead = static_cast<uint32_t>(overlapped.InternalHigh);
		fileOffset += bytesRead;
		if (fileOffset != searchData.fileSize)
			fileOffset -= m_MaxSearchStringLength;
	}

	if (m_StringSearcher.PerformFileContentSearch(secondaryBuffer, bytesRead, stackAllocator))
		m_SearchResultReporter.DispatchSearchResult(searchData.fileFindData, searchData.filePath.c_str());

	m_SearchResultReporter.AddToScannedFileSize(bytesRead);
}

