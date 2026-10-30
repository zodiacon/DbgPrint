#pragma once

#include "Interfaces.h"

//
// resolves call stack addresses: to module+offset (quick), or to functions with DbgHelp
// (may download symbols). An object caches module lists, and is used by one thread at a time.
//
class StackSymbols {
public:
	// "module+0x1234" for each frame, or an empty string if the address is not in a known module
	std::vector<CString> ModuleOffsets(std::vector<ULONG64> const& stack, ProcessKey const& process);

	// "module!function+0x12" for each frame, or an empty string where no symbol is found.
	// symbolPath is DbgHelp's search path (AppSettings::BuildSymbolSearchPath).
	// Loading symbols can take long the first time (downloads), so call it on a worker thread.
	std::vector<CString> FunctionNames(std::vector<ULONG64> const& stack, ProcessKey const& process, std::wstring const& symbolPath);

private:
	struct Module {
		ULONG64 Base;
		ULONG64 End;
		CString Name;	// file name
		CString Path;	// full Win32 path, for loading symbols
	};

	Module const* Find(ULONG64 address, ProcessKey const& process);
	std::vector<Module> const& KernelModules();
	std::vector<Module> const& UserModules(ProcessKey const& process);

	std::vector<Module> m_Kernel;
	bool m_KernelLoaded{ false };
	std::unordered_map<ProcessKey, std::vector<Module>> m_User;
};
