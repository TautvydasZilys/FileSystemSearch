#pragma once

typedef void (__stdcall *ProgressUpdated)(double progress);

class ProgressReporter
{
private:
	ProgressUpdated m_OnProgressUpdated;
	HandleHolder m_ReportingThread;
	double lastProgress;
	volatile int64_t* m_CurrentValue;
	volatile int64_t* m_TotalValue;
	volatile bool m_DoneReporting;

	inline void ReportLoop()
	{
		const uint32_t kReportIntervalMS = 10;

		while (!m_DoneReporting)
		{
			ReportProgress();
			Sleep(kReportIntervalMS);
		}
	}

	inline void ReportProgress()
	{
		auto currentProgress = static_cast<double>(*m_CurrentValue) / static_cast<double>(*m_TotalValue);

		// Report in 0.1% increments at the very least
		if (currentProgress - lastProgress > 0.001)
		{
			m_OnProgressUpdated(currentProgress);
			lastProgress = currentProgress;
		}
	}

public:
	inline ProgressReporter(ProgressUpdated onProgressUpdated, volatile int64_t* currentValue, int64_t* totalValue) :
		m_OnProgressUpdated(onProgressUpdated),
		m_DoneReporting(true),
		m_CurrentValue(currentValue),
		m_TotalValue(totalValue)
	{
	}

	inline void StartReporting()
	{
		if (m_ReportingThread != INVALID_HANDLE_VALUE)
			__fastfail(1);

		lastProgress = 0.0;
		m_DoneReporting = false;
		m_ReportingThread = CreateThread(nullptr, 64 * 1024, [](void* ctx) -> DWORD
		{
			static_cast<ProgressReporter*>(ctx)->ReportLoop();
			return 0;
		}, this, 0, nullptr);
	}

	inline void StopReporting()
	{
		if (m_ReportingThread == INVALID_HANDLE_VALUE)
			__fastfail(1);

		m_DoneReporting = true;

		auto waitResult = WaitForSingleObject(m_ReportingThread, INFINITE);
		Assert(waitResult == WAIT_OBJECT_0);

		m_ReportingThread = INVALID_HANDLE_VALUE;
	}
};