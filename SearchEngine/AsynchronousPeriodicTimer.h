#pragma once

#include "HandleHolder.h"
#include "SearchInstructions.h"

template <typename Callback>
class AsynchronousPeriodicTimer
{
private:
	HandleHolder m_TimerThread;
	Callback m_Callback;
	uint32_t m_Interval;
	volatile bool m_Done;

	inline void TimerLoop()
	{
		while (!m_Done)
		{
			m_Callback();
			Sleep(m_Interval);
		}
	}

public:
	inline AsynchronousPeriodicTimer(Callback&& callback, uint32_t intervalInMilliseconds) :
		m_Callback(std::forward<Callback>(callback)),
		m_Done(true),
		m_Interval(intervalInMilliseconds)
	{
	}

	AsynchronousPeriodicTimer(const AsynchronousPeriodicTimer& other) = delete;
	AsynchronousPeriodicTimer& operator=(const AsynchronousPeriodicTimer& other) = delete;

	inline AsynchronousPeriodicTimer(AsynchronousPeriodicTimer&& other) :
		m_TimerThread(std::move(other.m_TimerThread)),
		m_Callback(std::move(other.m_Callback)),
		m_Interval(other.m_Interval),
		m_Done(other.m_Done)
	{
	}

	inline AsynchronousPeriodicTimer& operator=(AsynchronousPeriodicTimer&& other)
	{
		m_TimerThread = std::move(other.m_TimerThread);
		m_Callback = std::move(other.m_Callback);
		m_Interval = other.m_Interval;
		m_Done = other.m_Done;
		return *this;
	}

	inline ~AsynchronousPeriodicTimer()
	{
		Assert(m_TimerThread == INVALID_HANDLE_VALUE);
	}

	inline void Start()
	{
		if (m_TimerThread != INVALID_HANDLE_VALUE)
			__fastfail(1);

		m_Done = false;
		m_TimerThread = CreateThread(nullptr, 64 * 1024, [](void* ctx) -> DWORD
		{
			static_cast<AsynchronousPeriodicTimer<Callback>*>(ctx)->TimerLoop();
			return 0;
		}, this, 0, nullptr);
	}

	inline void Stop()
	{
		if (m_TimerThread == INVALID_HANDLE_VALUE)
			__fastfail(1);

		m_Done = true;

		auto waitResult = WaitForSingleObject(m_TimerThread, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);

		m_TimerThread = INVALID_HANDLE_VALUE;
	}
};

template <typename Callback>
inline AsynchronousPeriodicTimer<Callback> MakePeriodicTimer(uint32_t intervalInMilliseconds, Callback&& callback)
{
	return AsynchronousPeriodicTimer<Callback>(std::forward<Callback>(callback), intervalInMilliseconds);
}