#include "PrecompiledHeader.h"
#include "AsynchronousPeriodicTimer.h"
#include "FileEnumerator.h"
#include "FileSearcher.h"
#include "PathUtils.h"
#include "StringUtils.h"

FileSearcher::FileSearcher(SearchInstructions&& searchInstructions) :
	m_RefCount(1),
	m_SearchInstructions(std::move(searchInstructions)),
	m_FileContentSearchWorkQueue(*this),
	m_SearchResultDispatchWorkQueue(*this),
	m_FinishedSearchingFileSystem(false),
	m_IsFinished(false)
{
	ZeroMemory(&m_SearchStatistics, sizeof(m_SearchStatistics));

	QueryPerformanceCounter(&m_SearchStart);
	QueryPerformanceFrequency(&m_PerformanceFrequency);

	// Sanity checks
	if (m_SearchInstructions.searchString.length() == 0)
		__fastfail(1);

	if (m_SearchInstructions.searchString.length() > std::numeric_limits<int32_t>::max())
		__fastfail(1);

	m_SearchStringIsAscii = StringUtils::IsAscii(m_SearchInstructions.searchString);

	if (m_SearchInstructions.IgnoreCase() && m_SearchStringIsAscii)
		StringUtils::ToLowerAsciiInline(m_SearchInstructions.searchString);

	if (m_SearchStringIsAscii || !m_SearchInstructions.IgnoreCase())
		m_OrdinalUtf16Searcher.Initialize(m_SearchInstructions.searchString.c_str(), m_SearchInstructions.searchString.length());
	else
		m_UnicodeUtf16Searcher.Initialize(m_SearchInstructions.searchString.c_str(), m_SearchInstructions.searchString.length());

	if (m_SearchInstructions.SearchInFileContents() && m_SearchInstructions.SearchContentsAsUtf8())
	{
		m_SearchInstructions.utf8SearchString = StringUtils::Utf16ToUtf8(m_SearchInstructions.searchString);
		m_OrdinalUtf8Searcher.Initialize(m_SearchInstructions.utf8SearchString.c_str(), m_SearchInstructions.utf8SearchString.length());
	}

	if (m_SearchInstructions.SearchInFileContents())
	{
		SYSTEM_INFO systemInfo;
		GetNativeSystemInfo(&systemInfo);

		m_FileContentSearchWorkQueue.Initialize<&FileSearcher::InitializeFileContentSearchThread>(systemInfo.dwNumberOfProcessors);
	}

	m_SearchResultDispatchWorkQueue.Initialize<&FileSearcher::InitializeSearchResultDispatcherWorkerThread>(1);
}

void FileSearcher::AddRef()
{
	InterlockedIncrement(&m_RefCount);
}

void FileSearcher::Release()
{
	if (InterlockedDecrement(&m_RefCount) == 0)
		delete this;
}

void FileSearcher::Search()
{
	const uint32_t kProgressReportIntervalInMilliseconds = 100;
	auto progressTimer = MakePeriodicTimer(kProgressReportIntervalInMilliseconds, [this]()
	{
		ReportProgress();
	});

	// Start reporting progress
	progressTimer.Start();

	// Search filesystem for files
	SearchFileSystem();

	// Wait for worker threads to finish
	m_FileContentSearchWorkQueue.Cleanup();
	m_SearchResultDispatchWorkQueue.Cleanup();

	// Stop reporting progress
	progressTimer.Stop();

	// Note total search time
	m_SearchStatistics.searchTimeInSeconds = GetTotalSearchTimeInSeconds();

	// Fire done event
	m_SearchInstructions.onDone(m_SearchStatistics);

	// Release ref count
	Release();
}

