// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "MainFrm.h"
#include "IconHelper.h"
#include "SecurityHelper.h"
#include "AppSettings.h"
#include "Helpers.h"
#include <ThemeHelper.h>

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

void CMainFrame::InitMenu() {
	AddMenu(GetMenu());
	struct {
		UINT id, icon;
		HICON hIcon = nullptr;
	} cmds[] = {
		{ ID_FILE_RUNASADMINISTRATOR, 0, IconHelper::GetShieldIcon() },
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_FIND, IDI_FIND },
		{ ID_CAPTURE_CAPTUREOUTPUT, IDI_PLAY },
		{ ID_CAPTURE_CAPTUREUSERMODE, IDI_USER },
		{ ID_CAPTURE_CAPTURESESSION0, IDI_USER0 },
		{ ID_CAPTURE_CAPTUREKERNEL, IDI_ATOM },
		{ ID_FILE_SAVE, IDI_SAVEAS },
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_VIEW_AUTOSCROLL, IDI_AUTOSCROLL },
		{ ID_EDIT_DELETE, IDI_CANCEL },
		{ ID_EDIT_CLEAR_ALL, IDI_ERASE },
		{ ID_EDIT_COMMENT, IDI_COMMENT },
		{ ID_EDIT_BOOKMARK, IDI_BOOKMARK },
		{ ID_VIEW_NEXTBOOKMARK, IDI_BOOKMARK_NEXT },
		{ ID_VIEW_PREVIOUSBOOKMARK, IDI_BOOKMARK_PREV },
	};
	for (auto& cmd : cmds) {
		if (cmd.icon)
			AddCommand(cmd.id, cmd.icon);
		else
			AddCommand(cmd.id, cmd.hIcon);
	}
}

void CMainFrame::InitToolBar(CToolBarCtrl& tb) const {
	const int size = 24;
	CImageList tbImages;
	tbImages.Create(size, size, ILC_COLOR32, 8, 4);
	tb.SetImageList(tbImages);

	const struct {
		UINT id;
		int image;
		BYTE style = BTNS_BUTTON;
		PCWSTR text = nullptr;
	} buttons[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ 0 },
		{ ID_CAPTURE_CAPTUREOUTPUT, IDI_PLAY },
		{ ID_VIEW_AUTOSCROLL, IDI_AUTOSCROLL },
		{ 0 },
		{ ID_CAPTURE_CAPTUREUSERMODE, IDI_USER },
		{ ID_CAPTURE_CAPTURESESSION0, IDI_USER0 },
		{ 0 },
		{ ID_CAPTURE_CAPTUREKERNEL, IDI_ATOM },
		{ 0 },
		{ ID_EDIT_CLEAR_ALL, IDI_ERASE },
		{ 0 },
		{ ID_EDIT_BOOKMARK, IDI_BOOKMARK },
		{ ID_VIEW_PREVIOUSBOOKMARK, IDI_BOOKMARK_PREV },
		{ ID_VIEW_NEXTBOOKMARK, IDI_BOOKMARK_NEXT },
	};
	for (auto& b : buttons) {
		if (b.id == 0)
			tb.AddSeparator(0);
		else {
			auto hIcon = AtlLoadIconImage(b.image, 0, size, size);
			ATLASSERT(hIcon);
			int image = tbImages.AddIcon(hIcon);
			tb.AddButton(b.id, b.style, TBSTATE_ENABLED, image, b.text, 0);
		}
	}
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	InitDarkTheme();
	if (SecurityHelper::IsRunningElevated()) {
		CMenuHandle menu(GetMenu());
		menu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
		menu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
	}
	SetCheckIcon(AtlLoadIconImage(IDI_OK));

	auto& settings = AppSettings::Get();
	settings.LoadFromKey(L"Software\\ScorpioSoftware\\DbgPrint");

	auto font = settings.Font();
	if (font.lfHeight == 0)
		m_Font.CreatePointFont(100, L"Segoe UI");
	else
		m_Font.CreateFontIndirect(&font);

	InitMenu();
	UIAddMenu(GetMenu());

	CToolBarCtrl tb;
	tb.Create(m_hWnd, nullptr, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE, 0, ATL_IDW_TOOLBAR);
	InitToolBar(tb);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(tb);

	CreateSimpleStatusBar();

	UIAddToolBar(tb);

	UISetCheck(ID_VIEW_TOOLBAR, settings.ViewToolBar());
	UISetCheck(ID_VIEW_STATUS_BAR, settings.ViewStatusBar());

	m_Tabs.m_bTabCloseButton = false;
	m_hWndClient = m_Tabs.Create(m_hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_Tabs.SetWindowMenu(CMenuHandle(GetMenu()).GetSubMenu(7));

	CImageList images;
	images.Create(16, 16, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 4, 4);
	images.AddIcon(AtlLoadIconImage(IDI_SCRIPT, 0, 16, 16));
	images.AddIcon(AtlLoadIconImage(IDI_SCRIPT_FILE, 0, 16, 16));
	m_Tabs.SetImageList(images);

	UISetCheck(ID_CAPTURE_CAPTUREOUTPUT, settings.Capture());
	UISetCheck(ID_CAPTURE_CAPTUREUSERMODE, settings.CaptureUserMode());
	UISetCheck(ID_CAPTURE_CAPTURESESSION0, settings.CaptureSession0());
	UISetCheck(ID_CAPTURE_CAPTUREKERNEL, settings.CaptureKernel());
	UISetCheck(ID_VIEW_AUTOSCROLL, settings.AutoScroll());

	if (!SecurityHelper::IsRunningElevated()) {
		if (settings.CaptureSession0() || settings.CaptureKernel()) {
			AtlMessageBox(m_hWnd, L"Running with standard user rights. Session 0 and Kernel captures will not be available.",
				IDS_TITLE, MB_ICONWARNING);
			settings.CaptureKernel(false);
			settings.CaptureSession0(false);
		}
		UIEnable(ID_CAPTURE_CAPTURESESSION0, false);
		UIEnable(ID_CAPTURE_CAPTUREKERNEL, false);
	}
	auto view = new CDebugView(this);
	view->Create(m_Tabs, 0, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	view->SetFont(m_Font);
	m_pActiveView = view;
	view->Capture(AppSettings::Get().Capture());
	m_Tabs.AddPage(view->m_hWnd, L"Real-time Log", 0, view);

	SetAlwaysOnTop(settings.AlwaysOnTop());
	SetDarkMode(AppSettings::Get().DarkMode());

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	AppSettings::Get().SaveToKey();

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	static bool bVisible = true;	// initially visible
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST);	// toolbar is first 1st band
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnRunAsAdmin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (SecurityHelper::RunElevated())
		SendMessage(WM_COMMAND, ID_APP_EXIT);
	return 0;
}

