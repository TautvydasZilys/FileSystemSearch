#pragma once

#include "HandleHolder.h"
#include "HeapArray.h"
#include "NonCopyable.h"

template <typename Owner, typename WorkItem>
class WorkQueue : public NonCopyable
{
protected:
	SemaphoreHandleHolder m_WorkSemaphore;
	SLIST_HEADER* m_WorkList;
	HeapArray<ThreadHandleHolder> m_WorkerThreadHandles;

	struct WorkEntry
	{
		SLIST_ENTRY slistEntry;
		WorkItem workItem;
	};

	inline WorkEntry* PopWorkEntry()
	{
		return reinterpret_cast<WorkEntry*>(InterlockedPopEntrySList(m_WorkList));
	}

	inline void DeleteWorkEntry(WorkEntry* workEntry)
	{
		workEntry->workItem.~WorkItem();
		_aligned_free(workEntry);
	}

public:
	inline WorkQueue() :
		m_WorkList(nullptr)
	{
	}

	inline ~WorkQueue()
	{
		CompleteAllWork();
	}

	template <void (Owner::* WorkerThreadInitializer)()>
	inline void Initialize(Owner* owner, uint32_t workerThreadCount)
	{
		Assert(workerThreadCount > 0);

		m_WorkSemaphore = CreateSemaphoreW(nullptr, 0, std::numeric_limits<LONG>::max(), nullptr);
		Assert(m_WorkSemaphore != nullptr);

		m_WorkList = static_cast<SLIST_HEADER*>(_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT));
		InitializeSListHead(m_WorkList);

		m_WorkerThreadHandles = HeapArray<ThreadHandleHolder>(workerThreadCount);
		for (uint32_t i = 0; i < workerThreadCount; i++)
		{
			auto threadHandle = CreateThread(nullptr, 64 * 1024, [](void* context) -> DWORD
			{
				auto owner = static_cast<Owner*>(context);
				(owner->*WorkerThreadInitializer)();
				return 0;
			}, owner, 0, nullptr);
			Assert(threadHandle != nullptr);

			auto setThreadPriorityResult = SetThreadPriority(threadHandle, THREAD_PRIORITY_BELOW_NORMAL);
			Assert(setThreadPriorityResult != FALSE);

			m_WorkerThreadHandles[i] = threadHandle;
		}
	}

	template <typename... WorkItemConstructionArgs>
	inline void PushWorkItem(WorkItemConstructionArgs&&... workItemConstructionArgs)
	{
		WorkEntry* workEntry = static_cast<WorkEntry*>(_aligned_malloc(sizeof(WorkEntry), MEMORY_ALLOCATION_ALIGNMENT));
		workEntry->slistEntry.Next = nullptr;
		new (&workEntry->workItem) WorkItem(std::forward<WorkItemConstructionArgs>(workItemConstructionArgs)...);

		InterlockedPushEntrySList(m_WorkList, &workEntry->slistEntry);

		auto releaseResult = ReleaseSemaphore(m_WorkSemaphore, 1, nullptr);
		Assert(releaseResult != FALSE);
	}

	template <typename Callback>
	inline void DoWork(Callback&& callback)
	{
		for (;;)
		{
			auto waitResult = WaitForSingleObject(m_WorkSemaphore, INFINITE);
			Assert(waitResult == WAIT_OBJECT_0);

			auto workEntry = PopWorkEntry();
			if (workEntry == nullptr)
				break;

			callback(workEntry->workItem);
			DeleteWorkEntry(workEntry);
		}
	}

	inline void CompleteAllWork()
	{
		if (m_WorkerThreadHandles.empty())
			return;

		auto workerThreadCount = static_cast<DWORD>(m_WorkerThreadHandles.size());
		auto releaseResult = ReleaseSemaphore(m_WorkSemaphore, workerThreadCount, nullptr);
		Assert(releaseResult != FALSE);

		auto waitResult = WaitForMultipleObjects(workerThreadCount, reinterpret_cast<const HANDLE*>(m_WorkerThreadHandles.data()), TRUE, INFINITE);
		Assert(waitResult >= WAIT_OBJECT_0 && waitResult <= WAIT_OBJECT_0 + workerThreadCount - 1);

		// If our threads due to their queues being drained, there is a slight chance that work producers
		// keep producing work for a little bit longer. By the time we get to CompleteAllWork, all producers
		// will have been halted but they could have added work before this method got called.
		DrainWorkQueue();

		Assert(QueryDepthSList(m_WorkList) == 0); // If this fires, something went very very wrong
		_aligned_free(m_WorkList);

		m_WorkerThreadHandles = HeapArray<ThreadHandleHolder>();
	}

	inline void DrainWorkQueue()
	{
		if (m_WorkerThreadHandles.empty())
			return;

		for (;;)
		{
			auto workEntry = PopWorkEntry();
			if (workEntry == nullptr)
				break;

			DeleteWorkEntry(workEntry);
		}
	}
};
