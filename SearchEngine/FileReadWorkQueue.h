#pragma once

#include "Event.h"
#include "FileContentSearchData.h"
#include "HandleHolder.h"
#include "HeapArray.h"
#include "ObjectPool.h"
#include "ReadBatch.h"
#include "ScopedStackAllocator.h"
#include "WorkQueue.h"

struct SearchInstructions;
class SearchResultReporter;
class StringSearcher;

class FileReadWorkQueue : public WorkQueue<FileReadWorkQueue, FileReadData>
{
public:
    FileReadWorkQueue(const StringSearcher& stringSearcher, const SearchInstructions& searchInstructions, SearchResultReporter& searchResultReporter);
    ~FileReadWorkQueue();

    void Initialize();
    void DrainWorkQueue();

    static constexpr uint16_t kFileReadSlotCount = DSTORAGE_MAX_QUEUE_CAPACITY;
    static constexpr size_t kFileReadBufferBaseSize = 512 * 1024 * 1024 / kFileReadSlotCount; // 512 MB total

private:
    typedef WorkQueue<FileReadWorkQueue, FileReadData> MyBase;
    friend class MyBase;

private:
    void FileReadThread();
    void QueueFileReads();
    void SubmitReadRequests();
    void ProcessReadCompletion();

private:
    SearchResultReporter& m_SearchResultReporter;
    const StringSearcher& m_StringSearcher;
    ScopedStackAllocator m_StackAllocator;
    size_t m_ReadBufferSize;
    std::vector<FileReadStateData> m_FilesToRead; // TO DO: ring buffer
    std::vector<FileReadStateData> m_FilesWithReadProgress; // TO DO: ring buffer
    std::vector<ReadBatch> m_SubmittedBatches; // TO DO: ring buffer
    ObjectPool<ReadBatch> m_BatchPool;
    ReadBatch m_CurrentBatch;

    ComPtr<IDStorageQueue> m_DStorageQueue;
    ComPtr<ID3D12Fence> m_Fence;
    Event<EventType::AutoReset> m_FenceEvent;
    uint64_t m_FenceValue;

    TimerHandleHolder m_WaitableTimer;
    bool m_IsTerminating;

    std::unique_ptr<uint8_t[]> m_FileReadBuffers;
    uint64_t m_FreeReadSlots[kFileReadSlotCount / 64];
    uint32_t m_FileReadSlots[kFileReadSlotCount];
};