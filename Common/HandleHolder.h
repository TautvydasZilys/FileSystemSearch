#pragma once

struct HandleHolder
{
private:
	HANDLE m_Handle;

public:
	inline HandleHolder() :
		m_Handle(INVALID_HANDLE_VALUE)
	{
	}

	inline HandleHolder(HANDLE handle) :
		m_Handle(handle)
	{
	}

	inline ~HandleHolder()
	{
		if (m_Handle != INVALID_HANDLE_VALUE)
			CloseHandle(m_Handle);
	}

	inline operator HANDLE() const
	{
		return m_Handle;
	}

	HandleHolder(const HandleHolder&) = delete;
	HandleHolder& operator=(const HandleHolder&) = delete;

	HandleHolder(HandleHolder&& other) :
		m_Handle(other)
	{
		other.m_Handle = INVALID_HANDLE_VALUE;
	}

	HandleHolder& operator=(HandleHolder&& other)
	{
		std::swap(m_Handle, other.m_Handle);
		return *this;
	}

	HandleHolder& operator=(HANDLE handle)
	{
		if (m_Handle != INVALID_HANDLE_VALUE)
			CloseHandle(m_Handle);
		
		m_Handle = handle;
		return *this;
	}
};