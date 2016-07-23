#pragma once

class WindowClassHolder
{
private:
	ATOM m_WindowClass;

public:
	WindowClassHolder() :
		m_WindowClass(0)
	{
	}

	WindowClassHolder(ATOM windowClass) :
		m_WindowClass(windowClass)
	{
	}

	~WindowClassHolder()
	{
		if (m_WindowClass != 0)
			UnregisterClassW(*this, GetModuleHandleW(nullptr));
	}

	WindowClassHolder& operator=(ATOM windowClass)
	{
		this->~WindowClassHolder();
		m_WindowClass = windowClass;
		return *this;
	}

	WindowClassHolder(const WindowClassHolder&) = delete;
	WindowClassHolder& operator=(const WindowClassHolder&) = delete;

	inline operator ATOM() const
	{
		return m_WindowClass;
	}

	inline operator const wchar_t*() const
	{
		return reinterpret_cast<const wchar_t*>(m_WindowClass);
	}
};