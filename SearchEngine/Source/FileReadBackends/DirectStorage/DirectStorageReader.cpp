#include "PrecompiledHeader.h"
#include "DirectStorageReader.h"
#include "DirectXContext.h"
#include "SearchInstructions.h"
#include "SearchResultReporter.h"
#include "StringSearch/StringSearcher.h"
#include "Utilities/ScopedStackAllocator.h"

DirectStorageReader::DirectStorageReader(const StringSearcher& stringSearcher, const SearchInstructions& searchInstructions, SearchResultReporter& searchResultReporter) :
    m_SearchResultReporter(searchResultReporter),
    m_StringSearcher(stringSearcher),
    m_ReadBufferSize(0),
    m_FenceEvent(false),
    m_FenceValue(0),
    m_IsTerminating(false),
    m_FreeReadSlotCount(ARRAYSIZE(m_FileReadSlots))
{
    if (searchInstructions.SearchInFileContents())
        m_ReadBufferSize = kFileReadBufferBaseSize + std::max(searchInstructions.utf8SearchString.length(), searchInstructions.searchString.length() * sizeof(wchar_t));
}

DirectStorageReader::~DirectStorageReader()
{
    if (m_DStorageQueue != nullptr)
    {
        m_DStorageQueue->Close();
        m_DStorageQueue = nullptr;
    }
}

void DirectStorageReader::Initialize()
{
    m_FileReadBuffers.reset(new uint8_t[m_ReadBufferSize * kFileReadSlotCount]);
    m_WaitableTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    memset(m_FreeReadSlots, 0xFF, sizeof(m_FreeReadSlots));

    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;

    auto hr = DirectXContext::GetDStorageFactory()->CreateQueue(&queueDesc, __uuidof(m_DStorageQueue), &m_DStorageQueue);
    Assert(SUCCEEDED(hr));

    hr = DirectXContext::GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(m_Fence), &m_Fence);
    Assert(SUCCEEDED(hr));

    m_FenceEvent.Initialize();

    SYSTEM_INFO systemInfo;
    GetNativeSystemInfo(&systemInfo);

    MyFileReadBase::Initialize();
    m_FileOpenWorkQueue.Initialize<&DirectStorageReader::FileOpenThread>(this, systemInfo.dwNumberOfProcessors - 2);
    m_SearchWorkQueue.Initialize<&DirectStorageReader::ContentsSearchThread>(this, systemInfo.dwNumberOfProcessors - 1);
    MySearchResultBase::Initialize<&DirectStorageReader::FileReadThread>(this, 1);
}

void DirectStorageReader::DrainWorkQueue()
{
    m_IsTerminating = true;
    m_FileOpenWorkQueue.DrainWorkQueue();
    MyFileReadBase::DrainWorkQueue();
    m_SearchWorkQueue.DrainWorkQueue();
    MySearchResultBase::DrainWorkQueue();
}

void DirectStorageReader::CompleteAllWork()
{
    m_FileOpenWorkQueue.CompleteAllWork();

    ReleaseSemaphore(MyFileReadBase::GetWorkSemaphore(), 1, nullptr);
    WaitForSingleObject(m_FileReadsCompletedEvent, INFINITE);

    m_SearchWorkQueue.CompleteAllWork();
    MySearchResultBase::CompleteAllWork();
}

void DirectStorageReader::FileOpenThread()
{
    SetThreadDescription(GetCurrentThread(), L"FSS DirectStorage File Open Thread");

    m_FileOpenWorkQueue.DoWork([this](FileOpenData& searchData)
    {
        if (m_IsTerminating)
            return;

        DirectStorageFileReadData readData(std::move(searchData));
        auto hr = DirectXContext::GetDStorageFactory()->OpenFile(readData.filePath.c_str(), __uuidof(readData.file), &readData.file);
        if (FAILED(hr))
        {
            m_SearchResultReporter.AddToScannedFileCount();
            m_SearchResultReporter.AddToScannedFileSize(readData.fileSize);
            return;
        }

        MyFileReadBase::PushWorkItem(std::move(readData));
    });
}

