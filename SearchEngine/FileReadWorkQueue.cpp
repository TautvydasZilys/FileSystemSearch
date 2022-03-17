#include "PrecompiledHeader.h"
#include "DirectXContext.h"
#include "FileReadWorkQueue.h"

static void NTAPI OnFileReadComplete(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WAIT wait, TP_WAIT_RESULT waitResult);

FileReadWorkQueue::FileReadWorkQueue(size_t searchStringLength) :
    MyBase(*this),
    m_ReadBufferSize(kFileReadBufferBaseSize + searchStringLength * sizeof(wchar_t)),
    m_FileReadBuffers(new uint8_t[m_ReadBufferSize * kFileReadSlotCount]),
    m_WaitableTimer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)),
    m_FenceValue(0)
{
    memset(m_FreeReadSlots, 0xFF, sizeof(m_FreeReadSlots));

    auto d3d12Device = DirectXContext::GetD3D12Device();

    auto hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(m_Fence), &m_Fence);
    Assert(SUCCEEDED(hr));

    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;

    hr = DirectXContext::GetDStorageFactory()->CreateQueue(&queueDesc, __uuidof(m_DStorageQueue), &m_DStorageQueue);
    Assert(SUCCEEDED(hr));
}

FileReadWorkQueue::~FileReadWorkQueue()
{
    Cleanup();
}

void FileReadWorkQueue::Initialize()
{
    MyBase::Initialize<&FileReadWorkQueue::InitializeFileReadThread>(1);
}

void FileReadWorkQueue::Cleanup()
{
    if (m_DStorageQueue != nullptr)
    {
        m_DStorageQueue->Close();
        m_DStorageQueue = nullptr;
    }

    MyBase::Cleanup();
}

void FileReadWorkQueue::InitializeFileReadThread(FileReadWorkQueue&)
{
    DoWork([this](FileReadData& searchData)
    {
        m_FilesToRead.push_back(std::move(searchData));
        QueueFileReads();
    });
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
        uint32_t fileIndex = std::numeric_limits<uint32_t>::max();

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

        const auto fileCount = m_FilesToRead.size();
        for (uint32_t i = 0; i < fileCount; i++)
        {
            if (m_FilesToRead[i].chunksRead < GetChunkCount(m_FilesToRead[i]))
            {
                fileIndex = i;
                break;
            }
        }

        if (fileIndex == std::numeric_limits<uint32_t>::max())
            return;

        auto& file = m_FilesToRead[fileIndex];
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
    m_CurrentBatch.fenceValue = m_FenceValue++;

    m_DStorageQueue->EnqueueSignal(m_Fence.Get(), m_CurrentBatch.fenceValue);
    m_DStorageQueue->Submit();

    if (m_SubmittedBatches.empty())
        m_Fence->SetEventOnCompletion(m_CurrentBatch.fenceValue, m_FenceEvent);

    m_SubmittedBatches.push_back(std::move(m_CurrentBatch));
    m_CurrentBatch = m_BatchPool.GetNewObject();
}

void FileReadWorkQueue::WaitForWork(HANDLE workSemaphore)
{
    while (true)
    {
        uint32_t handleCount = 0;
        HANDLE waitHandles[3];

        waitHandles[handleCount++] = workSemaphore;

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

        auto waitResult = WaitForMultipleObjects(handleCount, waitHandles, FALSE, INFINITE);
        Assert(waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + handleCount);

        if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + handleCount)
        {
            switch (waitResult - WAIT_OBJECT_0)
            {
            case 0: // we got more work
                return;

            case 1: // No requests for 1 ms, submit outstanding work
                SubmitReadRequests();
                break;

            case 2:
                ProcessReadCompletion();
                break;
            }
        }
    }
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
            auto& file = m_FilesToRead[fileIndex];

            Assert(file.readsInProgress > 0);
            file.readsInProgress--;
            m_FreeReadSlots[slot / 64] |= 1ULL << (slot % 64);

            // auto buffer = m_FileReadBuffers.get() + slot * m_ReadBufferSize;
        }
    }

    m_SubmittedBatches.erase(m_SubmittedBatches.begin(), m_SubmittedBatches.begin() + batchIndex);
    if (!m_SubmittedBatches.empty())
        m_Fence->SetEventOnCompletion(m_SubmittedBatches.front().fenceValue, m_FenceEvent);

    while (!m_FilesToRead.empty())
    {
        auto& lastFile = m_FilesToRead.back();
        
        if (lastFile.chunksRead == GetChunkCount(lastFile) && lastFile.readsInProgress == 0)
        {
            m_FilesToRead.pop_back();
        }
        else
        {
            break;
        }
    }

    QueueFileReads();
}
