#include "PrecompiledHeader.h"
#include "SearchResultReporter.h"
#include "Utilities/PathUtils.h"

SearchResultReporter::SearchResultReporter(const SearchInstructions& searchInstructions) :
	m_FoundPathCallback(searchInstructions.onFoundPath),
    m_ProgressCallback(searchInstructions.onProgressUpdated),
    m_DoneCallback(searchInstructions.onDone),
	m_CallbackContext(searchInstructions.callbackContext)
{
	ZeroMemory(&m_SearchStatistics, sizeof(m_SearchStatistics));

	QueryPerformanceCounter(&m_SearchStart);
	QueryPerformanceFrequency(&m_PerformanceFrequency);

	Initialize<&SearchResultReporter::InitializeSearchResultDispatcherWorkerThread>(this, 1);
}

void SearchResultReporter::ReportProgress(bool finishedScanningFileSystem)
{
	// This is bad: we're reading m_SearchStatistics which is constantly being updated from other threads. 
	// This read isn't atomic, that means we might read the statistics that didn't actually exist as a whole at any given
	// HOWEVER, I believe it still is a better alternative than protecting it with a critical section, as that would just utterly destroy performance
	// Some glitches in the statistics should be okay, as you can't really tell just by looking at them if they're correct or not
	auto statisticsSnapshot = m_SearchStatistics;

	double progress = finishedScanningFileSystem
		? static_cast<double>(statisticsSnapshot.scannedFileSize) / static_cast<double>(statisticsSnapshot.totalFileSize)
		: sqrt(-1); // indeterminate

	statisticsSnapshot.searchTimeInSeconds = GetTotalSearchTimeInSeconds();
	m_ProgressCallback(m_CallbackContext, statisticsSnapshot, progress);
}

void SearchResultReporter::FinishSearch()
{
	CompleteAllWork();

    m_SearchStatistics.searchTimeInSeconds = GetTotalSearchTimeInSeconds();
    m_DoneCallback(m_CallbackContext, m_SearchStatistics);
}

double SearchResultReporter::GetTotalSearchTimeInSeconds()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	return static_cast<double>(currentTime.QuadPart - m_SearchStart.QuadPart) / static_cast<double>(m_PerformanceFrequency.QuadPart);
}

void SearchResultReporter::InitializeSearchResultDispatcherWorkerThread()
{
	SetThreadDescription(GetCurrentThread(), L"FSS Result Dispatcher Thread");

	DoWork([this](const SearchResultData& searchResult)
	{
		auto win32FindData = searchResult.resultFindData.ToWin32FindData(PathUtils::GetFileName(searchResult.resultPath));
		m_FoundPathCallback(m_CallbackContext, win32FindData, searchResult.resultPath.c_str());
	});
}

void SearchResultReporter::DispatchSearchResult(const FileFindData& findData, std::wstring&& path)
{
    InterlockedIncrement(&m_SearchStatistics.resultsFound);
	PushWorkItem(std::forward<std::wstring>(path), findData);
}