void FileSearcher::SearchFileSystem()
{
	ScopedStackAllocator stackAllocator;
	std::vector<std::wstring> directoriesToSearch;
	directoriesToSearch.push_back(m_SearchInstructions.searchPath);

	auto fileSystemEnumerationFlags = FileSystemEnumerationFlags::kEnumerateFiles;

	if (m_SearchInstructions.SearchRecursively() || m_SearchInstructions.SearchForDirectories())
		fileSystemEnumerationFlags |= FileSystemEnumerationFlags::kEnumerateDirectories;

	// Do this iteratively rather than recursively. Much easier to profile it that way.
	while (directoriesToSearch.size() > 0 && !m_IsFinished)
	{
		auto directory = std::move(directoriesToSearch[0]);
		directoriesToSearch[0] = std::move(directoriesToSearch[directoriesToSearch.size() - 1]);
		directoriesToSearch.pop_back();

		// First, find all directories if we're searching recursively
		if (m_SearchInstructions.SearchRecursively())
		{
			EnumerateFileSystem(directory, L"*", 1, FileSystemEnumerationFlags::kEnumerateDirectories, stackAllocator, [this, &directory, &directoriesToSearch](WIN32_FIND_DATAW& findData)
			{
				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					directoriesToSearch.push_back(PathUtils::CombinePaths(directory, findData.cFileName));
			});
		}

		// Second, enumerate directory using our filters
		EnumerateFileSystem(directory, m_SearchInstructions.searchPattern.c_str(), m_SearchInstructions.searchPattern.length(), fileSystemEnumerationFlags, stackAllocator, [this, &directory, &directoriesToSearch, &stackAllocator](WIN32_FIND_DATAW& findData)
		{
			m_SearchStatistics.filesEnumerated++;

			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				OnDirectoryFound(directory, findData, stackAllocator);
			}
			else
			{
				OnFileFound(directory, findData, stackAllocator);
			}
		});

		m_SearchStatistics.directoriesEnumerated++;
	}

	m_FinishedSearchingFileSystem = true;
}

void FileSearcher::OnDirectoryFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator)
{
	if (!m_SearchInstructions.SearchForDirectories())
		return;

	SearchInFileName(directory, findData, m_SearchInstructions.SearchInDirectoryPath(), stackAllocator);
}

void FileSearcher::OnFileFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData, ScopedStackAllocator& stackAllocator)
{
	if (!m_SearchInstructions.SearchForFiles())
		return;

	uint64_t fileSize = (static_cast<uint64_t>(findData.nFileSizeHigh) << 32) + findData.nFileSizeLow;
	if (fileSize > m_SearchInstructions.ignoreFilesLargerThan)
		return;

	if (m_SearchInstructions.SearchInFilePath())
	{
		if (SearchInFileName(directory, findData, true, stackAllocator))
			return;
	}
	else if (m_SearchInstructions.SearchInFileName())
	{
		if (SearchInFileName(directory, findData, false, stackAllocator))
			return;
	}

	m_SearchStatistics.totalFileSize += fileSize;

	if (!m_SearchInstructions.SearchInFileContents())
		return;

	m_FileContentSearchWorkQueue.PushWorkItem(PathUtils::CombinePaths(directory, findData.cFileName), fileSize, findData);
}

bool FileSearcher::SearchInFileName(const std::wstring& directory, const WIN32_FIND_DATAW& findData, bool searchInPath, ScopedStackAllocator& stackAllocator)
{
	auto fileNameLength = wcslen(findData.cFileName);

	if (searchInPath)
	{
		size_t pathLength;
		auto path = PathUtils::CombinePathsTemporary(directory, findData.cFileName, fileNameLength, stackAllocator, pathLength);
		if (SearchForString(path, pathLength, stackAllocator))
		{
			DispatchSearchResult(findData, std::wstring(path));
			return true;
		}
	}
	else
	{
		if (SearchForString(findData.cFileName, fileNameLength, stackAllocator))
		{
			auto path = PathUtils::CombinePathsTemporary(directory, findData.cFileName, fileNameLength, stackAllocator);
			DispatchSearchResult(findData, std::wstring(path));
			return true;
		}
	}

	return false;
}

