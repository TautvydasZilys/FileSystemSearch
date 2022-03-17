#include "PrecompiledHeader.h"
#include "AsynchronousPeriodicTimer.h"
#include "FileEnumerator.h"
#include "FileSearcher.h"
#include "PathUtils.h"
#include "ScopedStackAllocator.h"
#include "StringUtils.h"

FileSearcher::FileSearcher(SearchInstructions&& searchInstructions) :
	m_RefCount(1),
	m_SearchInstructions(std::move(searchInstructions)),
	m_FileOpenWorkQueue(*this),
	m_FileReadWorkQueue(m_SearchInstructions.searchString.length()),
	m_SearchResultDispatchWorkQueue(*this),
	m_FinishedSearchingFileSystem(false),
	m_IsFinished(false),
	m_FailedInit(false)
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

		auto hr = DStorageGetFactory(__uuidof(m_DStorageFactory), &m_DStorageFactory);
		Assert(SUCCEEDED(hr));

		if (FAILED(hr))
		{
			m_SearchInstructions.onError(L"Failed to initialize DirectStorage.");
			m_FailedInit = true;
			return;
		}

#if _DEBUG
		m_DStorageFactory->SetDebugFlags(DSTORAGE_DEBUG_SHOW_ERRORS | DSTORAGE_DEBUG_BREAK_ON_ERROR);
