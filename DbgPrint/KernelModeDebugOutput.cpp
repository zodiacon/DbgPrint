#include "pch.h"
#include "KernelModeDebugOutput.h"

#pragma comment(lib, "tdh")

bool KernelModeDebugOutput::Run(IDebugOutput* sink) {
    _sink = sink;

	if (_handle || _hTrace)
		return true;

	auto sessionName = KERNEL_LOGGER_NAME;

	// {6990501B-4484-4EF0-8793-84159B8D4728}
	static const GUID dummyGuid =
	{ 0x6990501b, 0x4484, 0x4ef0, { 0x87, 0x93, 0x84, 0x15, 0x9b, 0x8d, 0x47, 0x28 } };

	auto size = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(KERNEL_LOGGER_NAME);
	_propertiesBuffer = std::make_unique<BYTE[]>(size);
	bool isWin8Plus = ::IsWindows8OrGreater();
	ULONG error;

	for (;;) {
		::memset(_propertiesBuffer.get(), 0, size);

		_properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(_propertiesBuffer.get());
		_properties->EnableFlags = EVENT_TRACE_FLAG_DBGPRINT;
		_properties->Wnode.BufferSize = (ULONG)size;
		_properties->Wnode.Guid = isWin8Plus ? dummyGuid : SystemTraceControlGuid;
		_properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
		_properties->Wnode.ClientContext = 1;
		_properties->FlushTimer = 1;
		_properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_USE_LOCAL_SEQUENCE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
		_properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		error = ::StartTrace(&_handle, sessionName, _properties);
		if (error == ERROR_ALREADY_EXISTS) {
			error = ::ControlTrace(_hTrace, KERNEL_LOGGER_NAME, _properties, EVENT_TRACE_CONTROL_STOP);
			if (error != ERROR_SUCCESS)
				return false;
			continue;
		}
		break;
	}
	if (error != ERROR_SUCCESS)
		return false;

	_traceLog.Context = this;
	_traceLog.LoggerName = (PWSTR)KERNEL_LOGGER_NAME;
	_traceLog.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
	_traceLog.EventRecordCallback = [](PEVENT_RECORD record) {
		if (record->EventHeader.ProcessId != 0xffffffff) {
			auto text = (PCSTR)((BYTE*)record->UserData + sizeof(ULONG) * 2);
			ATLASSERT(strlen(text) < 512);
			((KernelModeDebugOutput*)record->UserContext)->_sink->DebugOutput(
				record->EventHeader.ProcessId,
				text,
				(FILETIME&)record->EventHeader.TimeStamp.QuadPart,
				DebugOutputFlags::Kernel);
		}
	};
	_hTrace = ::OpenTrace(&_traceLog);
	if (!_hTrace)
		return false;

	// create a dedicated thread to process the trace
	_hThread.reset(::CreateThread(nullptr, 0, [](auto param) {
		return ((KernelModeDebugOutput*)param)->Process();
		}, this, 0, nullptr));
	::SetThreadPriority(_hThread.get(), THREAD_PRIORITY_HIGHEST);
	return true;
}

bool KernelModeDebugOutput::Stop() {
	if (_handle) {
		::ControlTrace(_hTrace, KERNEL_LOGGER_NAME, _properties, EVENT_TRACE_CONTROL_STOP);
		_handle = 0;
	}
	if (_hTrace) {
		::CloseTrace(_hTrace);
		_hTrace = 0;
	}
	_hThread.reset();
	return true;
}

bool KernelModeDebugOutput::IsRunning() const {
    return _hTrace != 0;
}

DWORD KernelModeDebugOutput::Process() {
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	::ProcessTrace(&_hTrace, 1, &now, nullptr);
	return 0;
}