bool FileSearcher::SearchForString(const wchar_t* str, size_t length, ScopedStackAllocator& stackAllocator)
{
	if (m_SearchInstructions.IgnoreCase())
	{
		if (m_SearchStringIsAscii)
		{
			auto memory = stackAllocator.Allocate(sizeof(wchar_t) * length);
			auto lowerCase = static_cast<wchar_t*>(memory);			
			StringUtils::ToLowerAscii(str, lowerCase, length);
			return m_OrdinalUtf16Searcher.HasSubstring(lowerCase, lowerCase + length);
		}
		else
		{
			return m_UnicodeUtf16Searcher.HasSubstring(str, str + length);
		}
	}

	return m_OrdinalUtf16Searcher.HasSubstring(str, str + length);
}

const size_t kFileReadBufferSize = 5 * 1024 * 1024; // 5 MB

void FileSearcher::InitializeFileContentSearchThread(WorkQueue<FileSearcher, FileContentSearchData>& contentSearchWorkQueue)
{
	ScopedStackAllocator stackAllocator;
	std::unique_ptr<uint8_t[]> fileReadBuffers[2] =
	{
		std::unique_ptr<uint8_t[]>(new uint8_t[kFileReadBufferSize]),
		std::unique_ptr<uint8_t[]>(new uint8_t[kFileReadBufferSize]),
	};

	contentSearchWorkQueue.DoWork([this, &fileReadBuffers, &stackAllocator](const FileContentSearchData& searchData)
	{
		SearchFileContents(searchData, fileReadBuffers[0].get(), fileReadBuffers[1].get(), stackAllocator);
		InterlockedIncrement(&m_SearchStatistics.fileContentsSearched);
	});
}

void FileSearcher::InitializeSearchResultDispatcherWorkerThread(WorkQueue<FileSearcher, SearchResultData>& workQueue)
{
	workQueue.DoWork([this](const SearchResultData& searchResult)
	{
		m_SearchInstructions.onFoundPath(&searchResult.resultFindData, searchResult.resultPath.c_str());
	});
}

double FileSearcher::GetTotalSearchTimeInSeconds()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	return static_cast<double>(currentTime.QuadPart - m_SearchStart.QuadPart) / static_cast<double>(m_PerformanceFrequency.QuadPart);
}

void FileSearcher::ReportProgress()
{
	// This is bad: we're reading m_SearchStatistics which is constantly being updated from other threads. 
	// This read isn't atomic, that means we might read the statistics that didn't actually exist as a whole at any given
	// HOWEVER, I believe it still is a better alternative than protecting it with a critical section, as that would just utterly destroy performance
	// Some glitches in the statistics should be okay, as you can't really tell just by looking at them if they're correct or not
	auto statisticsSnapshot = m_SearchStatistics;

	double progress = m_FinishedSearchingFileSystem 
		? static_cast<double>(statisticsSnapshot.scannedFileSize) / static_cast<double>(statisticsSnapshot.totalFileSize)
		: sqrt(-1); // indeterminate

	statisticsSnapshot.searchTimeInSeconds = GetTotalSearchTimeInSeconds();
	m_SearchInstructions.onProgressUpdated(statisticsSnapshot, progress);
}

