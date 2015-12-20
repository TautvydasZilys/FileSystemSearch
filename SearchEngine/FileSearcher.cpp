#include "PrecompiledHeader.h"
#include "FileEnumerator.h"
#include "FileSearcher.h"
#include "PathUtils.h"
#include "StringUtils.h"

FileSearcher::FileSearcher(SearchInstructions&& searchInstructions) :
	m_RefCount(1),
	m_SearchInstructions(std::move(searchInstructions)),
	m_WorkerThreadCount(0)
{
	// Sanity checks
	if (m_SearchInstructions.searchString.length() == 0)
		__fastfail(1);

	if (m_SearchInstructions.searchString.length() > std::numeric_limits<int32_t>::max())
		__fastfail(1);

	m_SearchStringIsAscii = StringUtils::IsAscii(m_SearchInstructions.searchString);

	if (m_SearchInstructions.IgnoreCase())
	{
		if (m_SearchStringIsAscii)
			StringUtils::ToLowerAsciiInline(m_SearchInstructions.searchString);
		else
			m_SearchInstructions.searchString = StringUtils::ToLowerUnicode(m_SearchInstructions.searchString);
	}

	if (m_SearchStringIsAscii)
		m_OrdinalUtf16Searcher.Initialize(m_SearchInstructions.searchString.c_str(), m_SearchInstructions.searchString.length());
	else
		m_UnicodeUtf16Searcher.Initialize(m_SearchInstructions.searchString.c_str(), m_SearchInstructions.searchString.length());

	if (m_SearchInstructions.SearchInFileContents() && m_SearchInstructions.SearchContentsAsUtf8())
	{
		m_SearchInstructions.utf8SearchString = StringUtils::Utf16ToUtf8(m_SearchInstructions.searchString);
		m_OrdinalUtf8Searcher.Initialize(m_SearchInstructions.utf8SearchString.c_str(), m_SearchInstructions.utf8SearchString.length());
	}

	if (m_SearchInstructions.SearchInFileContents())
		InitializeContentSearchThreads();
}

FileSearcher::~FileSearcher()
{
	if (m_SearchInstructions.SearchInFileContents())
	{
		auto releaseResult = ReleaseSemaphore(m_WorkSemaphore, m_WorkerThreadCount, nullptr);
		Assert(releaseResult != FALSE);

		auto waitResult = WaitForMultipleObjects(m_WorkerThreadCount, reinterpret_cast<const HANDLE*>(m_WorkerThreadHandles.data()), TRUE, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0 + m_WorkerThreadCount - 1);

		Assert(QueryDepthSList(m_WorkList) == 0); // If this fires, something very very wrong

		_aligned_free(m_WorkList);
	}

	m_SearchInstructions.onDone();
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

struct WorkerJob
{
	SLIST_ENTRY slistEntry;
	std::wstring filePath;
	uint64_t fileSize;
};

void FileSearcher::InitializeContentSearchThreads()
{
	SYSTEM_INFO systemInfo;
	GetNativeSystemInfo(&systemInfo);

	m_WorkerThreadCount = systemInfo.dwNumberOfProcessors;

	m_WorkSemaphore = CreateSemaphoreW(nullptr, 0, std::numeric_limits<LONG>::max(), nullptr);
	Assert(m_WorkSemaphore != nullptr);

	m_WorkList = static_cast<SLIST_HEADER*>(_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT));
	InitializeSListHead(m_WorkList);
	m_WorkerThreadHandles.reserve(m_WorkerThreadCount);

	for (uint32_t i = 0; i < m_WorkerThreadCount; i++)
	{
		auto threadHandle = CreateThread(nullptr, 64 * 1024, [](void* context) -> DWORD
		{
			static_cast<FileSearcher*>(context)->WorkerThreadLoop();
			return 0;
		}, this, 0, nullptr);
		
		auto setThreadPriorityResult = SetThreadPriority(threadHandle, THREAD_PRIORITY_BELOW_NORMAL);
		Assert(setThreadPriorityResult != FALSE);

		m_WorkerThreadHandles.push_back(threadHandle);
	}
}

