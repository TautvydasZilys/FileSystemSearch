#include "PrecompiledHeader.h"
#include "DirectXContext.h"
#include "FileReadWorkQueue.h"
#include "SearchInstructions.h"
#include "SearchResultReporter.h"
#include "StringSearcher.h"

FileReadWorkQueue::FileReadWorkQueue(const StringSearcher& stringSearcher, const SearchInstructions& searchInstructions, SearchResultReporter& searchResultReporter) :
    m_SearchResultReporter(searchResultReporter),
    m_StringSearcher(stringSearcher),
    m_ReadBufferSize(0),
    m_FenceEvent(false),
    m_FenceValue(0),
    m_IsTerminating(false)
{
    if (searchInstructions.SearchInFileContents())
        m_ReadBufferSize = kFileReadBufferBaseSize + std::max(searchInstructions.utf8SearchString.length(), searchInstructions.searchString.length() * sizeof(wchar_t));
}

FileReadWorkQueue::~FileReadWorkQueue()
{
    if (m_DStorageQueue != nullptr)
    {
        m_DStorageQueue->Close();
        m_DStorageQueue = nullptr;
    }
}

void FileReadWorkQueue::Initialize()
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

    MyBase::Initialize<&FileReadWorkQueue::FileReadThread>(this, 1);
}

void FileReadWorkQueue::DrainWorkQueue()
{
    m_IsTerminating = true;
    MyBase::DrainWorkQueue();
}

void FileReadWorkQueue::FileReadThread()
{
    bool hasWorkItems = true;
    SetThreadDescription(GetCurrentThread(), L"FileSystemSearch Read Submission Thread");

    for (;;)
    {
        uint32_t handleCount = 0;
        HANDLE waitHandles[3];

        if (hasWorkItems)
            waitHandles[handleCount++] = m_WorkSemaphore;

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
        if (signaledHandle == m_WorkSemaphore)
        {
            auto workEntry = PopWorkEntry();
            if (workEntry != nullptr)
            {
                m_FilesToRead.push_back(std::move(workEntry->workItem));
                QueueFileReads();

                DeleteWorkEntry(workEntry);
            }
            else
            {
                hasWorkItems = false;
            }
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

static uint32_t GetChunkCount(const FileReadStateData& file)
{
    const auto fileSize = file.fileSize;
    auto chunkCount = file.fileSize / FileReadWorkQueue::kFileReadBufferBaseSize;
    if (file.fileSize % FileReadWorkQueue::kFileReadBufferBaseSize)
        chunkCount++;

    Assert(chunkCount < std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(chunkCount);
}

void FileReadWorkQueue::QueueFileReads()
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

        if (m_FilesWithReadProgress.empty() || m_FilesWithReadProgress.back().chunksRead == GetChunkCount(m_FilesWithReadProgress.back()))
        {
            if (m_FilesToRead.empty())
                return;

            m_FilesWithReadProgress.push_back(std::move(m_FilesToRead.back()));
            m_FilesToRead.pop_back();
        }

        uint32_t fileIndex = static_cast<uint32_t>(m_FilesWithReadProgress.size() - 1);
        auto& file = m_FilesWithReadProgress[fileIndex];
        file.readsInProgress++;

        m_FileReadSlots[slot] = fileIndex;
        m_FreeReadSlots[slot / 64] &= ~(1ULL << (slot % 64));

        const auto maxSearchStringLength = 1; // m_SearchInstructions.searchString.length() * sizeof(wchar_t);
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

        memset(request.Destination.Memory.Buffer, 0, m_ReadBufferSize); // TO DO: profile if this is impactful
        m_DStorageQueue->EnqueueRequest(&request);
        m_CurrentBatch.slots.push_back(slot);
    }
}

void FileReadWorkQueue::SubmitReadRequests()
{
    Assert(m_CurrentBatch.slots.size() > 0);
    m_CurrentBatch.fenceValue = m_FenceValue++;

    m_DStorageQueue->EnqueueSignal(m_Fence.Get(), m_CurrentBatch.fenceValue);
    m_DStorageQueue->Submit();

    if (m_SubmittedBatches.empty())
        m_Fence->SetEventOnCompletion(m_CurrentBatch.fenceValue, m_FenceEvent);

    m_SubmittedBatches.push_back(std::move(m_CurrentBatch));
    m_CurrentBatch = m_BatchPool.GetNewObject();
}

void FileReadWorkQueue::ProcessReadCompletion()
{
    uint32_t batchIndex = 0;
    for (; batchIndex < m_SubmittedBatches.size() && m_SubmittedBatches[batchIndex].fenceValue <= m_Fence->GetCompletedValue(); batchIndex++)
    {
        const auto& batch = m_SubmittedBatches[batchIndex];

        for (auto slot : batch.slots)
        {
            auto& fileIndex = m_FileReadSlots[slot];
            auto& file = m_FilesWithReadProgress[fileIndex];

            Assert(file.readsInProgress > 0);
            file.readsInProgress--;
            m_FreeReadSlots[slot / 64] |= 1ULL << (slot % 64);

            if (!file.dispatchedToResults)
            {
                auto buffer = m_FileReadBuffers.get() + slot * m_ReadBufferSize;
                if (m_StringSearcher.PerformFileContentSearch(buffer, static_cast<uint32_t>(m_ReadBufferSize), m_StackAllocator))
                {
                    m_SearchResultReporter.DispatchSearchResult(file.fileFindData, std::move(file.filePath));

                    m_SearchResultReporter.AddToScannedFileSize(file.fileSize - file.totalScannedSize);
                    file.totalScannedSize = file.fileSize;
                    file.chunksRead = GetChunkCount(file);
                    file.dispatchedToResults = true;
                }
                else if (file.readsInProgress == 0 && file.chunksRead == GetChunkCount(file))
                {
                    m_SearchResultReporter.AddToScannedFileSize(file.fileSize - file.totalScannedSize);
                    file.totalScannedSize = file.fileSize;
                }
                else
                {
                    m_SearchResultReporter.AddToScannedFileSize(kFileReadBufferBaseSize);
                    file.totalScannedSize += kFileReadBufferBaseSize;
                }
            }
        }
    }

    m_SubmittedBatches.erase(m_SubmittedBatches.begin(), m_SubmittedBatches.begin() + batchIndex);
    if (!m_SubmittedBatches.empty())
        m_Fence->SetEventOnCompletion(m_SubmittedBatches.front().fenceValue, m_FenceEvent);

    while (!m_FilesWithReadProgress.empty())
    {
        auto& lastFile = m_FilesWithReadProgress.back();
        
        if (lastFile.chunksRead == GetChunkCount(lastFile) && lastFile.readsInProgress == 0)
        {
            m_SearchResultReporter.AddToScannedFileCount();
            m_FilesWithReadProgress.pop_back();
        }
        else
        {
            break;
        }
    }

    QueueFileReads();
}
