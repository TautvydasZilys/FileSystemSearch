#pragma once

#include "NonCopyable.h"

class CriticalSection : NonCopyable
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

	inline ~CriticalSection()
	{
		DeleteCriticalSection(&m_CriticalSection);
	}

	struct Lock : NonCopyable
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
	};
};

