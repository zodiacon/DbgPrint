#include "pch.h"
#include "DebugOutputBase.h"

HANDLE g_hHeap = ::HeapCreate(0, 1 << 20, 0);

void* DebugItem::operator new(size_t size) {
	ATLASSERT(g_hHeap);
	return ::HeapAlloc(g_hHeap, 0, size);
}

void DebugItem::operator delete(void* p) {
	::HeapFree(g_hHeap, 0, p);
}

CString const& DebugItem::LocalTime() const {
	if (LocalTimeAsString.IsEmpty()) {
		FILETIME ft;
		::FileTimeToLocalFileTime(&SystemTime, &ft);
		SYSTEMTIME st;
		::FileTimeToSystemTime(&ft, &st);
		//
		// FILETIME is in 100 nsec units; the microseconds within the current millisecond
		//
		auto micro = (ULONG)((*(ULONGLONG*)&ft / 10) % 1000);
		LocalTimeAsString.Format(L"%02d:%02d:%02d.%03d%03u",
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, micro);
	}
	return LocalTimeAsString;
}