void CMainFrame::SetAlwaysOnTop(bool alwaysOnTop) {
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, alwaysOnTop);
	SetWindowPos(alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool onTop = (GetExStyle() & WS_EX_TOPMOST) ? false : true;
	AppSettings::Get().AlwaysOnTop(onTop);
	SetAlwaysOnTop(onTop);
	return 0;
}

LRESULT CMainFrame::OnCapture(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool capture;
	AppSettings::Get().Capture(capture = !AppSettings::Get().Capture());
	UISetCheck(ID_CAPTURE_CAPTUREOUTPUT, capture);
	m_pActiveView->Capture(capture);
	return 0;
}

LRESULT CMainFrame::OnCaptureUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool capture;
	AppSettings::Get().CaptureUserMode(capture = !AppSettings::Get().CaptureUserMode());
	UISetCheck(ID_CAPTURE_CAPTUREUSERMODE, capture);
	if (AppSettings::Get().Capture())
		m_pActiveView->CaptureUser(capture);
	return 0;
}

LRESULT CMainFrame::OnCaptureUserSession0(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool capture;
	AppSettings::Get().CaptureSession0(capture = !AppSettings::Get().CaptureSession0());
	UISetCheck(ID_CAPTURE_CAPTURESESSION0, capture);
	if (AppSettings::Get().Capture())
		m_pActiveView->CaptureSession0(capture);
	return 0;
}

LRESULT CMainFrame::OnCaptureKernel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool capture;
	AppSettings::Get().CaptureKernel(capture = !AppSettings::Get().CaptureKernel());
	UISetCheck(ID_CAPTURE_CAPTUREKERNEL, capture);
	if (AppSettings::Get().Capture())
		m_pActiveView->CaptureKernel(capture);
	return 0;
}

LRESULT CMainFrame::OnAutoScroll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool autoscroll = !AppSettings::Get().AutoScroll();
	AppSettings::Get().AutoScroll(autoscroll);
	UISetCheck(ID_VIEW_AUTOSCROLL, autoscroll);

	return 0;
}

LRESULT CMainFrame::OnPageActivated(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/) {
	UpdateUI();
	return 0;
}

