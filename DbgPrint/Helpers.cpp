#include "pch.h"
#include "Helpers.h"
#include "resource.h"
#include <winternl.h>
#include <dpfilter.h>
#include <Psapi.h>
#include <wil\resource.h>

#pragma comment(lib, "ntdll")

static PCWSTR components[] = {
	L"SYSTEM",
	L"SMSS",
	L"SETUP",
	L"NTFS",
	L"FSTUB",
	L"CRASHDUMP",
	L"CDAUDIO",
	L"CDROM",
	L"CLASSPNP",
	L"DISK",
	L"REDBOOK",
	L"STORPROP",
	L"SCSIPORT",
	L"SCSIMINIPORT",
	L"CONFIG",
	L"I8042PRT",
	L"SERMOUSE",
	L"LSERMOUS",
	L"KBDHID",
	L"MOUHID",
	L"KBDCLASS",
	L"MOUCLASS",
	L"TWOTRACK",
	L"WMILIB",
	L"ACPI",
	L"AMLI",
	L"HALIA64",
	L"VIDEO",
	L"SVCHOST",
	L"VIDEOPRT",
	L"TCPIP",
	L"DMSYNTH",
	L"NTOSPNP",
	L"FASTFAT",
	L"SAMSS",
	L"PNPMGR",
	L"NETAPI",
	L"SCSERVER",
	L"SCCLIENT",
	L"SERIAL",
	L"SERENUM",
	L"UHCD",
	L"RPCPROXY",
	L"AUTOCHK",
	L"DCOMSS",
	L"UNIMODEM",
	L"SIS",
	L"FLTMGR",
	L"WMICORE",
	L"BURNENG",
	L"IMAPI",
	L"SXS",
	L"FUSION",
	L"IDLETASK",
	L"SOFTPCI",
	L"TAPE",
	L"MCHGR",
	L"IDEP",
	L"PCIIDE",
	L"FLOPPY",
	L"FDC",
	L"TERMSRV",
	L"W32TIME",
	L"PREFETCHER",
	L"RSFILTER",
	L"FCPORT",
	L"PCI",
	L"DMIO",
	L"DMCONFIG",
	L"DMADMIN",
	L"WSOCKTRANSPORT",
	L"VSS",
	L"PNPMEM",
	L"PROCESSOR",
	L"DMSERVER",
	L"SR",
	L"INFINIBAND",
	L"IHVDRIVER",
	L"IHVVIDEO",
	L"IHVAUDIO",
	L"IHVNETWORK",
	L"IHVSTREAMING",
	L"IHVBUS",
	L"HPS",
	L"RTLTHREADPOOL",
	L"LDR",
	L"TCPIP6",
	L"ISAPNP",
	L"SHPC",
	L"STORPORT",
	L"STORMINIPORT",
	L"PRINTSPOOLER",
	L"VSSDYNDISK",
	L"VERIFIER",
	L"VDS",
	L"VDSBAS",
	L"VDSDYN",
	L"VDSDYNDR",
	L"VDSLDR",
	L"VDSUTIL",
	L"DFRGIFC",
	L"DEFAULT",
	L"MM",
	L"DFSC",
	L"WOW64",
	L"ALPC",
	L"WDI",
	L"PERFLIB",
	L"KTM",
	L"IOSTRESS",
	L"HEAP",
	L"WHEA",
	L"USERGDI",
	L"MMCSS",
	L"TPM",
	L"THREADORDER",
	L"ENVIRON",
	L"EMS",
	L"WDT",
	L"FVEVOL",
	L"NDIS",
	L"NVCTRACE",
	L"LUAFV",
	L"APPCOMPAT",
	L"USBSTOR",
	L"SBP2PORT",
	L"COVERAGE",
	L"CACHEMGR",
	L"MOUNTMGR",
	L"CFR",
	L"TXF",
	L"KSECDD",
	L"FLTREGRESS",
	L"MPIO",
	L"MSDSM",
	L"UDFS",
	L"PSHED",
	L"STORVSP",
	L"LSASS",
	L"SSPICLI",
	L"CNG",
	L"EXFAT",
	L"FILETRACE",
	L"XSAVE",
	L"SE",
	L"DRIVEEXTENDER",
	L"POWER",
	L"CRASHDUMPXHCI",
	L"GPIO",
	L"REFS",
	L"WER",
	L"CAPIMG",
	L"VPCI",
	L"STORAGECLASSMEMORY",
	L"FSLIB",
};

