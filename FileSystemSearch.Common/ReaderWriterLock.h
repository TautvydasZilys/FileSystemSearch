#pragma once

#include "NonCopyable.h"

class ReaderWriterLock : NonCopyable
{
public:
	inline ReaderWriterLock()
	{
		InitializeSRWLock(&m_Lock);
	}

	struct ReaderLock : NonCopyable
	{
	private:
		ReaderWriterLock& m_Lock;

	public:
		inline ReaderLock(ReaderWriterLock& lock) :
			m_Lock(lock)
		{
			m_Lock.AcquireShared();
		}

		inline ~ReaderLock()
		{
			m_Lock.ReleaseShared();
		}
	};

	struct WriterLock : NonCopyable
	{
	private:
		ReaderWriterLock& m_Lock;

	public:
		inline WriterLock(ReaderWriterLock& lock) :
			m_Lock(lock)
		{
			m_Lock.AcquireExclusive();
		}

		inline ~WriterLock()
		{
			m_Lock.ReleaseExclusive();
		}
	};

private:
	inline void AcquireExclusive()
	{
		AcquireSRWLockExclusive(&m_Lock);
	}

	inline void AcquireShared()
	{
		AcquireSRWLockShared(&m_Lock);
	}

	inline void ReleaseExclusive()
	{
		ReleaseSRWLockExclusive(&m_Lock);
	}

	inline void ReleaseShared()
	{
		ReleaseSRWLockShared(&m_Lock);
	}

	SRWLOCK m_Lock;
};

