#include "pch.h"
#include "UserModeDebugOutput.h"

UserModeDebugOutput::UserModeDebugOutput(PCWSTR prefix) : m_prefix(prefix) {
}

UserModeDebugOutput::~UserModeDebugOutput() {
	Stop();
}

bool UserModeDebugOutput::Run(IDebugOutput* sink) {
	m_sink = sink;
	m_hStop.reset(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
	if (!m_hStop)
		return false;

	if (!m_hBufferReady) {
		m_hBufferReady.reset(::CreateEvent(nullptr, FALSE, FALSE, m_prefix + L"\\DBWIN_BUFFER_READY"));
		if (!m_hBufferReady)
			return false;
	}

	if (!m_hDataReady) {
		m_hDataReady.reset(::CreateEvent(nullptr, FALSE, FALSE, m_prefix + L"\\DBWIN_DATA_READY"));
		if (!m_hDataReady)
			return false;
	}

	DWORD size = 1 << 12;	// 4KB
	if (!m_hMemFile) {
		m_hMemFile.reset(::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, m_prefix + L"\\DBWIN_BUFFER"));
		if (m_hMemFile == nullptr)
			return false;
	}

	if (!m_buffer) {
		m_buffer = (PBYTE)::MapViewOfFile(m_hMemFile.get(), FILE_MAP_READ, 0, 0, 0);
		if (m_buffer == nullptr)
			return false;
	}

	m_hThread.reset(::CreateThread(nullptr, 0, [](auto p) {
		return ((UserModeDebugOutput*)p)->DebugListen();
		}, this, 0, nullptr));
	return m_hThread != nullptr;
}

bool UserModeDebugOutput::Stop() {
	if (!m_hStop)
		return false;

	if (m_hThread == nullptr)
		return true;

	auto rv = ::SignalObjectAndWait(m_hStop.get(), m_hThread.get(), 2000, FALSE);
	m_hThread.reset();

	return rv == WAIT_OBJECT_0;
}

bool UserModeDebugOutput::IsRunning() const {
	return ::WaitForSingleObject(m_hThread.get(), 0) == WAIT_TIMEOUT;
}

DWORD UserModeDebugOutput::DebugListen() {
	::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	HANDLE h[] = { m_hStop.get(), m_hDataReady.get() };

	while (true) {
		::SetEvent(m_hBufferReady.get());
		auto rv = ::WaitForMultipleObjects(_countof(h), h, FALSE, INFINITE);
		if (rv == WAIT_OBJECT_0)
			break;

		ATLASSERT(rv == WAIT_OBJECT_0 + 1);

		FILETIME st;
		::GetSystemTimeAsFileTime(&st);
		DWORD pid = *(DWORD*)m_buffer;
		m_sink->DebugOutput(pid, (PCSTR)(m_buffer + sizeof(DWORD)), st);
	}

	return 0;
}
