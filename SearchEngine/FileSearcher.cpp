#include "PrecompiledHeader.h"
#include "FileEnumerator.h"
#include "FileSearcher.h"
#include "PathUtils.h"
#include "StringUtils.h"

FileSearcher::FileSearcher(SearchInstructions&& searchInstructions) :
	m_RefCount(1),
	m_SearchInstructions(std::move(searchInstructions))
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
}

FileSearcher::~FileSearcher()
{
}

void FileSearcher::AddRef()
{
	InterlockedIncrement(&m_RefCount);
}

void FileSearcher::Release()
{
	if (InterlockedDecrement(&m_RefCount) == 0)
	{
		m_SearchInstructions.onDone();
		delete this;
	}
}

void FileSearcher::Search()
{
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
			EnumerateFileSystem(directory, L"*", 1, FileSystemEnumerationFlags::kEnumerateDirectories, m_StackAllocator, [this, &directory, &directoriesToSearch](WIN32_FIND_DATAW& findData)
			{
				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					directoriesToSearch.push_back(PathUtils::CombinePaths(directory, findData.cFileName));
			});
		}

		// Second, enumerate directory using our filters
		EnumerateFileSystem(directory, m_SearchInstructions.searchPattern.c_str(), m_SearchInstructions.searchPattern.length(), fileSystemEnumerationFlags, m_StackAllocator, [this, &directory, &directoriesToSearch](WIN32_FIND_DATAW& findData)
		{
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				OnDirectoryFound(directory, findData);
			}
			else
			{
				OnFileFound(directory, findData);
			}
		});
	}
}

void FileSearcher::OnDirectoryFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData)
{
	if (!m_SearchInstructions.SearchForDirectories())
		return;

	SearchInFileName(directory, findData.cFileName, m_SearchInstructions.SearchInDirectoryPath());
}

void FileSearcher::OnFileFound(const std::wstring& directory, const WIN32_FIND_DATAW& findData)
{
	if (!m_SearchInstructions.SearchForFiles())
		return;

	uint64_t fileSize = (static_cast<uint64_t>(findData.nFileSizeHigh) << 32) + findData.nFileSizeLow;
	if (fileSize > m_SearchInstructions.ignoreFilesLargerThan)
		return;

	if (m_SearchInstructions.SearchInFilePath())
	{
		if (SearchInFileName(directory, findData.cFileName, true))
			return;
	}
	else if (m_SearchInstructions.SearchInFileName())
	{
		if (SearchInFileName(directory, findData.cFileName, false))
			return;
	}


}

bool FileSearcher::SearchInFileName(const std::wstring& directory, const wchar_t* fileName, bool searchInPath)
{
	auto fileNameLength = wcslen(fileName);

	if (searchInPath)
	{
		size_t pathLength;
		auto path = PathUtils::CombinePathsTemporary(directory, fileName, fileNameLength, m_StackAllocator, pathLength);
		if (SearchForString(path, pathLength))
		{
			m_SearchInstructions.onFoundPath(path);
			return true;
		}
	}
	else
	{
		if (SearchForString(fileName, fileNameLength))
		{
			auto path = PathUtils::CombinePathsTemporary(directory, fileName, fileNameLength, m_StackAllocator);
			m_SearchInstructions.onFoundPath(path);
			return true;
		}
	}

	return false;
}

bool FileSearcher::SearchForString(const wchar_t* str, size_t length)
{
	if (m_SearchInstructions.IgnoreCase())
	{
		if (m_SearchStringIsAscii)
		{
			auto memory = m_StackAllocator.Allocate(sizeof(wchar_t) * length);
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

/*
void FileSearcher::GoThroughFiles(wstring path)
{	
	WIN32_FIND_DATAW findData;
	wstring pattern;

	if (path[path.size() - 1] != '\\')
	{
		path += L"\\";
	}

	pattern = path + searchPattern;

	auto findHandle = FindFirstFile(pattern.c_str(), &findData);

	if (findHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		wstring filename = findData.cFileName;

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (searchForFolders)
			{
				if ((searchInFolderName && filename.find(searchString) != string::npos) || (searchInFolderPath && path.find(searchString) != string::npos))
				{
					FoundPath(&(path + filename)[0]);
				}
			}

			if (findData.cFileName != wstring(L".") && findData.cFileName != wstring(L".."))
			{
				GoThroughFiles(path + findData.cFileName);
			}
		}
		else if (searchForFiles && findData.nFileSizeLow < 64 * 1024 * 1024 && findData.nFileSizeHigh == 0)
		{
			if ((searchInFileName && filename.find(searchString) != string::npos) || (searchInFilePath && path.find(searchString) != string::npos))
			{
				FoundPath(&(path + filename)[0]);
				continue;
			}

			if (!searchInFileContents)
				continue;

			struct SearchContext
			{
				FileSearcher* __this;
				wstring path;
			};

			AddRef();
			QueueUserWorkItem([](void* ctx) -> DWORD
			{
				auto context = static_cast<SearchContext*>(ctx);
				context->__this->FindSearchString(context->path);
				context->__this->Release();
				delete context;
				return 0;
			}, new SearchContext { this, path + filename }, WT_EXECUTELONGFUNCTION);
		}
	} while (FindNextFile(findHandle, &findData) != FALSE);

	FindClose(findHandle);
}

void FileSearcher::FindSearchString(wstring path)
{
	wifstream in(path.c_str(), ios::binary);
	wstring buffer;
	int fileSize;

	if (!in.is_open())
	{
		return;
	}

	in.seekg(0, ios::end);
	fileSize = (int)in.tellg();
	in.seekg(ios::beg);

	buffer.resize(fileSize);
	in.read(&buffer[0], fileSize);
	in.close();

	if (buffer.find(searchString) != string::npos)
	{
		FoundPath(&path[0]);
	}
}*/

void FileSearcher::Search(SearchInstructions&& searchInstructions)
{
	auto searcher = new FileSearcher(std::forward<SearchInstructions>(searchInstructions));
	
	searcher->Search();
	searcher->Release();
}