#endif

		m_FileOpenWorkQueue.Initialize<&FileSearcher::InitializeFileOpenThread>(systemInfo.dwNumberOfProcessors - 2);
		m_FileReadWorkQueue.Initialize();
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
	m_FileOpenWorkQueue.Cleanup();
	m_FileReadWorkQueue.Cleanup();
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
				if (m_IsFinished)
					return;

                if (m_SearchInstructions.IgnoreDotStart() && findData.cFileName[0] == '.')
                    return;

				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					directoriesToSearch.push_back(PathUtils::CombinePaths(directory, findData.cFileName));
			});
		}

		// Second, enumerate directory using our filters
		EnumerateFileSystem(directory, m_SearchInstructions.searchPattern.c_str(), m_SearchInstructions.searchPattern.length(), fileSystemEnumerationFlags, stackAllocator, [this, &directory, &directoriesToSearch, &stackAllocator](WIN32_FIND_DATAW& findData)
		{
            if (m_SearchInstructions.IgnoreDotStart() && findData.cFileName[0] == '.')
                return;

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
	if (!m_SearchInstructions.SearchForFiles() || m_IsFinished)
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

	if (!m_SearchInstructions.SearchInFileContents() || fileSize == 0)
		return;

	m_FileOpenWorkQueue.PushWorkItem(PathUtils::CombinePaths(directory, findData.cFileName), fileSize, findData);
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

void FileSearcher::InitializeFileOpenThread(WorkQueue<FileSearcher, FileOpenData>& fileOpenWorkQueue)
{
	fileOpenWorkQueue.DoWork([this](FileOpenData& searchData)
	{
		FileReadData readData(std::move(searchData));
		auto hr = m_DStorageFactory->OpenFile(readData.filePath.c_str(), __uuidof(readData.file), &readData.file);
		if (FAILED(hr))
		{
			AddToScannedFileSize(readData.fileSize);
			return;
		}

		m_FileReadWorkQueue.PushWorkItem(std::move(readData));
	});
}

/*void FileSearcher::InitializeFileReadThread(WorkQueue<FileSearcher, FileContentSearchData>& fileReadWorkQueue)
{
	FileReadThreadContext threadContext(m_DStorageFactory.Get());

	fileReadWorkQueue.DoWork([this, &threadContext](const FileContentSearchData& searchData)
	{
		ReadSearchedFile(threadContext, searchData);

		// InterlockedIncrement(&m_SearchStatistics.fileContentsSearched);
	});
}*/

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

/*
static inline uint32_t InitiateFileRead(IDStorageFile* file, uint64_t fileOffset, uint64_t fileSize, uint8_t* buffer, IDStorageQueue* dstorageQueue, IDStorageStatusArray* statusArray, uint64_t& readIndex)
{
	uint32_t bytesToRead = static_cast<uint32_t>(std::min(fileSize - fileOffset, kFileReadBufferSize));

	DSTORAGE_REQUEST request = {};
	request.Source.File.Source = file;
	request.Source.File.Offset = fileOffset;
	request.Source.File.Size = bytesToRead;
	request.Destination.Memory.Buffer = buffer;
	request.Destination.Memory.Size = kFileReadBufferSize;
	request.UncompressedSize = kFileReadBufferSize;
	request.CancellationTag = readIndex++;

	dstorageQueue->EnqueueRequest(&request);
	dstorageQueue->EnqueueStatus(statusArray, 0);
	dstorageQueue->Submit();

	return bytesToRead;
}

inline HRESULT BlockUntilCompletion(IDStorageStatusArray* statusArray)
{
	for (;;)
	{
		if (statusArray->IsComplete(0))
			break;

		Sleep(0);
	}

	return statusArray->GetHResult(0);
}

void FileSearcher::SearchFileContents(const FileContentSearchData& searchData, uint8_t* readBuffer, uint8_t* scanBuffer, ScopedStackAllocator& stackAllocator, IDStorageQueue* dstorageQueue, IDStorageStatusArray* statusArray, uint64_t& readIndex)
{
	const DWORD kFileSharingFlags = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE; // We really don't want to step on anyones toes
	uint64_t fileOffset = 0;

	Microsoft::WRL::ComPtr<IDStorageFile> file;
	auto hr = m_DStorageFactory->OpenFile(searchData.filePath.c_str(), __uuidof(file), &file);

	if (FAILED(hr))
	{
		AddToScannedFileSize(searchData.fileSize);
		return;
	}

	uint32_t bytesRead = InitiateFileRead(file.Get(), fileOffset, searchData.fileSize, scanBuffer, dstorageQueue, statusArray, readIndex);
	fileOffset += bytesRead;
	if (fileOffset != searchData.fileSize)
		fileOffset -= m_SearchInstructions.searchString.length() * sizeof(wchar_t);

	hr = BlockUntilCompletion(statusArray);
	Assert(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		AddToScannedFileSize(searchData.fileSize);
		return;
	}

	while (!m_IsFinished && searchData.fileSize - fileOffset > 0)
	{
		auto newBytesRead = InitiateFileRead(file.Get(), fileOffset, searchData.fileSize, readBuffer, dstorageQueue, statusArray, readIndex);

		if (PerformFileContentSearch(scanBuffer, bytesRead, stackAllocator))
		{
			AddToScannedFileSize(bytesRead + searchData.fileSize - fileOffset);
			DispatchSearchResult(searchData.fileFindData, searchData.filePath.c_str());
			BlockUntilCompletion(statusArray);
			return;
		}
		else
		{
			AddToScannedFileSize(bytesRead);
		}

		hr = BlockUntilCompletion(statusArray);
		Assert(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			AddToScannedFileSize(searchData.fileSize - fileOffset);
			return;
		}

		bytesRead = newBytesRead;
		fileOffset += bytesRead;
		if (fileOffset != searchData.fileSize)
			fileOffset -= m_SearchInstructions.searchString.length() * sizeof(wchar_t);

		std::swap(readBuffer, scanBuffer);
	}

	hr = BlockUntilCompletion(statusArray);
	if (SUCCEEDED(hr) && PerformFileContentSearch(scanBuffer, bytesRead, stackAllocator))
		DispatchSearchResult(searchData.fileFindData, searchData.filePath.c_str());

	AddToScannedFileSize(bytesRead);
}

void FileSearcher::ReadSearchedFile(FileReadThreadContext& context, const FileContentSearchData& searchData)
{
	const auto maxSearchStringLength = m_SearchInstructions.searchString.length() * sizeof(wchar_t);
	const auto fileSize = searchData.fileSize;

	DSTORAGE_REQUEST request = {};
	request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
	request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
	request.Options.CompressionFormat = DSTORAGE_CUSTOM_COMPRESSION_0;
	request.Source.File.Source = searchData.file.Get();
	request.Destination.Memory.Size = FileReadThreadContext::kFileReadBufferSize;
	request.UncompressedSize = FileReadThreadContext::kFileReadBufferSize;

	for (uint64_t fileOffset = 0, i = 0; fileOffset < fileSize; fileOffset += FileReadThreadContext::kFileReadBufferSize - maxSearchStringLength, i++)
	{
		Assert(i < FileReadThreadContext::kFileReadBufferSize);

		uint32_t bytesToRead = static_cast<uint32_t>(std::min(fileSize - fileOffset, FileReadThreadContext::kFileReadBufferSize));
		request.Destination.Memory.Buffer = context.fileReadBuffers.get() + i * FileReadThreadContext::kFileReadBufferSize;
		request.Source.File.Offset = fileOffset;
		request.Source.File.Size = bytesToRead;
		request.CancellationTag = context.readIndex++;
	
		context.dstorageQueue->EnqueueRequest(&request);
	}

	context.dstorageQueue->Submit();

	Microsoft::WRL::ComPtr<IDStorageCustomDecompressionQueue> decompressionQueue;
	auto hr = m_DStorageFactory.As(&decompressionQueue);
	Assert(SUCCEEDED(hr));

	while (true)
	{
		DSTORAGE_CUSTOM_DECOMPRESSION_REQUEST decompressionRequest;
		uint32_t numRequests = 0;

		decompressionQueue->GetRequests(1, &decompressionRequest, &numRequests);

		if (numRequests == 0)
		{
			Sleep(0);
			continue;
		}

		__debugbreak();
		break;
	}
}*/

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
	FileSearcher* searcher = new FileSearcher(std::forward<SearchInstructions>(searchInstructions));
	if (searcher->m_FailedInit)
	{
		searcher->Release();
		return nullptr;
	}

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
	Release();
}