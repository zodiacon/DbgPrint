#include "pch.h"
#include "ProcessManager.h"
#include <TlHelp32.h>

ProcessManager& ProcessManager::Get() {
	static ProcessManager pm;
	return pm;
}

PCWSTR ProcessManager::GetFullImagePath(DWORD pid) const {
	AddProcessIfNotExist(pid);
	{
		std::shared_lock locker(_lock);
		if (auto it = _processes.find(pid); it != _processes.end()) {
			auto& pi = it->second;
			return pi->FullPath;
		}
	}
	return L"";
}

ProcessKey ProcessManager::GetProcessKey(DWORD pid) const {
	std::shared_lock locker(_lock);
	if (auto it = _processes.find(pid); it != _processes.end()) {
		return *it->second;
	}
	return ProcessKey{ pid };
}

StaticProcessInfo* const ProcessManager::GetProcessInfo(ProcessKey const& key) const {
	std::shared_lock locker(_lock);
	if (auto it = _processesByKey.find(key); it != _processesByKey.end()) {
		return it->second.get();
	}
	return nullptr;
}

std::vector<StaticProcessInfo*> ProcessManager::GetRuntimeProcesses() const {
	std::vector<StaticProcessInfo*> processes;
	processes.reserve(64);

	std::shared_lock locker(_lock);
	for (auto& [key, pi] : _processesByKey) {
		if ((pi->Flags & ProcessInfoFlags::FromFile) == ProcessInfoFlags::None)
			processes.push_back(pi.get());
	}
	return processes;
}

CString ProcessManager::GetProcessName(DWORD pid) const {
	AddProcessIfNotExist(pid);
	{
		std::shared_lock locker(_lock);
		if (auto it = _processes.find(pid); it != _processes.end()) {
			auto pi = it->second;
			return pi->Name;
		}
	}

	return L"";
}

ProcessManager::ProcessManager() {
	Init();
}

bool ProcessManager::AddProcess(DWORD pid, PCWSTR name) const {
	auto info = std::make_shared<StaticProcessInfo>();
	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
	WCHAR full[MAX_PATH];
	if (hProcess) {
		DWORD len = _countof(full);
		if (::QueryFullProcessImageName(hProcess, 0, full, &len)) {
			info->FullPath = full;
		}
		FILETIME dummy;
		::GetProcessTimes(hProcess, &info->StartTime, &dummy, &dummy, &dummy);
		info->hProcess.reset(hProcess);
	}

	if (name == nullptr) {
		auto bs = info->FullPath.ReverseFind(L'\\');
		info->Name = bs < 0 ? info->FullPath : info->FullPath.Mid(bs + 1);
	}
	else {
		info->Name = name;
	}
	info->ProcessId = pid;
	{
		ProcessKey key{ pid, info->StartTime };
		std::lock_guard locker(_lock);
		_processes.insert({ pid, info });
		_processesByKey.insert({ key, std::move(info) });
	}
	return hProcess != nullptr;
}

void ProcessManager::Init() {
	_processes.clear();

	auto hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	ATLASSERT(hSnapshot != INVALID_HANDLE_VALUE);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);

	::Process32First(hSnapshot, &pe);
	while (::Process32Next(hSnapshot, &pe)) {
		AddProcess(pe.th32ProcessID, pe.szExeFile);
	}
	::CloseHandle(hSnapshot);
}

void ProcessManager::AddProcessIfNotExist(DWORD pid) const {
	{
		std::shared_lock locker(_lock);
		if (auto it = _processes.find(pid); it != _processes.end()) {
			auto& pi = *it->second;
			if (pi.hProcess && ::WaitForSingleObject(pi.hProcess.get(), 0) == WAIT_TIMEOUT)
				return;
			locker.unlock();
			std::lock_guard locker(_lock);
			_processes.erase(pid);
		}
	}
	AddProcess(pid);
}

void ProcessManager::AddProcess(std::unique_ptr<StaticProcessInfo>& pi) const {
	std::lock_guard locker(_lock);
	_processesByKey.insert({ *pi, std::move(pi) });
}
