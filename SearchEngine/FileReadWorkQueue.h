#pragma once

#include "Event.h"
#include "FileContentSearchData.h"
#include "HandleHolder.h"
#include "HeapArray.h"
#include "ObjectPool.h"
#include "ReadBatch.h"
#include "WorkQueue.h"

struct SearchInstructions;
class SearchResultReporter;
class StringSearcher;

class FileReadWorkQueue : WorkQueue<FileReadData>, ThreadedWorkQueue<FileReadWorkQueue, SlotSearchData>
{
public:
    FileReadWorkQueue(const StringSearcher& stringSearcher, const SearchInstructions& searchInstructions, SearchResultReporter& searchResultReporter);
    ~FileReadWorkQueue();

    void Initialize();
    void DrainWorkQueue();
    void CompleteAllWork();

    static constexpr uint16_t kFileReadSlotCount = DSTORAGE_MAX_QUEUE_CAPACITY;
    static constexpr size_t kFileReadBufferBaseSize = 512 * 1024 * 1024 / kFileReadSlotCount; // 512 MB total

private:
    typedef WorkQueue<FileReadData> MyFileReadBase;
    typedef ThreadedWorkQueue<FileReadWorkQueue, SlotSearchData> MySearchResultBase;
    friend class MyFileReadBase;
    friend class MySearchResultBase;

public:
    using MyFileReadBase::PushWorkItem;

private:
    void FileReadThread();
    void QueueFileReads();
    void SubmitReadRequests();
    void ProcessReadCompletion();
    void ContentsSearchThread();
    void ProcessSearchCompletion(SlotSearchData searchData);

private:
    SearchResultReporter& m_SearchResultReporter;
    const StringSearcher& m_StringSearcher;
    ThreadedWorkQueue<FileReadWorkQueue, uint16_t> m_SearchWorkQueue;
    size_t m_ReadBufferSize;
    std::vector<FileReadStateData> m_FilesToRead; // TO DO: ring buffer
    std::vector<FileReadStateData> m_FilesWithReadProgress; // TO DO: ring buffer
    std::vector<ReadBatch> m_SubmittedBatches; // TO DO: ring buffer
    ObjectPool<ReadBatch> m_BatchPool;
    ReadBatch m_CurrentBatch;
    Event<EventType::ManualReset> m_FileReadsCompletedEvent;

    ComPtr<IDStorageQueue> m_DStorageQueue;
    ComPtr<ID3D12Fence> m_Fence;
    Event<EventType::AutoReset> m_FenceEvent;
    uint64_t m_FenceValue;

    TimerHandleHolder m_WaitableTimer;
    uint16_t m_FreeReadSlotCount;
    bool m_IsTerminating;

    std::unique_ptr<uint8_t[]> m_FileReadBuffers;
    uint64_t m_FreeReadSlots[kFileReadSlotCount / 64];
    uint32_t m_FileReadSlots[kFileReadSlotCount];
};