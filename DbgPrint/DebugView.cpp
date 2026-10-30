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
#include <WTLHelper.h>
#include "resource.h"
#include "DebugLogPersist.h"
#include "HighlightDlg.h"
#include <shlwapi.h>

namespace {
	//
	// messages are 8-bit text: use UTF-8 if the text is valid UTF-8, otherwise the ANSI code page
	//
	std::wstring ToWide(PCSTR text) {
		int len = (int)strlen(text);
		if (len == 0)
			return {};

		UINT codePage = CP_UTF8;
		DWORD flags = MB_ERR_INVALID_CHARS;
		int count = ::MultiByteToWideChar(codePage, flags, text, len, nullptr, 0);
		if (count == 0) {
			codePage = CP_ACP;
			flags = 0;
			count = ::MultiByteToWideChar(codePage, flags, text, len, nullptr, 0);
		}
		std::wstring result(count, L'\0');
		::MultiByteToWideChar(codePage, flags, text, len, result.data(), count);
		return result;
	}
}

CDebugView::CDebugView(IMainFrame* frame, bool realTime) : m_pFrame(frame), m_pm(realTime), m_RealTime(realTime) {
}

bool CDebugView::Load(PCWSTR path) {
	ATLASSERT(!m_RealTime);

	std::vector<std::shared_ptr<DebugItem>> items;
	if (!DebugLogPersist::Load(path, items, m_IconCache, m_pm))
		return false;

	{
		std::lock_guard locker(m_Lock);
		m_Items.clear();
		m_Items.append(items.begin(), items.end());
	}
	UpdateList();
	return true;
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

	if (!s_HighlightsLoaded) {
		s_Highlights = AppSettings::ParseHighlights(AppSettings::Get().HighlightRules());
		s_HighlightsLoaded = true;
	}

	m_TempItems.reserve(256);
	m_Items.reserve(4096);

	SetTimer(1, 1000);

	return 0;
}

