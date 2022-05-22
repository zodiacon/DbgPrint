#include "pch.h"
#include "DebugView.h"
#include "SortHelper.h"
#include "ImageIconCache.h"
#include "ProcessManager.h"
#include "SecurityHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"
#include "AppSettings.h"
#include "Helpers.h"
#include "PropertiesDlg.h"
#include "CommentDlg.h"

CDebugView::CDebugView(IMainFrame* frame, bool realTime) : m_pFrame(frame) {
}

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

LRESULT CDebugView::OnFindItem(int /*idCtrl*/, LPNMHDR hdr, BOOL& /*bHandled*/) {
	auto fi = (NMLVFINDITEM*)hdr;
	auto text = fi->lvfi.psz;
	auto col = GetColumnManager(m_List)->GetColumnByTag(ColumnType::ProcessName);
	int selected = m_List.GetNextItem(-1, LVIS_SELECTED);

	int start = selected + 1;
	int count = m_List.GetItemCount();
	CString name;
	for (int i = start; i < count + start; i++) {
		if (m_List.GetItemText(i % count, col, name) && name.CompareNoCase(text) == 0)
			return i % count;
	}
	return -1;
}

void CDebugView::ShowProperties(int selected) {
	auto& item = *m_Items[selected];
	CPropertiesDlg dlg(item);
	if (IDOK == dlg.DoModal()) {
		//
		// update comment
		//
		item.Comment = dlg.GetComment();
		item.Text = dlg.GetText();
		m_List.RedrawItems(selected, selected);
	}
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
				item.LocalTimeAsString.Format(L"%02d:%02d:%02d.%03d%03d", 
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, ft.dwLowDateTime * 10 % 1000);

			}
			return item.LocalTimeAsString;

		case ColumnType::ProcessId: text.Format(L"%u", item.Process.ProcessId); break;
		case ColumnType::ProcessName: return item.ProcessName;
		case ColumnType::Comment: return item.Comment;
	}
	return text;
}

PCWSTR CDebugView::GetExistingColumnText(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	if (GetColumnManager(h)->GetColumnTag<ColumnType>(col) == ColumnType::Text)
		return item->Text;

	return nullptr;
}

bool CDebugView::IsSortable(HWND, int col) const {
	return !AppSettings::Get().Capture();
}

BOOL CDebugView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) {
	if (row < 0)
		return FALSE;

	ShowProperties(row);
	return TRUE;
}

BOOL CDebugView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	if (row < 0)
		return FALSE;

	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	return m_pFrame->TrackPopupMenu(menu.GetSubMenu(0), 0, pt.x, pt.y);
}

void CDebugView::DebugOutput(DWORD pid, PCSTR text, FILETIME const& time, DebugOutputFlags flags) {
	auto& pm = ProcessManager::Get();
	auto item = std::make_unique<DebugItem>();
	item->ProcessName = pm.GetProcessName(pid);
	item->Process = pm.GetProcessKey(pid);
	item->Text = text;
	item->SystemTime = time;
	item->Flags = flags;
	item->Index = InterlockedIncrement(&s_Index);
	item->Image = pid <= 4 ? 0 : ImageIconCache::Get().GetIcon(pm.GetProcessInfo(item->Process)->FullPath);
	std::lock_guard locker(m_Lock);
	m_TempItems.push_back(std::move(item));
}

