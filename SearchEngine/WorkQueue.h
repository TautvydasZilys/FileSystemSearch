#pragma once

#include "HandleHolder.h"
#include "NonCopyable.h"

template <typename TOwner, typename TWorkQueue>
class WorkQueueOwnerStorage
{
public:
	WorkQueueOwnerStorage(TOwner& owner) : 
		m_Owner(owner)
	{
	}

	inline TOwner& GetOwner()
	{
		return m_Owner;
	}

private:
	TOwner& m_Owner;
};

template <typename TOwnerIsWorkQueue>
class WorkQueueOwnerStorage<TOwnerIsWorkQueue, TOwnerIsWorkQueue>
{
public:
	WorkQueueOwnerStorage(TOwnerIsWorkQueue&)
	{
	}

	inline TOwnerIsWorkQueue& GetOwner() 
	{
		static_assert(std::is_base_of_v<WorkQueueOwnerStorage<TOwnerIsWorkQueue, TOwnerIsWorkQueue>, TOwnerIsWorkQueue>, "TOwnerIsWorkQueue type must derive from WorkQueueBase!");
		return *static_cast<TOwnerIsWorkQueue*>(this);
	}
};

template <typename Owner, typename WorkItem, typename TWorkQueue>
class __declspec(empty_bases) WorkQueueBase : public WorkQueueOwnerStorage<Owner, TWorkQueue>, public NonCopyable
{
private:
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
	inline WorkQueueBase(Owner& owner) :
		WorkQueueOwnerStorage<Owner, TWorkQueue>(owner),
		m_WorkList(nullptr)
	{
		static_assert(std::is_base_of_v<WorkQueueBase<Owner, WorkItem, TWorkQueue>, TWorkQueue>, "WorkQueue type must derive from WorkQueueBase!");
	}

	inline ~WorkQueueBase()
	{
		static_cast<TWorkQueue*>(this)->GetOwner().Cleanup();
	}

	template <void (Owner::* WorkerThreadInitializer)(TWorkQueue& workQueue)>
	inline void Initialize(uint32_t workerThreadCount)
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
				auto _this = static_cast<TWorkQueue*>(context);
				(_this->GetOwner().*WorkerThreadInitializer)(*_this);
				return 0;
			}, this, 0, nullptr);
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
			static_cast<TWorkQueue*>(this)->WaitForWork(m_WorkSemaphore);

			auto workEntry = PopWorkEntry();
			if (workEntry == nullptr)
				break;

			callback(workEntry->workItem);
			DeleteWorkEntry(workEntry);
		}
	}

	inline void Cleanup()
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

		auto workerThreadCount = static_cast<DWORD>(m_WorkerThreadHandles.size());
		auto releaseResult = ReleaseSemaphore(m_WorkSemaphore, workerThreadCount, nullptr);
		Assert(releaseResult != FALSE);

		auto waitResult = WaitForMultipleObjects(workerThreadCount, reinterpret_cast<const HANDLE*>(m_WorkerThreadHandles.data()), TRUE, INFINITE);
		Assert(waitResult >= WAIT_OBJECT_0 && waitResult <= WAIT_OBJECT_0 + workerThreadCount - 1);

		Assert(QueryDepthSList(m_WorkList) == 0); // If this fires, something went very very wrong
		_aligned_free(m_WorkList);

		m_WorkerThreadHandles = HeapArray<ThreadHandleHolder>();
	}
};

template <typename Owner, typename WorkItem>
class WorkQueue : public WorkQueueBase<Owner, WorkItem, WorkQueue<Owner, WorkItem>>
{
public:
	inline WorkQueue(Owner& owner) :
		WorkQueueBase<Owner, WorkItem, WorkQueue<Owner, WorkItem>>(owner)
	{
	}

private:
	inline void WaitForWork(HANDLE workSemaphore)
	{
		auto waitResult = WaitForSingleObject(workSemaphore, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);
	}

	friend class WorkQueueBase<Owner, WorkItem, WorkQueue<Owner, WorkItem>>;
};

