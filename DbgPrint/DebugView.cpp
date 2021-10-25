#include "pch.h"
#include "DebugView.h"
#include "SortHelper.h"
#include "ImageIconCache.h"
#include "ProcessManager.h"
#include "SecurityHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"
#include "AppSettings.h"

void CDebugView::UpdateList() {
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	DoSort(GetSortInfo(m_List));
	if (AppSettings::Get().AutoScroll())
		m_List.EnsureVisible(m_List.GetItemCount() - 1, FALSE);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

LRESULT CDebugView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
		LVS_OWNERDATA | LVS_REPORT | LVS_SHAREIMAGELISTS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_SUBITEMIMAGES);
	m_List.SetImageList(ImageIconCache::Get().GetImageList(), LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"", LVCFMT_RIGHT, 1);
	cm->AddColumn(L"#", LVCFMT_RIGHT, 70, ColumnType::Index);
	cm->AddColumn(L"Time", LVCFMT_RIGHT, 110, ColumnType::Time);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 80, ColumnType::ProcessId);
	cm->AddColumn(L"Process Name", LVCFMT_LEFT, 180, ColumnType::ProcessName);
	cm->AddColumn(L"Message", LVCFMT_LEFT, 500, ColumnType::Text);
	cm->AddColumn(L"Comment", LVCFMT_LEFT, 150, ColumnType::Comment);
	cm->UpdateColumns();

	m_List.DeleteColumn(0);
	cm->DeleteColumn(0);

	m_TempItems.reserve(128);
	m_Items.reserve(4096);

	SetTimer(1, 1000);

	auto& settings = AppSettings::Get();
	if (settings.Capture()) {
		if (settings.CaptureUserMode())
			m_UserMode.Run(this);
		if (SecurityHelper::IsRunningElevated()) {
			if (settings.CaptureSession0())
				m_UserModeSession0.Run(this);
			if (settings.CaptureKernel())
				m_KernelMode.Run(this);
		}
	}
	return 0;
}

LRESULT CDebugView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1) {
		{
			std::lock_guard locker(m_Lock);
			if (m_TempItems.empty())
				return 0;

			m_Items.insert(m_Items.end(), m_TempItems.begin(), m_TempItems.end());
			m_TempItems.clear();
		}
		UpdateList();
	}
	return 0;
}

CString CDebugView::GetColumnText(HWND h, int row, int col) const {
	CString text;
	auto& item = *m_Items[row];

	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Index: text.Format(L"%u", item.Index); break;
		case ColumnType::Time:
			if (item.LocalTimeAsString.IsEmpty()) {
				FILETIME ft;
				::FileTimeToLocalFileTime(&item.SystemTime, &ft);
				SYSTEMTIME st;
				::FileTimeToSystemTime(&ft, &st);
				item.LocalTimeAsString.Format(L"%02d:%02d:%02d.%03d%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, ft.dwLowDateTime * 10 % 1000);

			}
			return item.LocalTimeAsString;

		case ColumnType::ProcessId: text.Format(L"%u", item.Pid); break;
		case ColumnType::ProcessName: return item.ProcessName;
	}
	return text;
}

PCWSTR CDebugView::GetExistingColumnText(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	if (GetColumnManager(h)->GetColumnTag<ColumnType>(col) == ColumnType::Text)
		return item->Text;

	return nullptr;
}

void CDebugView::DebugOutput(DWORD pid, PCSTR text, FILETIME const& time, DebugOutputFlags flags) {
	auto item = std::make_shared<DebugItem>();
	item->Pid = pid;
	item->Text = text;
	item->SystemTime = time;
	item->Flags = flags;
	item->Index = InterlockedIncrement(&s_Index);
	item->ProcessName = ProcessManager::Get().GetProcessName(pid);
	item->Image = pid <= 4 ? 0 : ImageIconCache::Get().GetIcon(ProcessManager::Get().GetFullImagePath(pid));
	std::lock_guard locker(m_Lock);
	m_TempItems.push_back(std::move(item));
}

void CDebugView::DoSort(SortInfo* const si) {
	if (si == nullptr)
		return;

}

void CDebugView::Capture(bool capture) {
	if (capture) {
		auto& settings = AppSettings::Get();
		if (settings.CaptureUserMode())
			m_UserMode.Run(this);
		if (settings.CaptureSession0())
			m_UserModeSession0.Run(this);
	}
	else {
		m_UserMode.Stop();
		m_UserModeSession0.Stop();
	}
}

void CDebugView::CaptureKernel(bool capture) {
	capture ? m_KernelMode.Run(this) : m_KernelMode.Stop();
}

void CDebugView::CaptureUser(bool capture) {
	capture ? m_UserMode.Run(this) : m_UserMode.Stop();
}

void CDebugView::CaptureSession0(bool capture) {
	capture ? m_UserModeSession0.Run(this) : m_UserModeSession0.Stop();
}

int CDebugView::GetRowImage(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	if (col == 0)
		return ((item->Flags & DebugOutputFlags::Kernel) == DebugOutputFlags::Kernel) ? 1 : -1;

	auto type = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	if (type != ColumnType::ProcessName)
		return -1;

	if (item->Image >= 0)
		return item->Image;
	return item->Pid ? 0 : 1;
}

LRESULT CDebugView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& handled) {
	KillTimer(1);
	m_UserMode.Stop();
	m_UserModeSession0.Stop();
	handled = FALSE;
	return 0;
}

LRESULT CDebugView::OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int n = -1;
	CString text;
	while ((n = m_List.GetNextItem(n, LVIS_SELECTED)) != -1) {
		text += ListViewHelper::GetRowAsString(m_List, n, L',') + L"\n";
	}
	ClipboardHelper::CopyText(m_hWnd, text);
	return 0;
}

LRESULT CDebugView::OnSetFont(UINT, WPARAM wp, LPARAM, BOOL&) {
	m_List.SetFont((HFONT)wp);
	return 0;
}