extern "C" NTSTATUS WINAPI NtSetDebugFilterState(
    ULONG ComponentId,
    ULONG Level,
    BOOLEAN State);

extern "C" ULONG WINAPI NtQueryDebugFilterState(
    __in ULONG ComponentId,
    __in ULONG Level);

void Helpers::ReportError(PCWSTR text, DWORD error) {
    PWSTR buffer;
    CString msg;
    if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, nullptr)) {
        msg = buffer;
        ::LocalFree(buffer);
        msg.Trim(L"\n\r");
    }
    AtlMessageBox(nullptr, (PCWSTR)(text + msg), IDS_TITLE, MB_ICONERROR);
}

bool Helpers::EnableAllkernelOutput(bool enable) {
	for (int i = 0; i < _countof(components); i++) {
		auto status = NtSetDebugFilterState(i, 0xffffffff, enable);
		if (!NT_SUCCESS(status))
			return false;
	}
    return true;
}

int Helpers::GetKernelComponentNames(PCWSTR*& names) {
	names = components;
	return _countof(components);
}

namespace {
	struct ModuleRange {
		ULONG64 Base;
		ULONG64 End;
		CString Name;
	};

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

	void AddKernelModules(std::vector<ModuleRange>& modules) {
		ULONG size = 1 << 16;
		std::unique_ptr<BYTE[]> buffer;
		NTSTATUS status;
		do {
			buffer = std::make_unique<BYTE[]>(size);
			status = ::NtQuerySystemInformation(SystemModuleInformation, buffer.get(), size, &size);
		} while (status == STATUS_INFO_LENGTH_MISMATCH);
		if (!NT_SUCCESS(status))
			return;

		auto info = (RTL_PROCESS_MODULES*)buffer.get();
		for (ULONG i = 0; i < info->NumberOfModules; i++) {
			auto& m = info->Modules[i];
			auto base = (ULONG64)m.ImageBase;
			if (base == 0)		// addresses are hidden without SeDebugPrivilege
				continue;
			modules.push_back({ base, base + m.ImageSize, CString((PCSTR)m.FullPathName + m.OffsetToFileName) });
		}
	}

	void AddProcessModules(std::vector<ModuleRange>& modules, ProcessKey const& process) {
		wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process.ProcessId));
		if (!hProcess)
			return;

		//
		// make sure this is the same process and not a new one that reused the PID
		//
		FILETIME start, dummy;
		if (!::GetProcessTimes(hProcess.get(), &start, &dummy, &dummy, &dummy) ||
			::CompareFileTime(&start, &process.StartTime) != 0)
			return;

		HMODULE handles[1024];
		DWORD needed;
		if (!::EnumProcessModulesEx(hProcess.get(), handles, sizeof(handles), &needed, LIST_MODULES_ALL))
			return;

		auto count = std::min<size_t>(needed / sizeof(HMODULE), _countof(handles));
		for (DWORD i = 0; i < count; i++) {
			MODULEINFO mi;
			WCHAR name[MAX_PATH];
			if (::GetModuleInformation(hProcess.get(), handles[i], &mi, sizeof(mi)) &&
				::GetModuleBaseName(hProcess.get(), handles[i], name, _countof(name))) {
				auto base = (ULONG64)mi.lpBaseOfDll;
				modules.push_back({ base, base + mi.SizeOfImage, name });
			}
		}
	}
}

std::vector<CString> Helpers::ResolveStack(std::vector<ULONG64> const& stack, ProcessKey const& process) {
	std::vector<ModuleRange> modules;
	modules.reserve(512);
	AddKernelModules(modules);
	if (process.ProcessId > 4)
		AddProcessModules(modules, process);

	std::vector<CString> symbols;
	symbols.reserve(stack.size());
	for (auto address : stack) {
		auto it = std::find_if(modules.begin(), modules.end(), [=](auto const& m) {
			return address >= m.Base && address < m.End;
			});
		CString symbol;
		if (it != modules.end())
			symbol.Format(L"%s+0x%llX", (PCWSTR)it->Name, address - it->Base);
		symbols.push_back(std::move(symbol));
	}
	return symbols;
}
