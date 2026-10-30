#pragma once

struct Helpers {
	static void ReportError(PCWSTR text, DWORD error = ::GetLastError());
	static bool EnableAllkernelOutput(bool enable);
	static int GetKernelComponentNames(PCWSTR*& names);
};

