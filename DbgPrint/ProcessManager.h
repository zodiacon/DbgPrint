#pragma once

#include <wil\resource.h>
#include <shared_mutex>
#include "Interfaces.h"

enum class ProcessInfoFlags {
	None = 0,
	FromFile = 1,
};
DEFINE_ENUM_FLAG_OPERATORS(ProcessInfoFlags);

struct StaticProcessInfo : ProcessKey {
	CString FullPath;
	CString Name;
	wil::unique_handle hProcess;
	DWORD SessionId;
	ProcessInfoFlags Flags{ ProcessInfoFlags::None };
};

class ProcessManager {
public:
	static ProcessManager& Get();

	CString GetProcessName(DWORD pid) const;
	PCWSTR GetFullImagePath(DWORD pid) const;
	ProcessKey GetProcessKey(DWORD pid) const;
	StaticProcessInfo* const GetProcessInfo(ProcessKey const&) const;

	std::vector<StaticProcessInfo*> GetRuntimeProcesses() const;

private:
	ProcessManager();
	bool AddProcess(DWORD pid, PCWSTR path = nullptr) const;
	void Init();
	void AddProcessIfNotExist(DWORD pid) const;
	void AddProcess(std::unique_ptr<StaticProcessInfo>& pi) const;

	mutable std::unordered_map<ProcessKey, std::shared_ptr<StaticProcessInfo>> _processesByKey;
	mutable std::unordered_map<DWORD, std::shared_ptr<StaticProcessInfo>> _processes;
	mutable std::shared_mutex _lock;
};

