#include "pch.h"
#include "StackSymbols.h"
#include <winternl.h>
#include <Psapi.h>
#include <DbgHelp.h>
#include <mutex>
#include <unordered_set>
#include <wil\resource.h>

#pragma comment(lib, "ntdll")

namespace {
	typedef struct _RTL_PROCESS_MODULE_INFORMATION {
		HANDLE Section;
		PVOID MappedBase;
		PVOID ImageBase;
		ULONG ImageSize;
		ULONG Flags;
		USHORT LoadOrderIndex;
		USHORT InitOrderIndex;
		USHORT LoadCount;
		USHORT OffsetToFileName;
		UCHAR FullPathName[256];
	} RTL_PROCESS_MODULE_INFORMATION;

	typedef struct _RTL_PROCESS_MODULES {
		ULONG NumberOfModules;
		RTL_PROCESS_MODULE_INFORMATION Modules[1];
	} RTL_PROCESS_MODULES;

	const auto SystemModuleInformation = (SYSTEM_INFORMATION_CLASS)11;
	const NTSTATUS STATUS_INFO_LENGTH_MISMATCH = 0xC0000004;

	//
	// kernel module paths are NT style (\SystemRoot\..., \??\C:\...)
	//
	CString KernelImagePath(PCSTR fullPath) {
		CString path(fullPath);
		WCHAR windows[MAX_PATH];
		::GetWindowsDirectory(windows, _countof(windows));
		if (path.Left(12).CompareNoCase(L"\\SystemRoot\\") == 0)
			path = windows + path.Mid(11);
		else if (path.Left(9).CompareNoCase(L"\\Windows\\") == 0)
			path = CString(windows, 2) + path;	// the drive letter
		else if (path.Left(4) == L"\\??\\")
			path = path.Mid(4);
		return path;
	}

	//
	// the Debugging Tools' DbgHelp comes with symsrv.dll, which symbol server downloads need;
	// the system's copy does not, so it only finds symbols already in the cache
	//
	HMODULE LoadDbgHelp() {
#if defined(_M_ARM64)
		const PCWSTR arch = L"arm64";
#elif defined(_M_X64)
		const PCWSTR arch = L"x64";
#else
		const PCWSTR arch = L"x86";
#endif
		WCHAR root[MAX_PATH];
		for (DWORD view : { 0, RRF_SUBKEY_WOW6432KEY }) {
			DWORD size = sizeof(root);
			if (::RegGetValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", L"KitsRoot10",
				RRF_RT_REG_SZ | view, nullptr, root, &size) == ERROR_SUCCESS) {
				CString path;
				path.Format(L"%sDebuggers\\%s\\dbghelp.dll", root, arch);
				if (auto hModule = ::LoadLibraryEx(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH))
					return hModule;
			}
		}
		return ::LoadLibraryEx(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	}

	struct DbgHelp {
		decltype(&::SymSetOptions) SymSetOptions;
		decltype(&::SymInitializeW) SymInitializeW;
		decltype(&::SymLoadModuleExW) SymLoadModuleExW;
		decltype(&::SymFromAddrW) SymFromAddrW;
		decltype(&::SymCleanup) SymCleanup;
		std::mutex Lock;	// DbgHelp is single threaded

		// null if DbgHelp could not be loaded
		static DbgHelp* Get() {
			static DbgHelp* api = []() -> DbgHelp* {
				auto hModule = LoadDbgHelp();
				if (!hModule)
					return nullptr;

				auto api = new DbgHelp;	// kept for the lifetime of the process
				api->SymSetOptions = (decltype(api->SymSetOptions))::GetProcAddress(hModule, "SymSetOptions");
				api->SymInitializeW = (decltype(api->SymInitializeW))::GetProcAddress(hModule, "SymInitializeW");
				api->SymLoadModuleExW = (decltype(api->SymLoadModuleExW))::GetProcAddress(hModule, "SymLoadModuleExW");
				api->SymFromAddrW = (decltype(api->SymFromAddrW))::GetProcAddress(hModule, "SymFromAddrW");
				api->SymCleanup = (decltype(api->SymCleanup))::GetProcAddress(hModule, "SymCleanup");
				if (!api->SymSetOptions || !api->SymInitializeW || !api->SymLoadModuleExW || !api->SymFromAddrW || !api->SymCleanup) {
					delete api;
					return nullptr;
				}
				api->SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
				return api;
			}();
			return api;
		}
	};
}

