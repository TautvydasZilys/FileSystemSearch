#include "PrecompiledHeader.h"
#include "FileReadBackends/DirectStorage/DirectXContext.h"
#include "FileSearcher.h"
#include "Utilities/AsynchronousPeriodicTimer.h"
#include "Utilities/FileEnumerator.h"
#include "Utilities/PathUtils.h"
#include "Utilities/ScopedStackAllocator.h"
#include "Utilities/StringUtils.h"

FileSearcher::FileSearcher(SearchInstructions&& searchInstructions) :
	m_RefCount(1),
	m_SearchInstructions(std::move(searchInstructions)),
	m_StringSearcher(m_SearchInstructions),
	m_SearchResultReporter(m_SearchInstructions),
	m_FileReadWorkQueue(m_StringSearcher, m_SearchInstructions, m_SearchResultReporter),
	m_FinishedSearchingFileSystem(false),
	m_IsFinished(false),
	m_FailedInit(false)
{
	if (m_SearchInstructions.SearchInFileContents())
	{
		if (DirectXContext::GetDStorageFactory() == nullptr)
		{
			m_SearchInstructions.onError(L"Failed to initialize DirectStorage!");
			m_FailedInit = true;
		}
		else if (DirectXContext::GetD3D12Device() == nullptr)
		{
			m_SearchInstructions.onError(L"Failed to initialize DirectX 12!");
			m_FailedInit = true;
		}
		else
		{
			m_FileReadWorkQueue.Initialize();
		}
	}
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
		m_SearchResultReporter.ReportProgress(m_FinishedSearchingFileSystem);
	});

	// Start reporting progress
	progressTimer.Start();

	// Search filesystem for files
	SearchFileSystem();

	// Wait for worker threads to finish
	m_FileReadWorkQueue.CompleteAllWork();

	// Stop reporting progress
	progressTimer.Stop();

	// Fire done event
	m_SearchResultReporter.FinishSearch();

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
			EnumerateFileSystem(directory, std::wstring_view(L"*", 1), FileSystemEnumerationFlags::kEnumerateDirectories, stackAllocator, [this, &directory, &directoriesToSearch](WIN32_FIND_DATAW& findData)
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
		EnumerateFileSystem(directory, m_SearchInstructions.searchPattern, fileSystemEnumerationFlags, stackAllocator, [this, &directory, &directoriesToSearch, &stackAllocator](WIN32_FIND_DATAW& findData)
		{
            if (m_SearchInstructions.IgnoreDotStart() && findData.cFileName[0] == '.')
                return;

			m_SearchResultReporter.OnFileEnumeratedThreadUnsafe();

			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				OnDirectoryFound(directory, findData, stackAllocator);
			}
			else
			{
				OnFileFound(directory, findData, stackAllocator);
			}
		});

		m_SearchResultReporter.OnDirectoryEnumeratedThreadUnsafe();
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

	m_SearchResultReporter.OnTotalFileSizeAddedThreadUnsafe(fileSize);

	if (!m_SearchInstructions.SearchInFileContents() || fileSize == 0)
		return;

	m_FileReadWorkQueue.ScanFile(FileOpenData(PathUtils::CombinePaths(directory, findData.cFileName), fileSize, findData));
}

bool FileSearcher::SearchInFileName(const std::wstring& directory, const WIN32_FIND_DATAW& findData, bool searchInPath, ScopedStackAllocator& stackAllocator)
{
	std::wstring_view fileName(findData.cFileName, wcslen(findData.cFileName));

	if (searchInPath)
	{
		size_t pathLength;
		auto path = PathUtils::CombinePathsTemporary(directory, fileName, stackAllocator, pathLength);
		if (m_StringSearcher.SearchForString(std::wstring_view(path, pathLength), stackAllocator))
		{
			m_SearchResultReporter.DispatchSearchResult(findData, std::wstring(path));
			return true;
		}
	}
	else
	{
		if (m_StringSearcher.SearchForString(fileName, stackAllocator))
		{
			auto path = PathUtils::CombinePathsTemporary(directory, fileName, stackAllocator);
			m_SearchResultReporter.DispatchSearchResult(findData, std::wstring(path));
			return true;
		}
	}

	return false;
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
		SetThreadDescription(GetCurrentThread(), L"FSS File System Enumeration Thread");
		static_cast<FileSearcher*>(ctx)->Search();
		return 0;
	}, searcher, 0, nullptr);

	searcher->m_FileSystemSearchThread = fileSystemSearchThread;
	return searcher;
}

void FileSearcher::Cleanup()
{
	m_IsFinished = true;

	m_FileReadWorkQueue.DrainWorkQueue();
	m_SearchResultReporter.DrainWorkQueue();

	WaitForSingleObject(m_FileSystemSearchThread, INFINITE);

	Release();
}