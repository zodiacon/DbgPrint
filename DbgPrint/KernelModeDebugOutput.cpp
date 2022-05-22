#include "pch.h"
#include "KernelModeDebugOutput.h"

#pragma comment(lib, "tdh")

bool KernelModeDebugOutput::Run(IDebugOutput* sink) {
    m_sink = sink;

	if (m_handle || m_hTrace)
		return true;

	auto sessionName = KERNEL_LOGGER_NAME;

	// {6990501B-4484-4EF0-8793-84159B8D4728}
	static const GUID dummyGuid =
	{ 0x6990501b, 0x4484, 0x4ef0, { 0x87, 0x93, 0x84, 0x15, 0x9b, 0x8d, 0x47, 0x28 } };

	auto size = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(KERNEL_LOGGER_NAME);
	m_propertiesBuffer = std::make_unique<BYTE[]>(size);
	bool isWin8Plus = ::IsWindows8OrGreater();
	ULONG error;

	for (;;) {
		::memset(m_propertiesBuffer.get(), 0, size);

		m_properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(m_propertiesBuffer.get());
		m_properties->EnableFlags = EVENT_TRACE_FLAG_DBGPRINT;
		m_properties->Wnode.BufferSize = (ULONG)size;
		m_properties->Wnode.Guid = isWin8Plus ? dummyGuid : SystemTraceControlGuid;
		m_properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
		m_properties->Wnode.ClientContext = 1;
		m_properties->FlushTimer = 1;
		m_properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_USE_LOCAL_SEQUENCE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
		m_properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		error = ::StartTrace(&m_handle, sessionName, m_properties);
		if (error == ERROR_ALREADY_EXISTS) {
			error = ::ControlTrace(m_hTrace, KERNEL_LOGGER_NAME, m_properties, EVENT_TRACE_CONTROL_STOP);
			if (error != ERROR_SUCCESS)
				return false;
			continue;
		}
		break;
	}
	if (error != ERROR_SUCCESS)
		return false;

	m_traceLog.Context = this;
	m_traceLog.LoggerName = (PWSTR)KERNEL_LOGGER_NAME;
	m_traceLog.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
	m_traceLog.EventRecordCallback = [](PEVENT_RECORD record) {
		if (record->EventHeader.ProcessId != 0xffffffff) {
			auto text((PCSTR)((BYTE*)record->UserData + sizeof(ULONG) * 2));
			ATLASSERT(text);
			((KernelModeDebugOutput*)record->UserContext)->m_sink->DebugOutput(
				record->EventHeader.ProcessId,
				text,
				(FILETIME&)record->EventHeader.TimeStamp.QuadPart,
				DebugOutputFlags::Kernel);
		}
	};
	m_hTrace = ::OpenTrace(&m_traceLog);
	if (!m_hTrace)
		return false;

	// create a dedicated thread to process the trace
	m_hThread.reset(::CreateThread(nullptr, 0, [](auto param) {
		return ((KernelModeDebugOutput*)param)->Process();
		}, this, 0, nullptr));
	::SetThreadPriority(m_hThread.get(), THREAD_PRIORITY_HIGHEST);
	return true;
}

bool KernelModeDebugOutput::Stop() {
	if (m_handle) {
		::ControlTrace(m_hTrace, KERNEL_LOGGER_NAME, m_properties, EVENT_TRACE_CONTROL_STOP);
		m_handle = 0;
	}
	if (m_hTrace) {
		::CloseTrace(m_hTrace);
		m_hTrace = 0;
	}
	m_hThread.reset();
	return true;
}

bool KernelModeDebugOutput::IsRunning() const {
    return m_hTrace != 0;
}

DWORD KernelModeDebugOutput::Process() {
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	::ProcessTrace(&m_hTrace, 1, &now, nullptr);
	return 0;
}
