#pragma once

#include <wil\resource.h>
#include <shared_mutex>
#include "Interfaces.h"
#include <span>

enum class ProcessInfoFlags {
	None = 0,
	FromFile = 1,
};
DEFINE_ENUM_FLAG_OPERATORS(ProcessInfoFlags);

struct StaticProcessInfo : ProcessKey {
	std::wstring FullPath;
	std::wstring CommandLine;
	std::wstring Name;
	wil::unique_handle hProcess;
	DWORD SessionId;
	ProcessInfoFlags Flags{ ProcessInfoFlags::None };
};

class ProcessManager {
public:
	ProcessManager();
	std::wstring GetProcessName(DWORD pid) const;
	PCWSTR GetFullImagePath(DWORD pid) const;
	ProcessKey GetProcessKey(DWORD pid) const;
	StaticProcessInfo* const GetProcessInfo(ProcessKey const&) const;

	std::vector<StaticProcessInfo*> GetRuntimeProcesses() const;

	void AddProcessesNoLock(std::span<std::shared_ptr<StaticProcessInfo>> const& processes);

private:
	void AddProcess(std::shared_ptr<StaticProcessInfo>& pi) const;
	bool AddProcess(DWORD pid, PCWSTR path = nullptr) const;
	void Init();
	void AddProcessIfNotExist(DWORD pid) const;

	mutable std::unordered_map<ProcessKey, std::shared_ptr<StaticProcessInfo>> _processesByKey;
	mutable std::unordered_map<DWORD, std::shared_ptr<StaticProcessInfo>> _processes;
	mutable std::shared_mutex _lock;
};