LRESULT CDebugView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1) {
		std::vector<std::shared_ptr<DebugItem>> items;
		{
			std::lock_guard locker(m_Lock);
			if (m_TempItems.empty())
				return 0;
			items.swap(m_TempItems);
		}

		//
		// icons are extracted here rather than on the capture threads, which writers of debug output wait for
		//
		for (auto& item : items) {
			auto pi = item->Process.ProcessId <= 4 ? nullptr : m_pm.GetProcessInfo(item->Process);
			item->Image = pi ? m_IconCache.GetIcon(pi->FullPath) : 0;
		}

		{
			std::lock_guard locker(m_Lock);
			m_Items.append(items.begin(), items.end());
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
		case ColumnType::Time: return item.LocalTime();

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
	//
	// a log loaded from a file does not change, so it can be sorted even while capturing
	//
	return !m_RealTime || !AppSettings::Get().Capture();
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
	auto row = (int)cd->dwItemSpec;
	if (cd->hdr.hwndFrom != m_List || row >= (int)m_Items.size())
		return CDRF_DODEFAULT;

	//
	// the selection keeps its own colors, so it stays visible
	//
	if (m_List.GetItemState(row, LVIS_SELECTED) == LVIS_SELECTED)
		return CDRF_DODEFAULT;

	auto rule = FindHighlight(*m_Items[row]);
	if (!rule)
		return CDRF_DODEFAULT;

	auto lvcd = (NMLVCUSTOMDRAW*)cd;
	lvcd->clrTextBk = WTLHelper::IsDarkMode() ? rule->Dark : rule->Light;
	//
	// black or white text, whichever reads better on the background
	//
	auto bk = lvcd->clrTextBk;
	auto luminance = (GetRValue(bk) * 299 + GetGValue(bk) * 587 + GetBValue(bk) * 114) / 1000;
	lvcd->clrText = luminance > 128 ? RGB(0, 0, 0) : RGB(255, 255, 255);
	return CDRF_NEWFONT;
}

HighlightItem const* CDebugView::FindHighlight(DebugItem const& item) {
	for (auto& rule : s_Highlights)
		if (!rule.Text.empty() && ::StrStrIW(item.Text.c_str(), rule.Text.c_str()))
			return &rule;
	return nullptr;
}

void CDebugView::OnFinalMessage(HWND) {
	//
	// views are created with new by the main frame, and destroyed with their tab or the frame
	//
	delete this;
}

bool CDebugView::IsRealTime() const {
	return m_RealTime;
}

bool CDebugView::IsEmpty() const {
	return m_Items.empty();
}

void CDebugView::DebugOutput(DWORD pid, PCSTR text, FILETIME const& time, DebugOutputFlags flags, std::vector<ULONG64> stack) {
	if (!m_Running)
		return;

	auto item = std::make_unique<DebugItem>();
	item->ProcessName = m_pm.GetProcessName(pid);
	item->Process = m_pm.GetProcessKey(pid);
	item->Text = ToWide(text);
	item->SystemTime = time;
	item->Flags = flags;
	item->Stack = std::move(stack);
	item->Index = InterlockedIncrement(&s_Index);
	//
	// the process is looked up now, while it's still running; its icon is extracted later, on the UI thread (OnTimer)
	//
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

bool CDebugView::Capture(bool capture) {
	m_Running = capture;
	bool ok = true;
	if (capture) {
		auto const& settings = AppSettings::Get();
		if (settings.CaptureUserMode())
			m_UserMode.Run(this);
		if (settings.CaptureSession0())
			m_UserModeSession0.Run(this);
		if (settings.CaptureKernel()) {
			m_KernelMode.SetCaptureStacks(settings.CaptureStacks());
			ok = m_KernelMode.Run(this);
		}
	}
	else {
		m_UserMode.Stop();
		m_UserModeSession0.Stop();
		m_KernelMode.Stop();
	}
	return ok;
}

bool CDebugView::CaptureKernel(bool capture) {
	if (!m_Running)
		return true;
	if (capture) {
		m_KernelMode.SetCaptureStacks(AppSettings::Get().CaptureStacks());
		return m_KernelMode.Run(this);
	}
	m_KernelMode.Stop();
	return true;
}

bool CDebugView::CaptureStacks(bool capture) {
	m_KernelMode.SetCaptureStacks(capture);
	if (m_Running && m_KernelMode.IsRunning()) {
		//
		// stack tracing is configured when the session starts
		//
		m_KernelMode.Stop();
		return m_KernelMode.Run(this);
	}
	return true;
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

void CDebugView::SetFilter(CString const& text) {
	if (text == m_FilterText)
		return;

	m_FilterText = text;
	{
		std::lock_guard locker(m_Lock);
		if (text.IsEmpty())
			m_Items.Filter(nullptr);
		else
			m_Items.Filter([text](auto const& item, size_t) {
				return ::StrStrIW(item->Text.c_str(), text) != nullptr
					|| ::StrStrIW(item->ProcessName.c_str(), text) != nullptr;
			});
	}
	DoSort(GetSortInfo(m_List));

	m_List.SelectAllItems(false);
	m_List.SetItemCount((int)m_Items.size());
	if (AppSettings::Get().AutoScroll())
		m_List.EnsureVisible(m_List.GetItemCount() - 1, FALSE);
	m_List.Invalidate();
}

CString const& CDebugView::GetFilter() const {
	return m_FilterText;
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
	m_KernelMode.Stop();
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
	WTLHelper::SuspendHook();
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if(ok) {
		auto text = ListViewHelper::GetAllRowsAsString(m_List, L",");
		wil::unique_hfile hFile(::CreateFile(dlg.m_szFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
		if (!hFile) {
			Helpers::ReportError(L"Error: ");
			return 0;
		}
		//
		// UTF-16 with a byte order mark, so editors detect the encoding
		//
		const WCHAR bom = 0xFEFF;
		DWORD bytes;
		if (!::WriteFile(hFile.get(), &bom, sizeof(bom), &bytes, nullptr) ||
			!::WriteFile(hFile.get(), (PCWSTR)text, text.GetLength() * sizeof(WCHAR), &bytes, nullptr)) {
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
	wil::unique_hicon hIcon(CImageList(m_IconCache.GetImageList()).GetIcon(item->Image));
	if (IDOK == dlg.DoModal(m_hWnd, reinterpret_cast<LPARAM>(hIcon.get()))) {
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
	int count = m_List.GetItemCount();
	int n = m_List.GetSelectionMark();	// -1 if nothing is selected, so the search starts at the first item
	int next = -1;
	for (int i = 1; i <= count; i++) {
		int index = (n + i) % count;
		if ((m_Items[index]->Flags & DebugOutputFlags::Bookmark) == DebugOutputFlags::Bookmark) {
			next = index;
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
	int count = m_List.GetItemCount();
	int n = m_List.GetSelectionMark();
	if (n < 0)
		n = count;	// nothing selected: start at the last item
	int next = -1;
	for (int i = 1; i <= count; i++) {
		int index = (n - i % count + count) % count;
		if ((m_Items[index]->Flags & DebugOutputFlags::Bookmark) == DebugOutputFlags::Bookmark) {
			next = index;
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
	WTLHelper::SuspendHook();
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
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
	CHighlightDlg dlg(s_Highlights);
	if (IDOK == dlg.DoModal()) {
		//
		// the rules are shared by all views; the others repaint when their tab is shown
		//
		s_Highlights = dlg.GetItems();
		AppSettings::Get().HighlightRules(AppSettings::FormatHighlights(s_Highlights));
		m_List.Invalidate();
	}
	return 0;
}

LRESULT CDebugView::OnFindNext(WORD, WORD, HWND, BOOL&) {
	return SendMessage(CFindReplaceDialog::GetFindReplaceMsg());
}


