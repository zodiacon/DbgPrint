#include "pch.h"
#include "ProcessManager.h"
#include <TlHelp32.h>
#include <winternl.h>

PCWSTR ProcessManager::GetFullImagePath(DWORD pid) const {
	AddProcessIfNotExist(pid);
	{
		std::shared_lock locker(_lock);
		if (auto it = _processes.find(pid); it != _processes.end()) {
			auto& pi = it->second;
			return pi->FullPath.c_str();
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

std::wstring ProcessManager::GetProcessName(DWORD pid) const {
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
		enum {
			ProcessCommandLineInformation = 60
		};
		BYTE buffer[2048];
		if (STATUS_SUCCESS == ::NtQueryInformationProcess(hProcess, (PROCESSINFOCLASS)ProcessCommandLineInformation, buffer, sizeof(buffer), nullptr)) {
			auto us = (UNICODE_STRING*)buffer;
			info->CommandLine = std::wstring(us->Buffer, us->Length / sizeof(WCHAR));
		}
	}

	if (name == nullptr) {
		auto path = info->FullPath.empty() ? GetImageNameNoAccess(pid) : info->FullPath;
		auto bs = path.rfind(L'\\');
		info->Name = bs == std::wstring::npos ? path : path.substr(bs + 1);
	}
	else {
		info->Name = name;
	}
	info->ProcessId = pid;
	info->Added = ::GetTickCount64();
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

	if (::Process32First(hSnapshot, &pe)) {
		do {
			AddProcess(pe.th32ProcessID, pe.szExeFile);
		} while (::Process32Next(hSnapshot, &pe));
	}
	::CloseHandle(hSnapshot);
}

void ProcessManager::AddProcessIfNotExist(DWORD pid) const {
	auto now = ::GetTickCount64();
	if (now - _lastSweep > SweepInterval) {
		_lastSweep = now;
		std::lock_guard locker(_lock);
		CloseExitedProcesses();
	}

	std::shared_ptr<StaticProcessInfo> existing;
	{
		std::shared_lock locker(_lock);
		if (auto it = _processes.find(pid); it != _processes.end()) {
			auto& pi = *it->second;
			if (pi.hProcess) {
				if (::WaitForSingleObject(pi.hProcess.get(), 0) == WAIT_TIMEOUT)
					return;
			}
			else if (now - pi.Added < RetryInterval) {
				//
				// the process could not be opened, so it's unknown whether it exited;
				// don't retry for every message it sends
				//
				return;
			}
			existing = it->second;
		}
	}
	if (existing) {
		std::lock_guard locker(_lock);
		existing->hProcess.reset();
		if (auto it = _processes.find(pid); it != _processes.end() && it->second == existing)
			_processes.erase(it);
	}
	AddProcess(pid);
}

void ProcessManager::CloseExitedProcesses() const {
	//
	// caller holds the lock exclusively; the entries stay in _processesByKey for existing items
	//
	for (auto it = _processes.begin(); it != _processes.end(); ) {
		auto& pi = *it->second;
		if (pi.hProcess && ::WaitForSingleObject(pi.hProcess.get(), 0) == WAIT_OBJECT_0) {
			pi.hProcess.reset();
			it = _processes.erase(it);
		}
		else {
			++it;
		}
	}
}

std::wstring ProcessManager::GetImageNameNoAccess(DWORD pid) {
	//
	// works for processes that cannot be opened (e.g. protected ones); returns an NT path
	//
	struct SYSTEM_PROCESS_ID_INFORMATION {
		HANDLE ProcessId;
		UNICODE_STRING ImageName;
	};
	const auto SystemProcessIdInformation = (SYSTEM_INFORMATION_CLASS)88;

	WCHAR buffer[MAX_PATH * 2];
	SYSTEM_PROCESS_ID_INFORMATION info{ ULongToHandle(pid) };
	info.ImageName.Buffer = buffer;
	info.ImageName.MaximumLength = sizeof(buffer);
	if (!NT_SUCCESS(::NtQuerySystemInformation(SystemProcessIdInformation, &info, sizeof(info), nullptr)))
		return L"";
	return std::wstring(info.ImageName.Buffer, info.ImageName.Length / sizeof(WCHAR));
}

void ProcessManager::AddProcess(std::shared_ptr<StaticProcessInfo>& pi) const {
	std::lock_guard locker(_lock);
	_processesByKey.insert({ *pi, std::move(pi) });
}

void ProcessManager::AddProcessesNoLock(std::span<std::shared_ptr<StaticProcessInfo>> const& processes) {
	for(auto& pi : processes)
		_processesByKey.insert({ *pi, std::move(pi) });
}
