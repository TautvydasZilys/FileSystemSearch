#pragma once

#include "HandleHolder.h"
#include "NonCopyable.h"
#include "SearchInstructions.h"

template <typename Callback>
class AsynchronousPeriodicTimer : NonCopyable
{
private:
	ThreadHandleHolder m_TimerThread;
	Callback m_Callback;
	uint32_t m_IntervalMS;
	EventHandleHolder m_StopEvent;
	TimerHandleHolder m_WaitableTimer;

	inline void TimerLoop()
	{
		const HANDLE handles[2] = { m_StopEvent, m_WaitableTimer };
		for (;;)
		{
			DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            Assert(wait == WAIT_OBJECT_0 || wait == WAIT_OBJECT_0 + 1);
			if (wait == WAIT_OBJECT_0)
				break;

			m_Callback();
		}
	}

public:
	inline AsynchronousPeriodicTimer(Callback&& callback, uint32_t intervalInMilliseconds) :
		m_Callback(std::forward<Callback>(callback)),
		m_StopEvent(nullptr),
		m_WaitableTimer(nullptr),
		m_IntervalMS(intervalInMilliseconds)
	{
	}

	inline AsynchronousPeriodicTimer(AsynchronousPeriodicTimer&& other) :
		m_TimerThread(std::move(other.m_TimerThread)),
		m_Callback(std::move(other.m_Callback)),
		m_IntervalMS(other.m_IntervalMS),
		m_StopEvent(std::move(other.m_StopEvent)),
		m_WaitableTimer(std::move(other.m_WaitableTimer))
	{
	}

	inline AsynchronousPeriodicTimer& operator=(AsynchronousPeriodicTimer&& other)
	{
		m_TimerThread = std::move(other.m_TimerThread);
		m_Callback = std::move(other.m_Callback);
		m_IntervalMS = other.m_IntervalMS;
		m_StopEvent = std::move(other.m_StopEvent);
		m_WaitableTimer = std::move(other.m_WaitableTimer);
		return *this;
	}

	inline ~AsynchronousPeriodicTimer()
	{
		Assert(!m_TimerThread);
	}

	inline void Start()
	{
		if (m_TimerThread)
			__fastfail(1);

		m_StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		Assert(m_StopEvent);

		m_WaitableTimer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
		Assert(m_WaitableTimer);

		LARGE_INTEGER dueTime
		{
			.QuadPart = -static_cast<LONGLONG>(m_IntervalMS) * 10000LL // 100-ns units
		};
		auto setTimerResult = SetWaitableTimer(m_WaitableTimer, &dueTime, static_cast<LONG>(m_IntervalMS), nullptr, nullptr, FALSE);
		Assert(setTimerResult);

		m_TimerThread = CreateThread(nullptr, 64 * 1024, [](void* ctx) -> DWORD
		{
			SetThreadDescription(GetCurrentThread(), L"FSS Periodic Timer Thread");
			static_cast<AsynchronousPeriodicTimer<Callback>*>(ctx)->TimerLoop();
			return 0;
		}, this, 0, nullptr);
		Assert(m_TimerThread);
	}

	inline void Stop()
	{
		if (!m_TimerThread)
			__fastfail(1);

		auto setEventResult = SetEvent(m_StopEvent);
		Assert(setEventResult != FALSE);

		auto waitResult = WaitForSingleObject(m_TimerThread, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);

		CancelWaitableTimer(m_WaitableTimer);
		m_WaitableTimer = nullptr;
		m_StopEvent = nullptr;
		m_TimerThread = nullptr;
	}
};

template <typename Callback>
inline AsynchronousPeriodicTimer<Callback> MakePeriodicTimer(uint32_t intervalInMilliseconds, Callback&& callback)
{
	return AsynchronousPeriodicTimer<Callback>(std::forward<Callback>(callback), intervalInMilliseconds);
}