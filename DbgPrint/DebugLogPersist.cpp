#include "pch.h"
#include "DebugLogPersist.h"
#include <wil\resource.h>
#include "DebugOutputBase.h"
#include "Interfaces.h"
#include <CompoundFile.h>
#include <CompoundFileReaderWriter.h>
#include "ProcessManager.h"
#include "ImageIconCache.h"

bool DebugLogPersist::Save(PersistFormat format, std::vector<std::shared_ptr<DebugItem>> const& items, ImageIconCache const& iconCache, ProcessManager const& pm, std::wstring_view path) {
	return format == PersistFormat::Native ? SaveNative(items, iconCache, pm, path) : SaveCSV(items, path);
}

bool DebugLogPersist::Load(std::wstring_view path, std::vector<std::shared_ptr<DebugItem>>& items, ImageIconCache& iconCache, ProcessManager& pm) {
	using namespace StructuredStorage;

	auto file = CompoundFile::Open(path.data());
	if (!file)
		return false;

	DWORD count = 0;
	if (!OpenFileAndRead(file, L"Count", count))
		return false;
	items.reserve(count + items.size());

	for (DWORD i = 0; i < count; i++) {
		auto dir = file.OpenStructuredDirectory(std::to_wstring(i));
		if (!dir)
			break;

		auto item = std::make_shared<DebugItem>();
		if (!item)
			break;
		OpenFileAndRead(dir, L"Index", item->Index);
		OpenFileAndRead(dir, L"ProcessName", item->ProcessName);
		OpenFileAndRead(dir, L"ProcessKey", item->Process);
		OpenFileAndRead(dir, L"Time", item->SystemTime);
		OpenFileAndRead(dir, L"Text", item->Text);
		OpenFileAndRead(dir, L"Comment", item->Comment);
		OpenFileAndRead(dir, L"Flags", item->Flags);
		OpenFileAndRead(dir, L"Image", item->Image);
		OpenFileAndRead(dir, L"Stack", item->Stack);	// not present in older files

		items.push_back(item);
	}

	//
	// item image indices refer to the saved image list
	//
	if (auto images = file.OpenStructuredFile(L"ImageList"))
		iconCache.Load(images);

	auto pdir = file.OpenStructuredDirectory(L"Processes");
	if (!pdir)
		return true;

	DWORD processCount = 0;
	OpenFileAndRead(pdir, L"ProcessCount", processCount);

	std::vector<std::shared_ptr<StaticProcessInfo>> processes;
	processes.reserve(processCount);
	for (DWORD i = 0; i < processCount; i++) {
		auto pidir = pdir.OpenStructuredDirectory(std::to_wstring(i));
		if (!pidir)
			break;
		auto pi = std::make_shared<StaticProcessInfo>();
		OpenFileAndRead(pidir, L"Name", pi->Name);
		OpenFileAndRead(pidir, L"FullPath", pi->FullPath);
		OpenFileAndRead(pidir, L"CommandLine", pi->CommandLine);
		OpenFileAndRead(pidir, L"Session", pi->SessionId);
		OpenFileAndRead(pidir, L"StartTime", pi->StartTime);
		OpenFileAndRead(pidir, L"ProcessId", pi->ProcessId);
		OpenFileAndRead(pidir, L"Flags", pi->Flags);
		processes.push_back(std::move(pi));
	}
	pm.AddProcessesNoLock(processes);
	return true;
}

namespace {
	//
	// RFC 4180 field: quoted, with embedded quotes doubled (commas and newlines are then allowed)
	//
	void AppendCsvField(CString& line, std::wstring_view text) {
		line += L'"';
		for (auto ch : text) {
			if (ch == L'"')
				line += L'"';
			line += ch;
		}
		line += L'"';
	}
}

bool DebugLogPersist::SaveCSV(std::vector<std::shared_ptr<DebugItem>> const& items, std::wstring_view path) {
	CString text(L"Index,Time,PID,Process Name,Source,Text,Comment\r\n");
	for (auto const& item : items) {
		CString line;
		line.Format(L"%u,%s,%u,", item->Index, (PCWSTR)item->LocalTime(), item->Process.ProcessId);
		AppendCsvField(line, item->ProcessName);
		line += (item->Flags & DebugOutputFlags::Kernel) == DebugOutputFlags::Kernel ? L",Kernel," : L",User,";
		AppendCsvField(line, item->Text);
		line += L',';
		AppendCsvField(line, item->Comment);
		line += L"\r\n";
		text += line;
	}

	//
	// UTF-8 with a byte order mark, which Excel needs to detect the encoding
	//
	CStringA utf8(CW2A(text, CP_UTF8));
	wil::unique_hfile hFile(::CreateFile(path.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	if (!hFile)
		return false;

	const BYTE bom[] = { 0xEF, 0xBB, 0xBF };
	DWORD bytes;
	return ::WriteFile(hFile.get(), bom, sizeof(bom), &bytes, nullptr) &&
		::WriteFile(hFile.get(), (PCSTR)utf8, utf8.GetLength(), &bytes, nullptr);
}

bool DebugLogPersist::SaveNative(std::vector<std::shared_ptr<DebugItem>> const& items, ImageIconCache const& iconCache, ProcessManager const& pm, std::wstring_view path) {
	using namespace StructuredStorage;

	auto file = CompoundFile::Create(path.data());
	if(!file)
		return false;

	CreateFileAndWrite(file, L"Count", (DWORD)items.size());
	int i = 0;
	for (auto const& item : items) {
		auto dir = file.CreateStructuredDirectory(std::to_wstring(i++));
		if (!dir)
			return false;

		CreateFileAndWrite(dir, L"Index", item->Index);
		CreateFileAndWrite(dir, L"ProcessName", item->ProcessName);
		CreateFileAndWrite(dir, L"ProcessKey", item->Process);
		CreateFileAndWrite(dir, L"Time", item->SystemTime);
		CreateFileAndWrite(dir, L"Text", item->Text);
		CreateFileAndWrite(dir, L"Image", item->Image);
		CreateFileAndWrite(dir, L"Comment", item->Comment);
		CreateFileAndWrite(dir, L"Flags", item->Flags);
		if (!item->Stack.empty())
			CreateFileAndWrite(dir, L"Stack", item->Stack);
	}

	auto images = file.CreateStructuredFile(L"ImageList");
	CImageList il(iconCache.GetImageList());
	il.Write(images);

	//
	// save process information
	//
	auto dir = file.CreateStructuredDirectory(L"Processes");

	i = 0;
	auto processes = pm.GetRuntimeProcesses();
	CreateFileAndWrite(dir, L"ProcessCount", (ULONG)processes.size());

	for (auto& pi : processes) {
		auto pdir = dir.CreateStructuredDirectory(std::to_wstring(i++));
		CreateFileAndWrite(pdir, L"Name", pi->Name);
		CreateFileAndWrite(pdir, L"FullPath", pi->FullPath);
		CreateFileAndWrite(pdir, L"CommandLine", pi->CommandLine);
		CreateFileAndWrite(pdir, L"Session", pi->SessionId);
		CreateFileAndWrite(pdir, L"StartTime", pi->StartTime);
		CreateFileAndWrite(pdir, L"ProcessId", pi->ProcessId);
		CreateFileAndWrite(pdir, L"Flags", pi->Flags);
	}

	return true;
}