void DirectStorageReader::FileReadThread()
{
    bool readSubmissionCompleted = false;
    bool fileContentSearchCompleted = false;
    SetThreadDescription(GetCurrentThread(), L"FSS DirectStorage Read Submission Thread");

    for (;;)
    {
        uint32_t handleCount = 0;
        HANDLE waitHandles[4];

        if (!readSubmissionCompleted)
            waitHandles[handleCount++] = MyFileReadBase::GetWorkSemaphore();

        if (!fileContentSearchCompleted)
            waitHandles[handleCount++] = MySearchResultBase::GetWorkSemaphore();

        if (!m_CurrentBatch.slots.empty())
        {
            LARGE_INTEGER timer;
            timer.QuadPart = -10000; // 1 ms
            auto result = SetWaitableTimer(m_WaitableTimer, &timer, 0, nullptr, nullptr, FALSE);
            Assert(result);

            waitHandles[handleCount++] = m_WaitableTimer;
        }

        if (!m_SubmittedBatches.empty())
            waitHandles[handleCount++] = m_FenceEvent;

        if (handleCount == 0)
            break;

        auto waitResult = WaitForMultipleObjects(handleCount, waitHandles, FALSE, INFINITE);
        Assert(waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + handleCount);

        if (waitResult < WAIT_OBJECT_0 || waitResult >= WAIT_OBJECT_0 + handleCount)
            break;

        if (m_IsTerminating)
        {
            DrainWorkQueue();
            break;
        }

        auto signaledHandle = waitHandles[waitResult - WAIT_OBJECT_0];
        if (signaledHandle == MyFileReadBase::GetWorkSemaphore())
        {
            auto workEntry = MyFileReadBase::PopWorkEntry();
            if (workEntry != nullptr)
            {
                m_FilesToRead.push_back(std::move(workEntry->workItem));
                QueueFileReads();

                MyFileReadBase::DeleteWorkEntry(workEntry);
            }
            else
            {
                readSubmissionCompleted = true;
            }
        }
        else if (signaledHandle == MySearchResultBase::GetWorkSemaphore())
        {
            auto workEntry = MySearchResultBase::PopWorkEntry();
            if (workEntry != nullptr)
            {
                ProcessSearchCompletion(workEntry->workItem);
                MySearchResultBase::DeleteWorkEntry(workEntry);
            }
            else
            {
                fileContentSearchCompleted = true;
            }

            if (readSubmissionCompleted && m_CurrentBatch.slots.empty() && m_FreeReadSlotCount == ARRAYSIZE(m_FileReadSlots))
                m_FileReadsCompletedEvent.Set();
        }
        else if (signaledHandle == m_WaitableTimer)
        {
            // No requests for 1 ms, submit outstanding work
            SubmitReadRequests();
        }
        else
        {
            ProcessReadCompletion();
        }
    }
}

