#include "pch.h"
#include "resource.h"
#include "HighlightDlg.h"
#include "AppSettings.h"
#include <WTLHelper.h>
#include <wil\resource.h>

namespace {
	// colors given to new rules in turn: light backgrounds for light mode
	const COLORREF NewRuleColors[] = {
		RGB(255, 255, 128), RGB(160, 255, 160), RGB(255, 180, 180),
		RGB(170, 210, 255), RGB(255, 200, 120), RGB(220, 180, 255),
	};

	const WCHAR FileFilter[] = L"Highlight Rules (*.dbgh)\0*.dbgh\0All Files\0*.*\0";
}

CHighlightDlg::CHighlightDlg(std::vector<HighlightItem> items) : m_Items(std::move(items)) {
}

std::vector<HighlightItem> const& CHighlightDlg::GetItems() const {
	return m_Items;
}

COLORREF CHighlightDlg::DarkVariant(COLORREF light) {
	return RGB(GetRValue(light) * 2 / 5, GetGValue(light) * 2 / 5, GetBValue(light) * 2 / 5);
}

CString CHighlightDlg::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Items[row];
	switch (col) {
		case 1: return item.Text.c_str();
	}
	return L"";
}

int CHighlightDlg::OnPrePaint(int, LPNMCUSTOMDRAW cd) {
	if (cd->hdr.hwndFrom != m_List) {
		SetMsgHandled(FALSE);
		return 0;
	}

	return CDRF_NOTIFYITEMDRAW;
}

int CHighlightDlg::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	if (cd->hdr.hwndFrom != m_List) {
		SetMsgHandled(FALSE);
		return 0;
	}
	return CDRF_NOTIFYSUBITEMDRAW;
}

int CHighlightDlg::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	auto lv = (NMLVCUSTOMDRAW*)cd;
	if (lv->iSubItem == 0 && cd->dwItemSpec < m_Items.size()) {
		auto& item = m_Items[cd->dwItemSpec];
		CRect rc(cd->rc);
		CDCHandle dc(cd->hdc);
		rc.DeflateRect(2, 2);
		dc.FillSolidRect(&rc, WTLHelper::IsDarkMode() ? item.Dark : item.Light);
	}
	return CDRF_SKIPPOSTPAINT;
}

LRESULT CHighlightDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	AdjustOKCancelButtons(IDI_OK, IDI_CANCEL);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Color", 0, 80);
	cm->AddColumn(L"Text", 0, 200);
	cm->UpdateColumns();

	Refresh();
	if (!m_Items.empty())
		Select(0);
	UpdateButtons();
	return 0;
}

LRESULT CHighlightDlg::OnCloseCmd(WORD, WORD id, HWND, BOOL&) {
	if (id == IDOK) {
		//
		// a rule with no text never matches
		//
		std::erase_if(m_Items, [](auto const& item) { return item.Text.empty(); });
	}
	EndDialog(id);
	return 0;
}

int CHighlightDlg::GetSelected() const {
	return m_List.GetNextItem(-1, LVNI_SELECTED);
}

void CHighlightDlg::Select(int index) {
	m_List.SelectItem(index);
	m_List.EnsureVisible(index, FALSE);
}

void CHighlightDlg::Refresh() {
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	m_List.Invalidate();
}

void CHighlightDlg::UpdateButtons() {
	bool selected = GetSelected() >= 0;
	GetDlgItem(IDC_DELETE).EnableWindow(selected);
	GetDlgItem(IDC_COLOR).EnableWindow(selected);
	GetDlgItem(IDC_TEXT).EnableWindow(selected);
	GetDlgItem(IDC_SAVE).EnableWindow(!m_Items.empty());
	GetDlgItem(IDC_RESET).EnableWindow(!m_Items.empty());
}

LRESULT CHighlightDlg::OnItemChanged(int, LPNMHDR, BOOL& handled) {
	//
	// the edit box shows (and edits) the selected rule's text
	//
	int selected = GetSelected();
	m_Updating = true;
	SetDlgItemText(IDC_TEXT, selected >= 0 ? m_Items[selected].Text.c_str() : L"");
	m_Updating = false;
	UpdateButtons();
	handled = FALSE;	// CVirtualListView tracks the state too
	return 0;
}

