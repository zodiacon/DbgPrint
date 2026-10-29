#include "pch.h"
#include "KernelModeDebugOutput.h"

#pragma comment(lib, "tdh")

namespace {
	// private system logger session name (Windows 8+)
	const WCHAR SessionName[] = L"DbgPrintKernelSession";

	// kernel event classes (see wmicore.mof)
	// {13976D09-A327-438C-950B-7F03192815C7}
	const GUID DebugPrintGuid =
	{ 0x13976d09, 0xa327, 0x438c, { 0x95, 0x0b, 0x7f, 0x03, 0x19, 0x28, 0x15, 0xc7 } };
	// {DEF2FE46-7BD6-4B80-BD94-F57FE20D0CE3}
	const GUID StackWalkGuid =
	{ 0xdef2fe46, 0x7bd6, 0x4b80, { 0xbd, 0x94, 0xf5, 0x7f, 0xe2, 0x0d, 0x0c, 0xe3 } };

	const UCHAR DebugPrintOpcode = 32;
	const UCHAR StackWalkOpcode = 32;

	// how long a message waits for its stack walk event before being reported without a stack
	const ULONGLONG PendingTimeout = 1000;

	// how long Stop waits for the processing thread to drain the remaining events
	const DWORD StopTimeout = 5000;
}

KernelModeDebugOutput::~KernelModeDebugOutput() {
	Stop();
}

void KernelModeDebugOutput::SetCaptureStacks(bool capture) {
	m_CaptureStacks = capture;
}

bool KernelModeDebugOutput::Run(IDebugOutput* sink) {
	if (m_handle || m_hTrace) {
		m_sink = sink;
		return true;
	}

	//
	// a processing thread that did not exit in time during Stop still uses this object
	//
	if (m_hThread) {
		if (::WaitForSingleObject(m_hThread.get(), 0) == WAIT_TIMEOUT)
			return false;
		m_hThread.reset();
		FlushPending(true);
	}

	m_sink = sink;

	//
	// Windows 8+ supports multiple system loggers, so use a private session
	// rather than taking over the global "NT Kernel Logger" other tools may be using
	//
	bool isWin8Plus = ::IsWindows8OrGreater();
	m_SessionName = isWin8Plus ? SessionName : KERNEL_LOGGER_NAME;

	// {6990501B-4484-4EF0-8793-84159B8D4728}
	static const GUID dummyGuid =
	{ 0x6990501b, 0x4484, 0x4ef0, { 0x87, 0x93, 0x84, 0x15, 0x9b, 0x8d, 0x47, 0x28 } };

	auto size = sizeof(EVENT_TRACE_PROPERTIES) + (wcslen(m_SessionName) + 1) * sizeof(WCHAR);
	m_propertiesBuffer = std::make_unique<BYTE[]>(size);
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

		error = ::StartTrace(&m_handle, m_SessionName, m_properties);
		if (error == ERROR_ALREADY_EXISTS) {
			//
			// most likely a session left behind by a previous instance
			//
			error = ::ControlTrace(0, m_SessionName, m_properties, EVENT_TRACE_CONTROL_STOP);
			if (error != ERROR_SUCCESS)
				return false;
			continue;
		}
		break;
	}
	if (error != ERROR_SUCCESS)
		return false;

	if (m_CaptureStacks) {
		//
		// failure is not fatal - messages are still captured, just without stacks
		//
		CLASSIC_EVENT_ID id{ DebugPrintGuid, DebugPrintOpcode };
		::TraceSetInformation(m_handle, TraceStackTracingInfo, &id, sizeof(id));
	}

	m_traceLog.Context = this;
	m_traceLog.LoggerName = const_cast<PWSTR>(m_SessionName);
	m_traceLog.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
	m_traceLog.EventRecordCallback = [](PEVENT_RECORD record) {
		((KernelModeDebugOutput*)record->UserContext)->OnEvent(record);
	};
	m_traceLog.BufferCallback = [](PEVENT_TRACE_LOGFILE log) -> ULONG {
		((KernelModeDebugOutput*)log->Context)->FlushPending(false);
		return TRUE;
	};
	m_hTrace = ::OpenTrace(&m_traceLog);
	if (m_hTrace == INVALID_PROCESSTRACE_HANDLE) {
		m_hTrace = 0;
		Stop();
		return false;
	}

	// create a dedicated thread to process the trace
	m_hThread.reset(::CreateThread(nullptr, 0, [](auto param) {
		return ((KernelModeDebugOutput*)param)->Process();
		}, this, 0, nullptr));
	if (!m_hThread) {
		Stop();
		return false;
	}
	::SetThreadPriority(m_hThread.get(), THREAD_PRIORITY_HIGHEST);
	return true;
}