void FileSearcher::Search()
{
	ScopedStackAllocator stackAllocator;
	std::vector<std::wstring> directoriesToSearch;
	directoriesToSearch.push_back(m_SearchInstructions.searchPath);

	auto fileSystemEnumerationFlags = FileSystemEnumerationFlags::kEnumerateFiles;

	if (m_SearchInstructions.SearchRecursively() || m_SearchInstructions.SearchForDirectories())
		fileSystemEnumerationFlags |= FileSystemEnumerationFlags::kEnumerateDirectories;

	// Do this iteratively rather than recursively. Much easier to profile it that way.
	while (directoriesToSearch.size() > 0)
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
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				OnDirectoryFound(directory, findData, stackAllocator);
			}
			else
			{
				OnFileFound(directory, findData, stackAllocator);
			}
		});
	}
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

	if (!m_SearchInstructions.SearchInFileContents())
		return;

	auto memory = _aligned_malloc(sizeof(WorkerJob), MEMORY_ALLOCATION_ALIGNMENT);
	WorkerJob* job = new (memory) WorkerJob; // Needed to construct std::wstring that's inside
	job->slistEntry.Next = nullptr;
	job->filePath = PathUtils::CombinePaths(directory, findData.cFileName);
	job->fileSize = fileSize;

	InterlockedPushEntrySList(m_WorkList, &job->slistEntry);

	auto releaseResult = ReleaseSemaphore(m_WorkSemaphore, 1, nullptr);
	Assert(releaseResult != FALSE);
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
			m_SearchInstructions.onFoundPath(&findData, path);
			return true;
		}
	}
	else
	{
		if (SearchForString(findData.cFileName, fileNameLength, stackAllocator))
		{
			auto path = PathUtils::CombinePathsTemporary(directory, findData.cFileName, fileNameLength, stackAllocator);
			m_SearchInstructions.onFoundPath(&findData, path);
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

void FileSearcher::WorkerThreadLoop()
{
	ScopedStackAllocator stackAllocator;
	std::unique_ptr<uint8_t[]> fileReadBuffers[2] =
	{
		std::unique_ptr<uint8_t[]>(new uint8_t[kFileReadBufferSize]),
		std::unique_ptr<uint8_t[]>(new uint8_t[kFileReadBufferSize]),
	};

	for (;;)
	{
		auto waitResult = WaitForSingleObject(m_WorkSemaphore, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);

		auto job = reinterpret_cast<WorkerJob*>(InterlockedPopEntrySList(m_WorkList));
		if (job == nullptr)
			break;

		SearchFileContents(job, fileReadBuffers[0].get(), fileReadBuffers[1].get(), stackAllocator);
		_aligned_free(job);
	}
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

void FileSearcher::SearchFileContents(const WorkerJob* job, uint8_t* primaryBuffer, uint8_t* secondaryBuffer, ScopedStackAllocator& stackAllocator)
{
	const DWORD kFileSharingFlags = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE; // We really don't want to step on anyones toes
	uint64_t fileOffset = 0;

	HandleHolder fileHandle = CreateFileW(job->filePath.c_str(), GENERIC_READ, kFileSharingFlags, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

	if (fileHandle == INVALID_HANDLE_VALUE)
		return;

	HandleHolder overlappedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	Assert(overlappedEvent != nullptr);

	OVERLAPPED overlapped;
	if (!InitiateFileRead(fileHandle, fileOffset, job->fileSize, primaryBuffer, secondaryBuffer, overlapped, overlappedEvent))
		return;

	auto waitResult = WaitForSingleObject(overlappedEvent, INFINITE);
	Assert(waitResult == WAIT_OBJECT_0);

	uint32_t bytesRead = static_cast<uint32_t>(overlapped.InternalHigh);;
	fileOffset += bytesRead;
	if (fileOffset != job->fileSize)
		fileOffset -= m_SearchInstructions.searchString.length() * sizeof(wchar_t);

	while (job->fileSize - fileOffset > 0)
	{
		if (!InitiateFileRead(fileHandle, fileOffset, job->fileSize, primaryBuffer, secondaryBuffer, overlapped, overlappedEvent))
			return;

		if (PerformFileContentSearch(primaryBuffer, bytesRead, stackAllocator))
		{
			m_SearchInstructions.onFoundPath(nullptr, job->filePath.c_str());
			return;
		}

		auto waitResult = WaitForSingleObject(overlappedEvent, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);

		bytesRead = static_cast<uint32_t>(overlapped.InternalHigh);
		fileOffset += bytesRead;
		if (fileOffset != job->fileSize)
			fileOffset -= m_SearchInstructions.searchString.length() * sizeof(wchar_t);
	}

	if (PerformFileContentSearch(secondaryBuffer, bytesRead, stackAllocator))
		m_SearchInstructions.onFoundPath(nullptr, job->filePath.c_str());
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

void FileSearcher::Search(SearchInstructions&& searchInstructions)
{
	auto searcher = new FileSearcher(std::forward<SearchInstructions>(searchInstructions));
	
	searcher->Search();
	searcher->Release();
}