void FileSearcher::DispatchSearchResult(const WIN32_FIND_DATAW& findData, std::wstring&& path)
{
	InterlockedIncrement(&m_SearchStatistics.resultsFound);
	m_SearchResultDispatchWorkQueue.PushWorkItem(std::forward<std::wstring>(path), findData);
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

void FileSearcher::SearchFileContents(const FileContentSearchData& searchData, uint8_t* primaryBuffer, uint8_t* secondaryBuffer, ScopedStackAllocator& stackAllocator)
{
	const DWORD kFileSharingFlags = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE; // We really don't want to step on anyones toes
	uint64_t fileOffset = 0;

	HandleHolder fileHandle = CreateFileW(searchData.filePath.c_str(), GENERIC_READ, kFileSharingFlags, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		AddToScannedFileSize(searchData.fileSize);
		return;
	}

	HandleHolder overlappedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	Assert(overlappedEvent != nullptr);

	OVERLAPPED overlapped;
	if (!InitiateFileRead(fileHandle, fileOffset, searchData.fileSize, primaryBuffer, secondaryBuffer, overlapped, overlappedEvent))
	{
		AddToScannedFileSize(searchData.fileSize);
		return;
	}

	auto waitResult = WaitForSingleObject(overlappedEvent, INFINITE);
	Assert(waitResult == WAIT_OBJECT_0);

	uint32_t bytesRead = static_cast<uint32_t>(overlapped.InternalHigh);
	fileOffset += bytesRead;
	if (fileOffset != searchData.fileSize)
		fileOffset -= m_SearchInstructions.searchString.length() * sizeof(wchar_t);

	while (!m_IsFinished && searchData.fileSize - fileOffset > 0)
	{
		if (!InitiateFileRead(fileHandle, fileOffset, searchData.fileSize, primaryBuffer, secondaryBuffer, overlapped, overlappedEvent))
		{
			AddToScannedFileSize(searchData.fileSize - fileOffset);
			return;
		}

		if (PerformFileContentSearch(primaryBuffer, bytesRead, stackAllocator))
		{
			AddToScannedFileSize(bytesRead + searchData.fileSize - fileOffset);
			DispatchSearchResult(searchData.fileFindData, searchData.filePath.c_str());
			return;
		}
		else
		{
			AddToScannedFileSize(bytesRead);
		}

		waitResult = WaitForSingleObject(overlappedEvent, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);

		bytesRead = static_cast<uint32_t>(overlapped.InternalHigh);
		fileOffset += bytesRead;
		if (fileOffset != searchData.fileSize)
			fileOffset -= m_SearchInstructions.searchString.length() * sizeof(wchar_t);
	}

	if (PerformFileContentSearch(secondaryBuffer, bytesRead, stackAllocator))
		DispatchSearchResult(searchData.fileFindData, searchData.filePath.c_str());

	AddToScannedFileSize(bytesRead);
}

bool FileSearcher::PerformFileContentSearch(uint8_t* fileBytes, uint32_t bufferLength, ScopedStackAllocator& stackAllocator)
{
	if (m_SearchInstructions.SearchContentsAsUtf16())
	{
		if (SearchForString(reinterpret_cast<const wchar_t*>(fileBytes), bufferLength / sizeof(wchar_t), stackAllocator))
			return true;
	}

	if (!m_SearchInstructions.SearchContentsAsUtf8())
		return false;

	if (!m_SearchStringIsAscii && m_SearchInstructions.IgnoreCase())
		__fastfail(1); // Not implemented

	if (m_SearchInstructions.IgnoreCase())
		StringUtils::ToLowerAscii(fileBytes, fileBytes, bufferLength);

	return m_OrdinalUtf8Searcher.HasSubstring(fileBytes, fileBytes + bufferLength);
}

void FileSearcher::AddToScannedFileSize(int64_t size)
{
	InterlockedAdd64(&m_SearchStatistics.scannedFileSize, size);
}

FileSearcher* FileSearcher::BeginSearch(SearchInstructions&& searchInstructions)
{
	auto searcher = new FileSearcher(std::forward<SearchInstructions>(searchInstructions));
	searcher->AddRef();

	auto fileSystemSearchThread = CreateThread(nullptr, 64 * 1024, [](void* ctx) -> DWORD
	{
		static_cast<FileSearcher*>(ctx)->Search();
		return 0;
	}, searcher, 0, nullptr);

	searcher->m_FileSystemSearchThread = fileSystemSearchThread;
	return searcher;
}

void FileSearcher::Cleanup()
{
	m_IsFinished = true;

	m_FileContentSearchWorkQueue.DrainWorkQueue();
	m_SearchResultDispatchWorkQueue.DrainWorkQueue();

	Release();
}