std::vector<StackSymbols::Module> const& StackSymbols::KernelModules() {
	if (m_KernelLoaded)
		return m_Kernel;
	m_KernelLoaded = true;

	ULONG size = 1 << 16;
	std::unique_ptr<BYTE[]> buffer;
	NTSTATUS status;
	do {
		buffer = std::make_unique<BYTE[]>(size);
		status = ::NtQuerySystemInformation(SystemModuleInformation, buffer.get(), size, &size);
	} while (status == STATUS_INFO_LENGTH_MISMATCH);
	if (!NT_SUCCESS(status))
		return m_Kernel;

	auto info = (RTL_PROCESS_MODULES*)buffer.get();
	m_Kernel.reserve(info->NumberOfModules);
	for (ULONG i = 0; i < info->NumberOfModules; i++) {
		auto& m = info->Modules[i];
		auto base = (ULONG64)m.ImageBase;
		if (base == 0)		// addresses are hidden without SeDebugPrivilege
			continue;
		m_Kernel.push_back({ base, base + m.ImageSize,
			CString((PCSTR)m.FullPathName + m.OffsetToFileName), KernelImagePath((PCSTR)m.FullPathName) });
	}
	return m_Kernel;
}

std::vector<StackSymbols::Module> const& StackSymbols::UserModules(ProcessKey const& process) {
	auto [it, inserted] = m_User.try_emplace(process);
	auto& modules = it->second;
	if (!inserted || process.ProcessId <= 4)
		return modules;

	wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process.ProcessId));
	if (!hProcess)
		return modules;

	//
	// make sure this is the same process and not a new one that reused the PID
	//
	FILETIME start, dummy;
	if (!::GetProcessTimes(hProcess.get(), &start, &dummy, &dummy, &dummy) ||
		::CompareFileTime(&start, &process.StartTime) != 0)
		return modules;

	HMODULE handles[1024];
	DWORD needed;
	if (!::EnumProcessModulesEx(hProcess.get(), handles, sizeof(handles), &needed, LIST_MODULES_ALL))
		return modules;

	auto count = std::min<size_t>(needed / sizeof(HMODULE), _countof(handles));
	for (DWORD i = 0; i < count; i++) {
		MODULEINFO mi;
		WCHAR path[MAX_PATH];
		if (::GetModuleInformation(hProcess.get(), handles[i], &mi, sizeof(mi)) &&
			::GetModuleFileNameEx(hProcess.get(), handles[i], path, _countof(path))) {
			auto base = (ULONG64)mi.lpBaseOfDll;
			auto name = wcsrchr(path, L'\\');
			modules.push_back({ base, base + mi.SizeOfImage, name ? name + 1 : path, path });
		}
	}
	return modules;
}

StackSymbols::Module const* StackSymbols::Find(ULONG64 address, ProcessKey const& process) {
	auto find = [=](std::vector<Module> const& modules) -> Module const* {
		auto it = std::find_if(modules.begin(), modules.end(), [=](auto const& m) {
			return address >= m.Base && address < m.End;
			});
		return it == modules.end() ? nullptr : &*it;
	};
	if (auto module = find(KernelModules()))
		return module;
	return find(UserModules(process));
}

std::vector<CString> StackSymbols::ModuleOffsets(std::vector<ULONG64> const& stack, ProcessKey const& process) {
	std::vector<CString> offsets;
	offsets.reserve(stack.size());
	for (auto address : stack) {
		CString text;
		if (auto module = Find(address, process))
			text.Format(L"%s+0x%llX", (PCWSTR)module->Name, address - module->Base);
		offsets.push_back(std::move(text));
	}
	return offsets;
}

std::vector<CString> StackSymbols::FunctionNames(std::vector<ULONG64> const& stack, ProcessKey const& process, std::wstring const& symbolPath) {
	std::vector<CString> names(stack.size());
	auto api = DbgHelp::Get();
	if (!api)
		return names;

	std::lock_guard locker(api->Lock);

	//
	// the handle only identifies the DbgHelp session; no process memory is read, the modules are loaded from their files
	//
	auto session = reinterpret_cast<HANDLE>(this);
	//
	// never null: DbgHelp would then make up a path of its own, with _NT_SYMBOL_PATH in it
	//
	if (!api->SymInitializeW(session, symbolPath.c_str(), FALSE))
		return names;

	std::unordered_set<ULONG64> loaded;
	alignas(SYMBOL_INFOW) BYTE buffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(WCHAR)];
	auto symbol = reinterpret_cast<SYMBOL_INFOW*>(buffer);

	for (size_t i = 0; i < stack.size(); i++) {
		auto module = Find(stack[i], process);
		if (!module)
			continue;
		if (loaded.insert(module->Base).second)
			api->SymLoadModuleExW(session, nullptr, module->Path, nullptr, module->Base, (DWORD)(module->End - module->Base), nullptr, 0);

		::memset(buffer, 0, sizeof(SYMBOL_INFOW));
		symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
		symbol->MaxNameLen = MAX_SYM_NAME;
		DWORD64 displacement = 0;
		if (api->SymFromAddrW(session, stack[i], &displacement, symbol)) {
			//
			// as debuggers show it: the module name without its extension
			//
			CString name(module->Name);
			if (int dot = name.ReverseFind(L'.'); dot > 0)
				name = name.Left(dot);
			names[i].Format(L"%s!%s+0x%llX", (PCWSTR)name, symbol->Name, displacement);
		}
	}
	api->SymCleanup(session);
	return names;
}
