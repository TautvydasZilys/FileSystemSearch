#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

#include <Windows.h>

using namespace std;

wofstream out;
mutex outMutex;
mutex memoryUsageMutex;

long long int memoryUsage = 0;
long long int totalFileCount = 0;
vector<thread> threads;
wstring lastPath;

void GoThroughFiles(wstring path = L".");
void FindAsk(wstring fileName, int fileSizeInBytes);

int main()
{
	ios::sync_with_stdio(false);
	unique_ptr<wchar_t[]> pathBuffer(new wchar_t[10240]);
	_wgetcwd(pathBuffer.get(), 10240);

	GoThroughFiles(pathBuffer.get());

	for_each (threads.begin(), threads.end(), [](thread& t)
	{
		t.join();
	});
	
	out.close();
	return 0;
}

void GoThroughFiles(wstring path)
{
	WIN32_FIND_DATAW findData;
	wstring pattern;
	int success = true;

	if (threads.size() > 1000)
	{
		for_each (threads.begin(), threads.end(), [](thread& t)
		{
			t.join();
		});	
		threads.clear();
	}

	if (path[path.size() - 1] != '\\')
	{
		path += L"\\";
	}

	pattern = path + L"*.*";

	auto findHandle = FindFirstFile(pattern.c_str(), &findData);

	if (findHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if (success == 0)
		{
			continue;
		}

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (findData.cFileName != wstring(L".") && findData.cFileName != wstring(L".."))
			{
				GoThroughFiles(path + findData.cFileName);
			}
		}
		else if (findData.nFileSizeLow < 256 * 1024 * 1024 && findData.nFileSizeHigh == 0)
		{
			totalFileCount++;

			memoryUsageMutex.lock();
			bool startThread = memoryUsage < 512 * 1024 * 1024;
			memoryUsage += findData.nFileSizeLow;
			memoryUsageMutex.unlock();

			if (!startThread)
			{
				try
				{				
					FindAsk(path + findData.cFileName, findData.nFileSizeLow < 256 * 1024 * 1024);
				}
				catch (exception e)
				{
					coutMutex.lock();
					cout << e.what() << endl;
					coutMutex.unlock();
				}
			}
			else
			{
				threads.emplace_back([](wstring path, int fileSizeInBytes)
					{
						try
						{
							FindAsk(path, fileSizeInBytes);
						}
						catch (exception e)
						{
							coutMutex.lock();
							cout << e.what() << endl;
							coutMutex.unlock();
						}
					},
					path + findData.cFileName, findData.nFileSizeLow);
			}
		}
	}
	while ((success = FindNextFile(findHandle, &findData)) != 0 || GetLastError() != ERROR_NO_MORE_FILES);
}

void FindAsk(wstring path, int fileSizeInBytes)
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

	if (buffer.find(L"search.ask") != string::npos || path.find(L"search.ask") != string::npos)
	{
		outMutex.lock();
		out << path << endl;
		out.flush();
		outMutex.unlock();
	}
	
	memoryUsageMutex.lock();
	memoryUsage -= fileSizeInBytes;
	memoryUsageMutex.unlock();
}