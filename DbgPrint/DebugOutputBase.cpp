#include "pch.h"
#include "DebugOutputBase.h"

HANDLE g_hHeap = ::HeapCreate(0, 2 << 20, 0);

void* DebugItem::operator new(size_t size) {
	ATLASSERT(g_hHeap);
	return ::HeapAlloc(g_hHeap, 0, size);
}

void DebugItem::operator delete(void* p) {
	::HeapFree(g_hHeap, 0, p);
}

