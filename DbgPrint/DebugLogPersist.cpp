#include "pch.h"
#include "DebugLogPersist.h"
#include <fstream>
#include "DebugOutputBase.h"
#include "Interfaces.h"
#include <CompoundFile.h>
#include <CompoundFileReaderWriter.h>
#include "ProcessManager.h"
#include "ImageIconCache.h"

bool DebugLogPersist::Save(PersistFormat format, std::vector<std::shared_ptr<DebugItem>> const& items, ImageIconCache const& iconCache, ProcessManager const& pm, std::wstring_view path) {
	return format == PersistFormat::Native ? SaveNative(items, iconCache, pm, path) : SaveCSV(items, path);
}

bool DebugLogPersist::Load(std::wstring_view path, std::vector<std::shared_ptr<DebugItem>>& items, ProcessManager& pm) {
	using namespace StructuredStorage;

	auto file = CompoundFile::Open(path.data());
	if (!file)
		return false;

	DWORD count;
	OpenFileAndRead(file, L"Count", count);
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

		items.push_back(item);
	}
	OpenFileAndRead(file, L"ProcessCount", count);
	auto pdir = file.OpenStructuredDirectory(L"Processes");

	std::vector<std::shared_ptr<StaticProcessInfo>> processes;
	processes.reserve(count);
	for (DWORD i = 0; i < count; i++) {
		auto pidir = pdir.OpenStructuredDirectory(std::to_wstring(i));
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

bool DebugLogPersist::SaveCSV(std::vector<std::shared_ptr<DebugItem>> const& items, std::wstring_view path) {
	std::wofstream stm;
	stm.open(path.data(), std::ios_base::out);
	if (!stm)
		return false;

	for (auto const& item : items) {
		stm << item->Index << "," << *(uint64_t*)&item->SystemTime << "," << (PCWSTR)item->LocalTimeAsString << ","
			<< item->ProcessName << "," << item->Process << "," << (DWORD)item->Flags << ","
			<< item->Text << "," << item->Text << "\n";
	}
	return true;
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
