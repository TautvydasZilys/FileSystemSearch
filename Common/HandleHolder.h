#pragma once
#include "NonCopyable.h"

template <HANDLE InvalidValue>
struct HandleHolder : NonCopyable
{
private:
	HANDLE m_Handle;

public:
	inline HandleHolder() :
		m_Handle(InvalidValue)
	{
	}

	inline HandleHolder(HANDLE handle) :
		m_Handle(handle)
	{
	}

	HandleHolder(HandleHolder&& other) :
		m_Handle(other)
	{
		other.m_Handle = InvalidValue;
	}

	inline ~HandleHolder()
	{
		if (m_Handle != InvalidValue)
			CloseHandle(m_Handle);
	}

	HandleHolder& operator=(HandleHolder&& other)
	{
		std::swap(m_Handle, other.m_Handle);
		return *this;
	}

	inline operator HANDLE() const
	{
		return m_Handle;
	}

	inline operator bool() const
	{
		return m_Handle != InvalidValue;
	}

	inline bool operator==(const HandleHolder& other) const
	{
		return m_Handle == other.m_Handle;
	}

	inline bool operator==(HANDLE other) const
	{
		return m_Handle == other;
	}

	inline HANDLE* operator&()
	{
		if (m_Handle != InvalidValue)
		{
			CloseHandle(m_Handle);
			m_Handle = InvalidValue;
		}

		return &m_Handle;
	}

	HandleHolder& operator=(HANDLE handle)
	{
		if (m_Handle != InvalidValue)
			CloseHandle(m_Handle);
		
		m_Handle = handle;
		return *this;
	}
};

using EventHandleHolder = HandleHolder<nullptr>;
using FileHandleHolder = HandleHolder<INVALID_HANDLE_VALUE>;
using SemaphoreHandleHolder = HandleHolder<nullptr>;
using ThreadHandleHolder = HandleHolder<nullptr>;
using TimerHandleHolder = HandleHolder<nullptr>;