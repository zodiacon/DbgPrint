#include "pch.h"
#include "DebugOutputBase.h"

HANDLE g_hHeap = ::HeapCreate(HEAP_NO_SERIALIZE, 1 << 20, 0);

void* DebugItem::operator new(size_t size) {
	ATLASSERT(g_hHeap);
	return ::HeapAlloc(g_hHeap, HEAP_NO_SERIALIZE, size);
}

void DebugItem::operator delete(void* p) {
	::HeapFree(g_hHeap, HEAP_NO_SERIALIZE, p);
}