void CMainFrame::UpdateUI() {
	auto active = m_Tabs && m_Tabs.GetPageCount() > 0 && m_Tabs.GetPageData(m_Tabs.GetActivePage()) == m_pActiveView;
	UIEnable(ID_WINDOWS_CLOSE, active && !m_pActiveView->IsRealTime());
	UIEnable(ID_WINDOWS_CLOSEALL, m_Tabs.GetPageCount() > 1);
	UIEnable(ID_CAPTURE_CAPTUREOUTPUT, active);
	UIEnable(ID_CAPTURE_CAPTUREUSERMODE, active);
	UIEnable(ID_SEARCH_FIND, active && m_pActiveView && !m_pActiveView->IsEmpty());
	if (SecurityHelper::IsRunningElevated()) {
		UIEnable(ID_CAPTURE_CAPTUREKERNEL, active);
		UIEnable(ID_CAPTURE_CAPTURESESSION0, active);
	}
	if(active)
		m_pActiveView->UpdateUI(this);
}

LRESULT CMainFrame::OnMenuSelect(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) const {
	return 0;
}

BOOL CMainFrame::TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y, HWND hWnd) {
	return ShowContextMenu(hMenu, flags, x, y, hWnd);
}

LRESULT CMainFrame::OnEnableKernelComponents(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (!SecurityHelper::IsRunningElevated()) {
		AtlMessageBox(m_hWnd, L"Changing kernel component levels requires running elevated.",
			IDS_TITLE, MB_ICONERROR);
		return 0;
	}

	if (!Helpers::EnableAllkernelOutput(wID == ID_KERNEL_ENABLEALLCOMPONENTS)) {
		AtlMessageBox(m_hWnd, L"Failed to change kernel component levels.",
			IDS_TITLE, MB_ICONERROR);
	}

	return 0;
}

LRESULT CMainFrame::OnTabClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int page = m_Tabs.GetActivePage();
	ATLASSERT(page >= 0);
	if (page < 0)
		return 0;

	auto view = (CDebugView*)m_Tabs.GetPageData(page);
	if (view->CanClose())
		m_Tabs.RemovePage(page);
	return 0;
}

LRESULT CMainFrame::OnTabCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	while (m_Tabs.GetPageCount() > 0)
		SendMessage(WM_COMMAND, ID_WINDOWS_CLOSE);
	return 0;
}

void CMainFrame::InitDarkTheme() {
	m_DarkTheme.BackColor = m_DarkTheme.SysColors[COLOR_WINDOW] = RGB(32, 32, 32);
	m_DarkTheme.TextColor = m_DarkTheme.SysColors[COLOR_WINDOWTEXT] = RGB(248, 248, 248);
	m_DarkTheme.SysColors[COLOR_HIGHLIGHT] = RGB(10, 10, 160);
	m_DarkTheme.SysColors[COLOR_HIGHLIGHTTEXT] = RGB(240, 240, 240);
	m_DarkTheme.SysColors[COLOR_MENUTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_CAPTIONTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_BTNFACE] = m_DarkTheme.BackColor;
	m_DarkTheme.SysColors[COLOR_BTNTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_3DLIGHT] = RGB(192, 192, 192);
	m_DarkTheme.SysColors[COLOR_BTNHIGHLIGHT] = RGB(192, 192, 192);
	m_DarkTheme.SysColors[COLOR_CAPTIONTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_3DSHADOW] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_SCROLLBAR] = m_DarkTheme.BackColor;
	m_DarkTheme.Name = L"Dark";
	m_DarkTheme.Menu.BackColor = m_DarkTheme.BackColor;
	m_DarkTheme.Menu.TextColor = m_DarkTheme.TextColor;
	m_DarkTheme.StatusBar.BackColor = m_DarkTheme.BackColor;
	m_DarkTheme.StatusBar.TextColor = m_DarkTheme.TextColor;
}

LRESULT CMainFrame::OnDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto dark = !AppSettings::Get().DarkMode();
	AppSettings::Get().DarkMode(dark);
	SetDarkMode(dark);
	return 0;
}

void CMainFrame::SetDarkMode(bool dark) {
	ThemeHelper::SetCurrentTheme(dark ? m_DarkTheme : m_DefaultTheme, m_hWnd);
	ThemeHelper::UpdateMenuColors(*this, dark);
	UpdateMenu(GetMenu(), true);
	DrawMenuBar();

	UISetCheck(ID_OPTIONS_DARKMODE, dark);
}


