#include "pch.h"
#include "UserModeDebugOutput.h"

UserModeDebugOutput::UserModeDebugOutput(PCWSTR prefix) : _prefix(prefix) {
}

UserModeDebugOutput::~UserModeDebugOutput() {
	Stop();
}

bool UserModeDebugOutput::Run(IDebugOutput* sink) {
	_sink = sink;
	_hStop.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
	if (!_hStop)
		return false;

	if (!_hBufferReady) {
		_hBufferReady.reset(::CreateEvent(nullptr, FALSE, FALSE, _prefix + L"\\DBWIN_BUFFER_READY"));
		if (!_hBufferReady)
			return false;
	}

	if (!_hDataReady) {
		_hDataReady.reset(::CreateEvent(nullptr, FALSE, FALSE, _prefix + L"\\DBWIN_DATA_READY"));
		if (!_hDataReady)
			return false;
	}

	DWORD size = 1 << 12;	// 4KB
	if (!_hMemFile) {
		_hMemFile.reset(::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, _prefix + L"\\DBWIN_BUFFER"));
		if (_hMemFile == nullptr)
			return false;
	}

	if (!_buffer) {
		_buffer = (PBYTE)::MapViewOfFile(_hMemFile.get(), FILE_MAP_READ, 0, 0, 0);
		if (_buffer == nullptr)
			return false;
	}

	_hThread.reset(::CreateThread(nullptr, 0, [](auto p) {
		return ((UserModeDebugOutput*)p)->DebugListen();
		}, this, 0, nullptr));
	return _hThread != nullptr;
}

bool UserModeDebugOutput::Stop() {
	if (!_hStop)
		return false;

	if (_hThread == nullptr)
		return true;

	auto rv = ::SignalObjectAndWait(_hStop.get(), _hThread.get(), 2000, FALSE);
	ATLASSERT(rv == WAIT_OBJECT_0);
	_hThread.reset();

	return rv == WAIT_OBJECT_0;
}

bool UserModeDebugOutput::IsRunning() const {
	return ::WaitForSingleObject(_hThread.get(), 0) == WAIT_TIMEOUT;
}

DWORD UserModeDebugOutput::DebugListen() {
	HANDLE h[] = { _hStop.get(), _hDataReady.get() };

	while (true) {
		::SetEvent(_hBufferReady.get());
		auto rv = ::WaitForMultipleObjects(_countof(h), h, FALSE, INFINITE);
		if (rv == WAIT_OBJECT_0)
			break;

		ATLASSERT(rv == WAIT_OBJECT_0 + 1);

		FILETIME st;
		::GetSystemTimeAsFileTime(&st);
		DWORD pid = *(DWORD*)_buffer;
		_sink->DebugOutput(pid, (PCSTR)(_buffer + sizeof(DWORD)), st);
	}

	return 0;
}
