#pragma once

#include "DebugOutputBase.h"
#include <wil\resource.h>
#include <mutex>

class KernelModeDebugOutput : public DebugOutputBase {
public:
	~KernelModeDebugOutput();

	// takes effect the next time the session starts
	void SetCaptureStacks(bool capture);

	// Inherited via DebugOutputBase
	bool Run(IDebugOutput* sink) override;
	bool Stop() override;
	bool IsRunning() const override;

protected:
	DWORD Process();

private:
	struct PendingOutput {
		DWORD ProcessId;
		std::string Text;
		FILETIME Time;
		ULONGLONG Received;
	};

	void OnEvent(PEVENT_RECORD record);
	void OnDebugPrint(PEVENT_RECORD record);
	void OnStackWalk(PEVENT_RECORD record);
	void FlushPending(bool all);
	void FlushPendingLocked(bool all);
	void StartFlushTimer();
	void StopFlushTimer();
	void Report(PendingOutput const& output, std::vector<ULONG64> stack = {});

	IDebugOutput* m_sink;
	TRACEHANDLE m_handle{ 0 };
	TRACEHANDLE m_hTrace{ 0 };
	EVENT_TRACE_PROPERTIES* m_properties;
	std::unique_ptr<BYTE[]> m_propertiesBuffer;
	EVENT_TRACE_LOGFILE m_traceLog = { 0 };
	wil::unique_handle m_hThread, m_hStop;
	std::wstring m_SessionName{ KERNEL_LOGGER_NAME };
	bool m_CaptureStacks{ false };
	// messages waiting for their stack walk event, keyed by thread ID; used by the processing thread and the flush timer
	std::unordered_map<DWORD, PendingOutput> m_Pending;
	std::mutex m_PendingLock;
	PTP_TIMER m_FlushTimer{ nullptr };
};
