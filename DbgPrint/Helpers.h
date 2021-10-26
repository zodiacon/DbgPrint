#pragma once

struct Helpers {
	static void ReportError(PCWSTR text, DWORD error = ::GetLastError());
};

