#include "pch.h"
#include "DebugView.h"
#include "SortHelper.h"
#include "SecurityHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"
#include "AppSettings.h"
#include "Helpers.h"
#include "PropertiesDlg.h"
#include "CommentDlg.h"
#include <ThemeHelper.h>
#include "resource.h"
#include "DebugLogPersist.h"
#include "HighlightDlg.h"

CDebugView::CDebugView(IMainFrame* frame, bool realTime) : m_pFrame(frame), m_RealTime(realTime) {
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
		LVS_OWNERDATA | LVS_REPORT);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_SUBITEMIMAGES);
	m_List.SetImageList(m_IconCache.GetImageList(), LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"", LVCFMT_RIGHT, 1);
	cm->AddColumn(L"#", LVCFMT_RIGHT, 70, ColumnType::Index);
	cm->AddColumn(L"Time", LVCFMT_RIGHT, 110, ColumnType::Time);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 80, ColumnType::ProcessId);
	cm->AddColumn(L"Process Name", LVCFMT_LEFT, 180, ColumnType::ProcessName);
	cm->AddColumn(L"Message", LVCFMT_LEFT, 500, ColumnType::Text);
	cm->AddColumn(L"Comment", LVCFMT_LEFT, 150, ColumnType::Comment);
	cm->UpdateColumns();
	cm->DeleteColumn(0);

	m_Highlights = AppSettings::Get().HighlightItems();
	if (m_Highlights.empty())
		m_Highlights = DefaultHighlightColors;

	m_TempItems.reserve(256);
	m_Items.reserve(4096);

	SetTimer(1, 1000);

	return 0;
}

LRESULT CDebugView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1) {
		{
			std::lock_guard locker(m_Lock);
			if (m_TempItems.empty())
				return 0;

			m_Items.append(m_TempItems.begin(), m_TempItems.end());
			m_TempItems.clear();
		}
		UpdateList();
	}
	return 0;
}

void CDebugView::ShowProperties(int selected) {
	auto& item = *m_Items[selected];
	CPropertiesDlg dlg(item, m_IconCache, m_pm);
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
		case ColumnType::ProcessName: return item.ProcessName.c_str();
		case ColumnType::Comment: return item.Comment.c_str();
	}
	return text;
}