bool KernelModeDebugOutput::Stop() {
	if (m_handle) {
		::ControlTrace(m_handle, m_SessionName, m_properties, EVENT_TRACE_CONTROL_STOP);
		m_handle = 0;
	}
	if (m_hTrace) {
		::CloseTrace(m_hTrace);
		m_hTrace = 0;
	}
	if (m_hThread) {
		//
		// ProcessTrace returns once the remaining buffers have been delivered
		//
		if (::WaitForSingleObject(m_hThread.get(), StopTimeout) == WAIT_TIMEOUT) {
			//
			// keep the handle so Run does not start a second consumer while this one drains
			//
			ATLTRACE(L"Kernel trace processing thread did not exit in time\n");
			return false;
		}
		m_hThread.reset();
	}

	//
	// the processing thread is gone, so the pending messages can be flushed safely
	//
	FlushPending(true);
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

void KernelModeDebugOutput::OnEvent(PEVENT_RECORD record) {
	auto& header = record->EventHeader;
	if (header.ProviderId == DebugPrintGuid && header.EventDescriptor.Opcode == DebugPrintOpcode)
		OnDebugPrint(record);
	else if (header.ProviderId == StackWalkGuid && header.EventDescriptor.Opcode == StackWalkOpcode)
		OnStackWalk(record);
}

void KernelModeDebugOutput::OnDebugPrint(PEVENT_RECORD record) {
	//
	// layout: ULONG ComponentId, ULONG Level, null-terminated ANSI string
	//
	const ULONG offset = sizeof(ULONG) * 2;
	if (record->UserDataLength <= offset)
		return;

	auto& header = record->EventHeader;
	auto text = (PCSTR)((BYTE*)record->UserData + offset);
	PendingOutput output{
		header.ProcessId,
		std::string(text, strnlen(text, record->UserDataLength - offset)),
		(FILETIME&)header.TimeStamp.QuadPart,
		::GetTickCount64()
	};

	if (!m_CaptureStacks) {
		Report(output);
		return;
	}

	//
	// hold the message until its stack walk event (logged right after it on the same thread) arrives
	//
	if (auto it = m_Pending.find(header.ThreadId); it != m_Pending.end()) {
		Report(it->second);
		m_Pending.erase(it);
	}
	m_Pending.insert({ header.ThreadId, std::move(output) });
	FlushPending(false);
}

void KernelModeDebugOutput::OnStackWalk(PEVENT_RECORD record) {
	//
	// layout: ULONG64 EventTimeStamp, ULONG StackProcess, ULONG StackThread, pointer-sized frames
	//
	const ULONG offset = sizeof(ULONG64) + sizeof(ULONG) * 2;
	if (record->UserDataLength <= offset)
		return;

	auto data = (BYTE*)record->UserData;
	auto tid = *(ULONG*)(data + sizeof(ULONG64) + sizeof(ULONG));
	auto it = m_Pending.find(tid);
	if (it == m_Pending.end())
		return;

	bool is32Bit = (record->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) == EVENT_HEADER_FLAG_32_BIT_HEADER;
	ULONG pointerSize = is32Bit ? sizeof(ULONG) : sizeof(ULONG64);
	auto count = (record->UserDataLength - offset) / pointerSize;

	std::vector<ULONG64> stack;
	stack.reserve(count);
	for (ULONG i = 0; i < count; i++)
		stack.push_back(is32Bit ? ((ULONG*)(data + offset))[i] : ((ULONG64*)(data + offset))[i]);

	Report(it->second, std::move(stack));
	m_Pending.erase(it);
}

void KernelModeDebugOutput::FlushPending(bool all) {
	auto now = ::GetTickCount64();
	for (auto it = m_Pending.begin(); it != m_Pending.end(); ) {
		if (all || now - it->second.Received > PendingTimeout) {
			Report(it->second);
			it = m_Pending.erase(it);
		}
		else {
			++it;
		}
	}
}

void KernelModeDebugOutput::Report(PendingOutput const& output, std::vector<ULONG64> stack) {
	m_sink->DebugOutput(output.ProcessId, output.Text.c_str(), output.Time, DebugOutputFlags::Kernel, std::move(stack));
}
