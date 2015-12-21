#pragma once

class CriticalSection
{
private:
	CRITICAL_SECTION m_CriticalSection;

	inline void Enter()
	{
		EnterCriticalSection(&m_CriticalSection);
	}

	inline void Leave()
	{
		LeaveCriticalSection(&m_CriticalSection);
	}

public:
	inline CriticalSection()
	{
		InitializeCriticalSection(&m_CriticalSection);
	}

	CriticalSection(const CriticalSection&) = delete;
	CriticalSection& operator=(const CriticalSection&) = delete;

	inline ~CriticalSection()
	{
		DeleteCriticalSection(&m_CriticalSection);
	}

	struct Lock
	{
	private:
		CriticalSection& m_CriticalSection;

	public:
		inline Lock(CriticalSection& criticalSection) :
			m_CriticalSection(criticalSection)
		{
			m_CriticalSection.Enter();
		}

		inline ~Lock()
		{
			m_CriticalSection.Leave();
		}

		Lock(const Lock&) = delete;
		Lock& operator=(const Lock&) = delete;
	};
};