PCWSTR CDebugView::GetExistingColumnText(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	if (GetColumnManager(h)->GetColumnTag<ColumnType>(col) == ColumnType::Text)
		return item->Text.c_str();

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

DWORD CDebugView::OnPrePaint(DWORD, LPNMCUSTOMDRAW cd) {
	if (cd->hdr.hwndFrom == m_List)
		return CDRF_NOTIFYITEMDRAW;

	SetMsgHandled(FALSE);
	return 0;
}

DWORD CDebugView::OnItemPrePaint(DWORD, LPNMCUSTOMDRAW cd) {
	auto lvcd = (NMLVCUSTOMDRAW*)cd;
	auto& item = m_Items[(int)cd->dwItemSpec];
	if ((item->Flags & DebugOutputFlags::Highlight) == DebugOutputFlags::Highlight) {

	}
	return CDRF_SKIPPOSTPAINT;
}

bool CDebugView::CanClose() {
	return !m_RealTime;
}

bool CDebugView::IsRealTime() const {
	return m_RealTime;
}

bool CDebugView::IsEmpty() const {
	return m_Items.empty();
}

void CDebugView::DebugOutput(DWORD pid, PCSTR text, FILETIME const& time, DebugOutputFlags flags) {
	if (!m_Running)
		return;

	auto item = std::make_unique<DebugItem>();
	item->ProcessName = m_pm.GetProcessName(pid);
	item->Process = m_pm.GetProcessKey(pid);
	item->Text.assign(text, text + strlen(text) + 1);
	item->SystemTime = time;
	item->Flags = flags;
	item->Index = InterlockedIncrement(&s_Index);
	item->Image = pid <= 4 ? 0 : m_IconCache.GetIcon(m_pm.GetProcessInfo(item->Process)->FullPath);
	std::lock_guard locker(m_Lock);
	m_TempItems.push_back(std::move(item));
}

void CDebugView::DoSort(SortInfo const* si) {
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
	m_Items.Sort(compare);
}

void CDebugView::Capture(bool capture) {
	m_Running = capture;
	if (capture) {
		auto const& settings = AppSettings::Get();
		if (settings.CaptureUserMode())
			m_UserMode.Run(this);
		if (settings.CaptureSession0())
			m_UserModeSession0.Run(this);
		if (settings.CaptureKernel())
			m_KernelMode.Run(this);
	}
	else {
		m_UserMode.Stop();
		m_UserModeSession0.Stop();
		m_KernelMode.Stop();
	}
}

void CDebugView::CaptureKernel(bool capture) {
	if (!m_Running)
		return;
	capture ? m_KernelMode.Run(this) : m_KernelMode.Stop();
}

void CDebugView::CaptureUser(bool capture) {
	if (!m_Running)
		return;
	capture ? m_UserMode.Run(this) : m_UserMode.Stop();
}

void CDebugView::CaptureSession0(bool capture) {
	if (!m_Running)
		return;
	capture ? m_UserModeSession0.Run(this) : m_UserModeSession0.Stop();
}

void CDebugView::UpdateUI(CUpdateUIBase* ui) {
	if (m_ui == nullptr)
		m_ui = ui;

	auto selectedCount = m_List.GetSelectedCount();
	ui->UIEnable(ID_VIEW_PROPERTIES, selectedCount == 1);
	ui->UIEnable(ID_EDIT_DELETE, selectedCount > 0);
	ui->UIEnable(ID_EDIT_COPY, selectedCount > 0);
	ui->UIEnable(ID_EDIT_COMMENT, selectedCount == 1);
	ui->UIEnable(ID_EDIT_BOOKMARK, selectedCount > 0);
	ui->UIEnable(ID_SEARCH_FIND, m_List.GetItemCount() > 0);
	ui->UIEnable(ID_SEARCH_FINDNEXT, !m_pFrame->GetSearchString().IsEmpty());
}

int CDebugView::GetRowImage(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	if (col == 0)
		return ((item->Flags & DebugOutputFlags::Kernel) == DebugOutputFlags::Kernel) ? 1 : 2;

	auto type = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	switch (type) {
		case ColumnType::ProcessName:
			if (item->Image >= 0)
				return item->Image;
			return item->Process.ProcessId ? 0 : 1;

		case ColumnType::Text:
			auto bookmark = (item->Flags & DebugOutputFlags::Bookmark) == DebugOutputFlags::Bookmark;
			if (bookmark)
				return 3;
			break;
	}
	return -1;
}

LRESULT CDebugView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& handled) {
	KillTimer(1);
	m_UserMode.Stop();
	m_UserModeSession0.Stop();
	handled = FALSE;
	return 0;
}

LRESULT CDebugView::OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List, L","));
	return 0;
}

LRESULT CDebugView::OnSetFont(UINT, WPARAM wp, LPARAM, BOOL&) {
	m_List.SetFont((HFONT)wp);
	return 0;
}