LRESULT CHighlightDlg::OnTextChanged(WORD, WORD, HWND, BOOL&) {
	int selected = GetSelected();
	if (m_Updating || selected < 0)
		return 0;

	CString text;
	GetDlgItemText(IDC_TEXT, text);
	m_Items[selected].Text = text;
	m_List.RedrawItems(selected, selected);
	return 0;
}

LRESULT CHighlightDlg::OnAdd(WORD, WORD, HWND, BOOL&) {
	auto light = NewRuleColors[m_Items.size() % _countof(NewRuleColors)];
	m_Items.push_back({ light, DarkVariant(light), L"" });
	Refresh();
	Select((int)m_Items.size() - 1);
	UpdateButtons();
	GotoDlgCtrl(GetDlgItem(IDC_TEXT));
	return 0;
}

LRESULT CHighlightDlg::OnDelete(WORD, WORD, HWND, BOOL&) {
	int selected = GetSelected();
	if (selected < 0)
		return 0;

	m_Items.erase(m_Items.begin() + selected);
	Refresh();
	if (!m_Items.empty())
		Select(std::min<int>(selected, (int)m_Items.size() - 1));
	else
		SetDlgItemText(IDC_TEXT, L"");
	UpdateButtons();
	return 0;
}

LRESULT CHighlightDlg::OnColor(WORD, WORD, HWND, BOOL&) {
	int selected = GetSelected();
	if (selected < 0)
		return 0;

	auto& item = m_Items[selected];
	CColorDialog dlg(item.Light, CC_FULLOPEN, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal() == IDOK;
	WTLHelper::ResumeHook();
	if (ok) {
		item.Light = dlg.GetColor();
		item.Dark = DarkVariant(item.Light);
		m_List.RedrawItems(selected, selected);
	}
	return 0;
}

LRESULT CHighlightDlg::OnReset(WORD, WORD, HWND, BOOL&) {
	if (AtlMessageBox(m_hWnd, L"Remove all highlight rules?", IDS_TITLE, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDNO)
		return 0;

	m_Items.clear();
	Refresh();
	SetDlgItemText(IDC_TEXT, L"");
	UpdateButtons();
	return 0;
}

LRESULT CHighlightDlg::OnSave(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(FALSE, L"dbgh", L"highlights", OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT, FileFilter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal() == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	//
	// UTF-8 with a byte order mark, in the same format as the setting
	//
	CStringA utf8(CW2A(AppSettings::FormatHighlights(m_Items).c_str(), CP_UTF8));
	wil::unique_hfile hFile(::CreateFile(dlg.m_szFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	const BYTE bom[] = { 0xEF, 0xBB, 0xBF };
	DWORD bytes;
	if (!hFile || !::WriteFile(hFile.get(), bom, sizeof(bom), &bytes, nullptr) ||
		!::WriteFile(hFile.get(), (PCSTR)utf8, utf8.GetLength(), &bytes, nullptr))
		AtlMessageBox(m_hWnd, L"Error saving highlight rules.", IDS_TITLE, MB_ICONERROR);
	return 0;
}

LRESULT CHighlightDlg::OnLoad(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(TRUE, L"dbgh", nullptr, OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST, FileFilter, m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal() == IDOK;
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	wil::unique_hfile hFile(::CreateFile(dlg.m_szFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
	LARGE_INTEGER size{};
	if (!hFile || !::GetFileSizeEx(hFile.get(), &size) || size.QuadPart > (1 << 20)) {
		AtlMessageBox(m_hWnd, L"Error loading highlight rules.", IDS_TITLE, MB_ICONERROR);
		return 0;
	}
	std::string data((size_t)size.QuadPart, '\0');
	DWORD bytes = 0;
	if (!data.empty() && !::ReadFile(hFile.get(), data.data(), (DWORD)data.size(), &bytes, nullptr)) {
		AtlMessageBox(m_hWnd, L"Error loading highlight rules.", IDS_TITLE, MB_ICONERROR);
		return 0;
	}
	data.resize(bytes);
	if (data.starts_with("\xEF\xBB\xBF"))
		data.erase(0, 3);

	auto items = AppSettings::ParseHighlights(std::wstring(CA2W(data.c_str(), CP_UTF8)));
	if (items.empty()) {
		AtlMessageBox(m_hWnd, L"The file has no highlight rules.", IDS_TITLE, MB_ICONWARNING);
		return 0;
	}
	m_Items = std::move(items);
	Refresh();
	Select(0);
	UpdateButtons();
	return 0;
}
