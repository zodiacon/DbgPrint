#pragma once

#include "Interfaces.h"

enum class DebugOutputFlags {
	None = 0,
	Kernel = 0x100,
	Bookmark = 0x200,
	Highlight = 0x400,
};
DEFINE_ENUM_FLAG_OPERATORS(DebugOutputFlags);

struct IDebugOutput {
	virtual void DebugOutput(DWORD pid, PCSTR text, FILETIME const&, DebugOutputFlags flags = DebugOutputFlags::None) = 0;
};

struct DebugOutputBase abstract {
	virtual bool Run(IDebugOutput* sink) = 0;
	virtual bool Stop() = 0;
	virtual bool IsRunning() const = 0;
};

enum class ViewType {
	RealTimeLog,
	FileLog,
};

struct IViewBase abstract {
	virtual BOOL ProcessWindowMessage(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam,
		_In_ LPARAM lParam, _Inout_ LRESULT& lResult, _In_ DWORD dwMsgMapID = 0) = 0;

};

struct DebugItem {
	void* operator new(size_t size);
	void operator delete(void* p);

	ProcessKey Process;
	std::wstring Text;
	FILETIME SystemTime;
	mutable CString LocalTimeAsString;
	std::wstring ProcessName;
	DWORD Index;
	DebugOutputFlags Flags;
	std::wstring Comment;
	int Image{ -1 };
};
