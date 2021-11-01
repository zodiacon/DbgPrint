#include "pch.h"
#include "Exporter.h"
#include "CompoundFile.h"
#include "CompoundFileReaderWriter.h"
#include "ImageIconCache.h"
#include "ProcessManager.h"

bool Exporter::Save(std::vector<std::unique_ptr<DebugItem>> const& items, PCWSTR path) {
	using namespace StructuredStorage;

	auto file = CompoundFile::Create(path);
	if (!file)
		return false;

	auto count = static_cast<UINT>(items.size());
	for (UINT i = 0; i < count; i++) {
		CString name;
		name.Format(L"Item%u\n", i);
		auto dir = file.CreateStructuredDirectory((PCWSTR)name);
		if (!dir)
			return false;

		auto const& item = *items[i];
		CreateFileAndWrite(dir, L"Process", item.Process);
		CreateFileAndWrite(dir, L"ProcessName", item.ProcessName);
		CreateFileAndWrite(dir, L"Index", item.Index);
		CreateFileAndWrite(dir, L"Image", item.Image);
		CreateFileAndWrite(dir, L"Text", item.Text);
		CreateFileAndWrite(dir, L"Comment", item.Comment);
		CreateFileAndWrite(dir, L"SystemTime", item.SystemTime);
	}

	auto images = file.CreateStructuredFile(L"ImageList");
	CImageList il(ImageIconCache::Get().GetImageList());
	il.Write(images);

	//
	// save process information
	//
	auto dir = file.CreateStructuredDirectory(L"Processes");

	int i = 0;
	auto processes = ProcessManager::Get().GetRuntimeProcesses();
	CreateFileAndWrite(dir, L"ProcessCount", (ULONG)processes.size());

	for (auto& pi : processes) {
		CString name;
		name.Format(L"Process%04d", i);
		auto pdir = dir.CreateStructuredDirectory((PCWSTR)name);
		CreateFileAndWrite(pdir, L"Name", pi->Name);
		CreateFileAndWrite(pdir, L"FullPath", pi->FullPath);
		CreateFileAndWrite(pdir, L"Session", pi->SessionId);
		CreateFileAndWrite(pdir, L"StartTime", pi->StartTime);
		CreateFileAndWrite(pdir, L"ProcessId", pi->ProcessId);
		CreateFileAndWrite(pdir, L"Flags", pi->Flags);
		i++;
	}
	return true;
}