static uint32_t GetChunkCount(const DirectStorageFileReadStateData& file)
{
    const auto fileSize = file.fileSize;
    auto chunkCount = file.fileSize / DirectStorageReader::kFileReadBufferBaseSize;
    if (file.fileSize % DirectStorageReader::kFileReadBufferBaseSize)
        chunkCount++;

    Assert(chunkCount < std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(chunkCount);
}

void DirectStorageReader::QueueFileReads()
{
    while (true)
    {
        uint16_t slot = std::numeric_limits<uint16_t>::max();

        // TO DO: use a trie if this is too slow
        for (uint16_t i = 0; i < ARRAYSIZE(m_FreeReadSlots); i++)
        {
            DWORD index;
            if (_BitScanForward64(&index, m_FreeReadSlots[i]))
            {
                slot = 64 * i + static_cast<uint16_t>(index);
                break;
            }
        }

        if (slot == std::numeric_limits<uint16_t>::max())
            return;

        uint32_t fileIndex;
        if (m_FilesWithReadProgress.IsEmpty() || m_FilesWithReadProgress.Back().chunksRead == GetChunkCount(m_FilesWithReadProgress.Back()))
        {
            if (m_FilesToRead.empty())
                return;

            fileIndex = m_FilesWithReadProgress.PushBack(std::move(m_FilesToRead.back()));
            m_FilesToRead.pop_back();
        }
        else
        {
            fileIndex = m_FilesWithReadProgress.BackIndex();
        }

        auto& file = m_FilesWithReadProgress.Back();
        file.readsInProgress++;

        Assert(slot < kFileReadSlotCount);
        m_FileReadSlots[slot] = fileIndex;
        m_FreeReadSlots[slot / 64] &= ~(1ULL << (slot % 64));
        m_FreeReadSlotCount--;

        const auto fileOffset = (file.chunksRead++) * kFileReadBufferBaseSize;
        uint32_t bytesToRead = static_cast<uint32_t>(std::min(file.fileSize - fileOffset, m_ReadBufferSize));

        DSTORAGE_REQUEST request = {};
        request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
        request.Source.File.Source = file.file.Get();
        request.Source.File.Offset = fileOffset;
        request.Source.File.Size = bytesToRead;
        request.Destination.Memory.Buffer = m_FileReadBuffers.get() + slot * m_ReadBufferSize;
        request.Destination.Memory.Size = bytesToRead;
        request.UncompressedSize = bytesToRead;

        m_DStorageQueue->EnqueueRequest(&request);
        m_CurrentBatch.slots.emplace_back(slot, bytesToRead);

        if (m_CurrentBatch.slots.size() >= ARRAYSIZE(m_FileReadSlots) / 2)
            SubmitReadRequests();
    }
}

void DirectStorageReader::SubmitReadRequests()
{
    Assert(m_CurrentBatch.slots.size() > 0);
    m_CurrentBatch.fenceValue = ++m_FenceValue;

    m_DStorageQueue->EnqueueSignal(m_Fence.Get(), m_CurrentBatch.fenceValue);
    m_DStorageQueue->Submit();

    if (m_SubmittedBatches.empty())
        m_Fence->SetEventOnCompletion(m_CurrentBatch.fenceValue, m_FenceEvent);

    m_SubmittedBatches.push_back(std::move(m_CurrentBatch));
    m_CurrentBatch = m_BatchPool.GetNewObject();
}

void DirectStorageReader::ProcessReadCompletion()
{
    uint32_t batchIndex = 0;
    for (; batchIndex < m_SubmittedBatches.size() && m_SubmittedBatches[batchIndex].fenceValue <= m_Fence->GetCompletedValue(); batchIndex++)
    {
        auto& batch = m_SubmittedBatches[batchIndex];

        for (auto slot : batch.slots)
            m_SearchWorkQueue.PushWorkItem(slot);

        batch.slots.clear();
        m_BatchPool.PoolObject(std::move(batch));
    }

    m_SubmittedBatches.erase(m_SubmittedBatches.begin(), m_SubmittedBatches.begin() + batchIndex);
    if (!m_SubmittedBatches.empty())
        m_Fence->SetEventOnCompletion(m_SubmittedBatches.front().fenceValue, m_FenceEvent);
}

void DirectStorageReader::ContentsSearchThread()
{
    ScopedStackAllocator allocator;
    SetThreadDescription(GetCurrentThread(), L"FSS Content Search Thread");

    m_SearchWorkQueue.DoWork([this, &allocator](SlotSearchData& searchData)
    {
        searchData.found = m_StringSearcher.PerformFileContentSearch(m_FileReadBuffers.get() + searchData.slot * m_ReadBufferSize, searchData.size, allocator);
        MySearchResultBase::PushWorkItem(searchData);
    });
}

void DirectStorageReader::ProcessSearchCompletion(SlotSearchData searchData)
{
    auto& fileIndex = m_FileReadSlots[searchData.slot];
    auto& file = m_FilesWithReadProgress[fileIndex];

    Assert(file.readsInProgress > 0);
    file.readsInProgress--;
    m_FreeReadSlots[searchData.slot / 64] |= 1ULL << (searchData.slot % 64);
    m_FreeReadSlotCount++;

    if (!file.dispatchedToResults)
    {
        if (searchData.found)
        {
            m_SearchResultReporter.AddToScannedFileCount();
            m_SearchResultReporter.AddToScannedFileSize(file.fileSize - file.totalScannedSize);
            m_SearchResultReporter.DispatchSearchResult(file.fileFindData, std::move(file.filePath));

            file.totalScannedSize = file.fileSize;
            file.chunksRead = GetChunkCount(file);
            file.dispatchedToResults = true;
        }
        else if (file.readsInProgress == 0 && file.chunksRead == GetChunkCount(file))
        {
            m_SearchResultReporter.AddToScannedFileCount();
            m_SearchResultReporter.AddToScannedFileSize(file.fileSize - file.totalScannedSize);
            file.totalScannedSize = file.fileSize;
        }
        else
        {
            m_SearchResultReporter.AddToScannedFileSize(kFileReadBufferBaseSize);
            file.totalScannedSize += kFileReadBufferBaseSize;
        }
    }

    while (!m_FilesWithReadProgress.IsEmpty())
    {
        auto& firstFile = m_FilesWithReadProgress.Front();

        if (firstFile.chunksRead == GetChunkCount(firstFile) && firstFile.readsInProgress == 0)
        {
            m_FilesWithReadProgress.PopFront();
        }
        else
        {
            break;
        }
    }

    QueueFileReads();
}
