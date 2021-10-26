#pragma once

#include <wil\resource.h>
#include <shared_mutex>

struct ProcessKey {
	DWORD ProcessId;
	FILETIME StartTime{};
	bool operator==(const ProcessKey& other) const {
		return other.ProcessId == ProcessId && 
			other.StartTime.dwLowDateTime == StartTime.dwLowDateTime && 
			other.StartTime.dwHighDateTime == StartTime.dwLowDateTime;
	}
};

template<>
struct std::hash<ProcessKey> {
	size_t operator()(ProcessKey const& key) const {
		return (key.ProcessId << 16LL) ^ key.StartTime.dwLowDateTime ^ (key.StartTime.dwHighDateTime << 16LL);
	}
};

struct StaticProcessInfo : ProcessKey {
	CString FullPath;
	CString Name;
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

