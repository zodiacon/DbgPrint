#include "pch.h"
#include "Helpers.h"
#include "resource.h"
#include <winternl.h>
#include <dpfilter.h>

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
