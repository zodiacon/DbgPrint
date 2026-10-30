#include "pch.h"
#include "SymbolSettingsDlg.h"
#include "AppSettings.h"
#include <WTLHelper.h>

std::wstring CSymbolSettingsDlg::GetText(UINT id) const {
	CString text;
	GetDlgItemText(id, text);
	return (PCWSTR)text.Trim();
}

void CSymbolSettingsDlg::Load(bool useServer, std::wstring const& url, std::wstring const& cache, std::wstring const& extra, bool useEnv) {
	CheckDlgButton(IDC_SYM_USE_SERVER, useServer ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_SYM_SERVER_URL, url.c_str());
	SetDlgItemText(IDC_SYM_CACHE, cache.c_str());
	SetDlgItemText(IDC_SYM_EXTRA, extra.c_str());
	CheckDlgButton(IDC_SYM_USE_ENV, useEnv ? BST_CHECKED : BST_UNCHECKED);
	UpdateUI();
}

void CSymbolSettingsDlg::UpdateUI() {
	bool server = IsDlgButtonChecked(IDC_SYM_USE_SERVER) == BST_CHECKED;
	for (auto id : { IDC_SYM_SERVER_URL, IDC_SYM_CACHE, IDC_SYM_BROWSE })
		GetDlgItem(id).EnableWindow(server);

	auto path = AppSettings::BuildSymbolSearchPath(server, GetText(IDC_SYM_SERVER_URL), GetText(IDC_SYM_CACHE),
		GetText(IDC_SYM_EXTRA), IsDlgButtonChecked(IDC_SYM_USE_ENV) == BST_CHECKED);
	SetDlgItemText(IDC_SYM_PREVIEW, path.empty() ? L"(none: only the PDB location recorded in each module)" : path.c_str());
}

LRESULT CSymbolSettingsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetDialogIcon(IDR_MAINFRAME);
	CenterWindow(GetParent());

	auto& s = AppSettings::Get();
	Load(s.SymbolServerEnabled(), s.SymbolServerUrl(), s.SymbolCache(), s.SymbolExtraPaths(), s.SymbolUseEnvPath());
	return TRUE;
}

LRESULT CSymbolSettingsDlg::OnOK(WORD, WORD wID, HWND, BOOL&) {
	bool server = IsDlgButtonChecked(IDC_SYM_USE_SERVER) == BST_CHECKED;
	auto url = GetText(IDC_SYM_SERVER_URL);
	if (server && _wcsnicmp(url.c_str(), L"http://", 7) != 0 && _wcsnicmp(url.c_str(), L"https://", 8) != 0) {
		AtlMessageBox(m_hWnd, L"The symbol server URL must start with http:// or https://", IDS_TITLE, MB_ICONWARNING);
		GotoDlgCtrl(GetDlgItem(IDC_SYM_SERVER_URL));
		return 0;
	}

	auto& s = AppSettings::Get();
	s.SymbolServerEnabled(server);
	s.SymbolServerUrl(url);
	s.SymbolCache(GetText(IDC_SYM_CACHE));
	s.SymbolExtraPaths(GetText(IDC_SYM_EXTRA));
	s.SymbolUseEnvPath(IsDlgButtonChecked(IDC_SYM_USE_ENV) == BST_CHECKED);
	EndDialog(wID);
	return 0;
}

LRESULT CSymbolSettingsDlg::OnCancel(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

LRESULT CSymbolSettingsDlg::OnBrowse(WORD, WORD, HWND, BOOL&) {
	CShellFileOpenDialog dlg(nullptr, FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal(m_hWnd) == IDOK;
	WTLHelper::ResumeHook();
	if (ok) {
		CString path;
		if (SUCCEEDED(dlg.GetFilePath(path)))
			SetDlgItemText(IDC_SYM_CACHE, path);
	}
	return 0;
}

LRESULT CSymbolSettingsDlg::OnDefaults(WORD, WORD, HWND, BOOL&) {
	Load(true, AppSettings::DefaultSymbolServerUrl, AppSettings::DefaultSymbolCache, L"", true);
	return 0;
}

LRESULT CSymbolSettingsDlg::OnChanged(WORD, WORD, HWND, BOOL&) {
	UpdateUI();
	return 0;
}
