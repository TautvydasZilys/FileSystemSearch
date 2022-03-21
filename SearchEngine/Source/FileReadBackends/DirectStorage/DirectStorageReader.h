#pragma once

#include "Event.h"
#include "FileContentSearchData.h"
#include "HandleHolder.h"
#include "ReadBatch.h"
#include "Utilities/IndexStableRingBuffer.h"
#include "Utilities/ObjectPool.h"
#include "Utilities/WorkQueue.h"

struct SearchInstructions;
class SearchResultReporter;
class StringSearcher;

class DirectStorageReader : WorkQueue<FileReadData>, ThreadedWorkQueue<DirectStorageReader, SlotSearchData>
{
public:
    DirectStorageReader(const StringSearcher& stringSearcher, const SearchInstructions& searchInstructions, SearchResultReporter& searchResultReporter);
    ~DirectStorageReader();

    void Initialize();
    void DrainWorkQueue();
    void CompleteAllWork();

    inline void ScanFile(FileOpenData fileOpenData)
    {
        m_FileOpenWorkQueue.PushWorkItem(std::move(fileOpenData));
    }

    static constexpr size_t kTargetTotalBufferSize = 512 * 1024 * 1024; // 512 MB total
    static constexpr size_t kFileReadBufferBaseSize = 128 * 1024;
    static constexpr uint16_t kFileReadSlotCount = kTargetTotalBufferSize / kFileReadBufferBaseSize;

private:
    typedef WorkQueue<FileReadData> MyFileReadBase;
    typedef ThreadedWorkQueue<DirectStorageReader, SlotSearchData> MySearchResultBase;
    friend class MyFileReadBase;
    friend class MySearchResultBase;

private:
    void FileOpenThread();
    void FileReadThread();
    void QueueFileReads();
    void SubmitReadRequests();
    void ProcessReadCompletion();
    void ContentsSearchThread();
    void ProcessSearchCompletion(SlotSearchData searchData);

private:
    SearchResultReporter& m_SearchResultReporter;
    const StringSearcher& m_StringSearcher;
    ThreadedWorkQueue<DirectStorageReader, FileOpenData> m_FileOpenWorkQueue;
    ThreadedWorkQueue<DirectStorageReader, SlotSearchData> m_SearchWorkQueue;
    size_t m_ReadBufferSize;
    std::vector<FileReadStateData> m_FilesToRead; // TO DO: ring buffer
    IndexStableRingBuffer<FileReadStateData, uint32_t> m_FilesWithReadProgress; // TO DO: ring buffer
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
    std::atomic<bool> m_IsTerminating;

    std::unique_ptr<uint8_t[]> m_FileReadBuffers;
    uint64_t m_FreeReadSlots[kFileReadSlotCount / 64];
    uint32_t m_FileReadSlots[kFileReadSlotCount];
};