#pragma once

#include "Interfaces.h"

struct Helpers {
	static void ReportError(PCWSTR text, DWORD error = ::GetLastError());
	static bool EnableAllkernelOutput(bool enable);
	static int GetKernelComponentNames(PCWSTR*& names);
	// "module+offset" for each frame, or an empty string if the address is not in a known module
	static std::vector<CString> ResolveStack(std::vector<ULONG64> const& stack, ProcessKey const& process);
};

