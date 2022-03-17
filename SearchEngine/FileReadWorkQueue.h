#pragma once

#include "Event.h"
#include "FileContentSearchData.h"
#include "HandleHolder.h"
#include "HeapArray.h"
#include "ObjectPool.h"
#include "ReadBatch.h"
#include "ScopedStackAllocator.h"
#include "WorkQueue.h"

class FileReadWorkQueue : public WorkQueueBase<FileReadWorkQueue, FileReadData, FileReadWorkQueue>
{
public:
    FileReadWorkQueue(size_t searchStringLength);
    ~FileReadWorkQueue();

    void Initialize();
    void Cleanup();

    static constexpr uint16_t kFileReadSlotCount = DSTORAGE_MAX_QUEUE_CAPACITY;
    static constexpr size_t kFileReadBufferBaseSize = 512 * 1024 * 1024 / kFileReadSlotCount; // 512 MB total

private:
    typedef WorkQueueBase<FileReadWorkQueue, FileReadData, FileReadWorkQueue> MyBase;
    friend class MyBase;

private:
    void InitializeFileReadThread(FileReadWorkQueue& fileReadWorkQueue);
    void QueueFileReads();
    void SubmitReadRequests();
    void WaitForWork(HANDLE workSemaphore);
    void ProcessReadCompletion();

private:
    size_t m_ReadBufferSize;
    ScopedStackAllocator m_StackAllocator;
    std::vector<FileReadStateData> m_FilesToRead; // TO DO: ring buffer
    std::vector<ReadBatch> m_SubmittedBatches; // TO DO: ring buffer
    ObjectPool<ReadBatch> m_BatchPool;
    ReadBatch m_CurrentBatch;

    ComPtr<IDStorageQueue> m_DStorageQueue;
    ComPtr<ID3D12Fence> m_Fence;
    Event<EventType::AutoReset> m_FenceEvent;
    uint64_t m_FenceValue;

    TimerHandleHolder m_WaitableTimer;

    std::unique_ptr<uint8_t[]> m_FileReadBuffers;
    uint64_t m_FreeReadSlots[kFileReadSlotCount / 64];
    uint32_t m_FileReadSlots[kFileReadSlotCount];
};