#include "pch.h"
#include "UserModeDebugOutput.h"

UserModeDebugOutput::UserModeDebugOutput(PCWSTR prefix) : m_prefix(prefix) {
}

UserModeDebugOutput::~UserModeDebugOutput() {
	//
	// the listening thread uses this object, so it must be gone before the memory is freed
	//
	if (!Stop() && m_hThread)
		::WaitForSingleObject(m_hThread.get(), INFINITE);
	if (m_buffer)
		::UnmapViewOfFile(m_buffer);
}

bool UserModeDebugOutput::Run(IDebugOutput* sink) {
	//
	// a listening thread that did not exit in time during Stop still uses the events
	//
	if (m_hThread) {
		if (::WaitForSingleObject(m_hThread.get(), 0) == WAIT_TIMEOUT)
			return false;
		m_hThread.reset();
	}

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

	if (!m_hMemFile) {
		m_hMemFile.reset(::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, BufferSize, m_prefix + L"\\DBWIN_BUFFER"));
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
	if (rv != WAIT_OBJECT_0)
		return false;	// keep the handle, the thread is still running

	m_hThread.reset();
	return true;
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
		//
		// any process that can open the section can write to it, so don't rely on a terminating NUL
		//
		auto text = (PCSTR)(m_buffer + sizeof(DWORD));
		std::string message(text, strnlen(text, BufferSize - sizeof(DWORD)));
		m_sink->DebugOutput(pid, message.c_str(), st);
	}

	return 0;
}
