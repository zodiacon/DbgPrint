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
	ULONGLONG Added{ 0 };	// tick count when the entry was created
};

class ProcessManager {
public:
	// live: start with the processes running now (not needed for logs loaded from a file)
	explicit ProcessManager(bool live = true);
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
	void CloseExitedProcesses() const;
	static std::wstring GetImageNameNoAccess(DWORD pid);

	// how often handles of exited processes are closed
	static const ULONGLONG SweepInterval = 10000;
	// how long before a process that could not be opened is looked up again
	static const ULONGLONG RetryInterval = 2000;

	mutable std::unordered_map<ProcessKey, std::shared_ptr<StaticProcessInfo>> _processesByKey;
	// live processes by PID; entries of exited processes remain in _processesByKey only
	mutable std::unordered_map<DWORD, std::shared_ptr<StaticProcessInfo>> _processes;
	mutable std::shared_mutex _lock;
	mutable std::atomic<ULONGLONG> _lastSweep{ 0 };
};