LRESULT CDebugView::OnSaveAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(FALSE, L"txt", L"log", OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT,
		L"Text Files (*.txt)\0*.txt\0All Files\0*.*\0", m_hWnd);
	ThemeHelper::Suspend();
	auto ok = IDOK == dlg.DoModal();
	ThemeHelper::Resume();
	if(ok) {
		auto text = ListViewHelper::GetAllRowsAsString(m_List, L",");
		wil::unique_hfile hFile(::CreateFile(dlg.m_szFileName, GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, 0, nullptr));
		if (!hFile) {
			Helpers::ReportError(L"Error: ");
			return 0;
		}
		if (DWORD bytes; !::WriteFile(hFile.get(), text.GetBuffer(), text.GetLength() * sizeof(WCHAR), &bytes, nullptr)) {
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
			m_Items.erase(n - offset);
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
	if (IDOK == dlg.DoModal(m_hWnd, reinterpret_cast<LPARAM>(CImageList(m_IconCache.GetImageList()).GetIcon(item->Image)))) {
		item->Comment = dlg.GetComment();
		m_List.RedrawItems(selected, selected);
	}
	return 0;
}

LRESULT CDebugView::OnEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (AppSettings::Get().ConfirmErase()) {
		if (AtlMessageBox(m_hWnd, L"Erase log?", IDS_TITLE, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDNO)
			return 0;
	}
	{
		std::lock_guard locker(m_Lock);
		m_Items.clear();
	}
	m_List.SetItemCount(0);
	return 0;
}

LRESULT CDebugView::OnFind(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	auto dlg = m_pFrame->GetFindDlg();
	auto searchDown = dlg->SearchDown();
	auto index = ListViewHelper::SearchItem(m_List, dlg->GetFindString(), searchDown, dlg->MatchCase());

	if (index >= 0) {
		m_List.SelectItem(index);
		m_List.SetFocus();
	}
	else {
		AtlMessageBox(m_hWnd, L"Finished searching.", IDR_MAINFRAME, MB_ICONINFORMATION);
	}
	return 0;
}

LRESULT CDebugView::OnToggleBookmark(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int count = m_List.GetSelectedCount();
	ATLASSERT(count);

	for (auto i : SelectedItemsView(m_List)) {
		auto& item = m_Items[i];
		item->Flags ^= DebugOutputFlags::Bookmark;
	}
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	return 0;
}

LRESULT CDebugView::OnNextBookmark(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int n = m_List.GetSelectionMark();
	int count = m_List.GetItemCount();
	int next = -1;
	for (int i = n + 1; i < count + n; i++) {
		if ((m_Items[i % count]->Flags & DebugOutputFlags::Bookmark) == DebugOutputFlags::Bookmark) {
			next = i % count;
			break;
		}
	}
	if (next >= 0)
		m_List.SelectItem(next);
	else
		::MessageBeep(-1);
	return 0;
}

LRESULT CDebugView::OnPrevBookmark(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int n = m_List.GetSelectionMark();
	int count = m_List.GetItemCount();
	int next = -1;
	for (int i = n - 1 + count; i >= n; i--) {
		if ((m_Items[i % count]->Flags & DebugOutputFlags::Bookmark) == DebugOutputFlags::Bookmark) {
			next = i % count;
			break;
		}
	}
	if (next >= 0)
		m_List.SelectItem(next);
	else
		::MessageBeep(-1);
	return 0;
}

LRESULT CDebugView::OnDeleteAllBookmarks(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	for (auto& item : m_Items)
		item->Flags &= ~DebugOutputFlags::Bookmark;

	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	return 0;
}

LRESULT CDebugView::OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(FALSE, L"dbgp", L"log", OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT,
		L"DebugPrint Native Files (*.dbgp)\0*.dbgp\0CSV Files (*.csv)\0*.csv\0", m_hWnd);
	ThemeHelper::Suspend();
	auto ok = IDOK == dlg.DoModal();
	ThemeHelper::Resume();
	if (ok) {
		auto ext = wcsrchr(dlg.m_szFileTitle, L'.');
		auto format = ext && _wcsicmp(ext, L".dbgp") == 0 ? PersistFormat::Native : PersistFormat::CSV;
		ok = DebugLogPersist::Save(format, m_Items.GetItems(), m_IconCache, m_pm, dlg.m_szFileName);
		if (ok)
			AtlMessageBox(m_hWnd, L"Saved successfully.", IDS_TITLE, MB_ICONINFORMATION);
		else
			AtlMessageBox(m_hWnd, L"Error saving log.", IDS_TITLE, MB_ICONERROR);
	}
	return 0;
}

LRESULT CDebugView::OnHighlight(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CHighlightDlg dlg(m_Highlights);
	if (IDOK == dlg.DoModal()) {
	}
	return 0;
}

LRESULT CDebugView::OnFindNext(WORD, WORD, HWND, BOOL&) {
	return SendMessage(CFindReplaceDialog::GetFindReplaceMsg());
}