void CDebugView::DoSort(SortInfo* const si) {
	if (si == nullptr)
		return;

	auto col = GetColumnManager(si->hWnd)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;
	auto compare = [&](auto const& item1, auto const& item2) {
		switch (col) {
			case ColumnType::Index: return SortHelper::Sort(item1->Index, item2->Index, asc);
			case ColumnType::ProcessId: return SortHelper::Sort(item1->Process.ProcessId, item2->Process.ProcessId, asc);
			case ColumnType::ProcessName: return SortHelper::Sort(item1->ProcessName, item2->ProcessName, asc);
			case ColumnType::Time: return SortHelper::Sort(*(LONG64*)&item1->SystemTime, *(LONG64*)&item2->SystemTime, asc);
			case ColumnType::Text: return SortHelper::Sort(item1->Text, item2->Text, asc);
			case ColumnType::Comment: return SortHelper::Sort(item1->Comment, item2->Comment, asc);
		};
		return false;
	};

	std::lock_guard locker(m_Lock);
	std::sort(m_Items.begin(), m_Items.end(), compare);
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

void CDebugView::UpdateUI(CUpdateUIBase* ui) {
	if (m_ui == nullptr)
		m_ui = ui;

	ui->UIEnable(ID_VIEW_PROPERTIES, m_List.GetSelectedCount() == 1);
	ui->UIEnable(ID_EDIT_DELETE, m_List.GetSelectedCount() > 0);
	ui->UIEnable(ID_EDIT_COPY, m_List.GetSelectedCount() > 0);
}

int CDebugView::GetRowImage(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	if (col == 0)
		return ((item->Flags & DebugOutputFlags::Kernel) == DebugOutputFlags::Kernel) ? 1 : 2;

	auto type = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	if (type != ColumnType::ProcessName)
		return -1;

	if (item->Image >= 0)
		return item->Image;
	return item->Process.ProcessId ? 0 : 1;
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

LRESULT CDebugView::OnSaveAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(FALSE, L"txt", L"log", OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT,
		L"Text Files (*.txt)\0*.txt\0All Files\0*.*\0", m_hWnd);
	auto columns = m_List.GetHeader().GetItemCount();
	if (dlg.DoModal() == IDOK) {
		CString text;
		for (int i = 0; i < m_List.GetItemCount(); i++) {
			CString item;
			for (int c = 0; c < columns; c++) {
				if (m_List.GetItemText(i, c, item)) {
					item.Trim(L"\n\r");
					text += item;
				}
				if (c < columns - 1)
					text += L",";
			}
			text += L"\n";
		}
		wil::unique_hfile hFile(::CreateFile(dlg.m_szFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
		if (!hFile) {
			Helpers::ReportError(L"Error: ");
			return 0;
		}
		DWORD bytes;
		if (!::WriteFile(hFile.get(), text.GetBuffer(), text.GetLength() * sizeof(WCHAR), &bytes, nullptr)) {
			Helpers::ReportError(L"Error: ");
			return 0;
		}
		AtlMessageBox(m_hWnd, L"Saved successfully.", IDS_TITLE, MB_ICONINFORMATION);
	}
	return 0;
}

LRESULT CDebugView::OnProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto selected = m_List.GetSelectionMark();
	ATLASSERT(selected >= 0);
	if (selected < 0)
		return 0;

	ShowProperties(selected);

	return 0;
}

LRESULT CDebugView::OnItemChanged(int /*idCtrl*/, LPNMHDR hdr, BOOL& /*bHandled*/) {
	if (m_ui) {
		UpdateUI(m_ui);
	}
	return 0;
}

LRESULT CDebugView::OnEditDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int n = -1;
	int offset = 0;
	{
		std::lock_guard locker(m_Lock);
		while ((n = m_List.GetNextItem(n, LVIS_SELECTED)) != -1) {
			m_Items.erase(m_Items.begin() + n - offset);
			offset++;
		}
	}
	m_List.SelectAllItems(false);
	UpdateList();
	return 0;
}

LRESULT CDebugView::OnEditComment(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	ATLASSERT(m_List.GetSelectedCount() == 1);
	int selected = m_List.GetSelectionMark();
	ATLASSERT(selected >= 0);

	auto& item = m_Items[selected];
	CCommentDlg dlg(item->Comment);
	if (IDOK == dlg.DoModal(m_hWnd, reinterpret_cast<LPARAM>(CImageList(ImageIconCache::Get().GetImageList()).GetIcon(item->Image)))) {
		item->Comment = dlg.GetComment();
		m_List.RedrawItems(selected, selected);
	}
	return 0;
}

LRESULT CDebugView::OnEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	{
		std::lock_guard locker(m_Lock);
		m_Items.clear();
	}
	m_List.SetItemCount(0);
	return 0;
}

