#pragma once

#include "DebugOutputBase.h"
#include <wil\resource.h>

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
	void Report(PendingOutput const& output, std::vector<ULONG64> stack = {});

	IDebugOutput* m_sink;
	TRACEHANDLE m_handle{ 0 };
	TRACEHANDLE m_hTrace{ 0 };
	EVENT_TRACE_PROPERTIES* m_properties;
	std::unique_ptr<BYTE[]> m_propertiesBuffer;
	EVENT_TRACE_LOGFILE m_traceLog = { 0 };
	wil::unique_handle m_hThread, m_hStop;
	PCWSTR m_SessionName{ KERNEL_LOGGER_NAME };
	bool m_CaptureStacks{ false };
	std::unordered_map<DWORD, PendingOutput> m_Pending;	// keyed by thread ID
};
