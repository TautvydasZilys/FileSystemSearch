#pragma once

#include "HandleHolder.h"

template <typename Owner, typename WorkItem>
class WorkQueue
{
private:
	Owner& m_Owner;
	uint32_t m_WorkerThreadCount;
	HandleHolder m_WorkSemaphore;
	SLIST_HEADER* m_WorkList;
	std::vector<HandleHolder> m_WorkerThreadHandles;

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
	inline WorkQueue(Owner& owner) :
		m_Owner(owner),
		m_WorkerThreadCount(0),
		m_WorkList(nullptr)
	{
	}

	inline ~WorkQueue()
	{
		Cleanup();
	}

	template <void (Owner::*WorkerThreadInitializer)(WorkQueue<Owner, WorkItem>& workQueue)>
	inline void Initialize(uint32_t workerThreadCount)
	{
		Assert(workerThreadCount > 0);
		m_WorkerThreadCount = workerThreadCount;

		m_WorkSemaphore = CreateSemaphoreW(nullptr, 0, std::numeric_limits<LONG>::max(), nullptr);
		Assert(m_WorkSemaphore != nullptr);

		m_WorkList = static_cast<SLIST_HEADER*>(_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT));
		InitializeSListHead(m_WorkList);

		m_WorkerThreadHandles.reserve(m_WorkerThreadCount);
		for (uint32_t i = 0; i < m_WorkerThreadCount; i++)
		{
			auto threadHandle = CreateThread(nullptr, 64 * 1024, [](void* context) -> DWORD
			{
				auto _this = static_cast<WorkQueue<Owner, WorkItem>*>(context);
				(_this->m_Owner.*WorkerThreadInitializer)(*_this);
				return 0;
			}, this, 0, nullptr);

			auto setThreadPriorityResult = SetThreadPriority(threadHandle, THREAD_PRIORITY_BELOW_NORMAL);
			Assert(setThreadPriorityResult != FALSE);

			m_WorkerThreadHandles.push_back(threadHandle);
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

	inline void DrainWorkQueue()
	{
		if (m_WorkerThreadCount == 0)
			return;

		for (;;)
		{
			auto workEntry = PopWorkEntry();
			if (workEntry == nullptr)
				break;

			DeleteWorkEntry(workEntry);
		}
	}

	inline void Cleanup()
	{
		if (m_WorkerThreadCount == 0)	
			return;

		auto releaseResult = ReleaseSemaphore(m_WorkSemaphore, m_WorkerThreadCount, nullptr);
		Assert(releaseResult != FALSE);

		auto waitResult = WaitForMultipleObjects(m_WorkerThreadCount, reinterpret_cast<const HANDLE*>(m_WorkerThreadHandles.data()), TRUE, INFINITE);
		Assert(waitResult >= WAIT_OBJECT_0 && waitResult <= WAIT_OBJECT_0 + m_WorkerThreadCount - 1);

		Assert(QueryDepthSList(m_WorkList) == 0); // If this fires, something went very very wrong
		_aligned_free(m_WorkList);

		m_WorkerThreadCount = 0;
	}
};

