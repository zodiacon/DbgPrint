#pragma once

#include <wil\resource.h>
#include <shared_mutex>

struct StaticProcessInfo {
	CString FullPath;
	CString Name;
	DWORD ProcessId;
	FILETIME StartTime{};
	wil::unique_handle hProcess;
};

class ProcessManager {
public:
	static ProcessManager& Get();

	PCWSTR GetProcessName(DWORD pid) const;
	PCWSTR GetFullImagePath(DWORD pid) const;

private:
	ProcessManager();
	bool AddProcess(DWORD pid, PCWSTR path = nullptr) const;
	void Init();
	void AddProcessIfNotExist(DWORD pid) const;

	mutable std::unordered_map<DWORD, StaticProcessInfo> _processes;
	mutable std::shared_mutex _lock;
};

