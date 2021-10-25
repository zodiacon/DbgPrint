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

CMainFrame::CMainFrame() : m_Menu(this) {
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

void CMainFrame::InitMenu() {
	m_Menu.AddMenu(GetMenu());
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
	};
	for (auto& cmd : cmds) {
		if (cmd.icon)
			m_Menu.AddCommand(cmd.id, cmd.icon);
		else
			m_Menu.AddCommand(cmd.id, cmd.hIcon);
	}
}

void CMainFrame::InitToolBar(CToolBarCtrl& tb) {
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
	if (SecurityHelper::IsRunningElevated()) {
		CMenuHandle menu(GetMenu());
		menu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
		menu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
	}
	m_Menu.SetCheckIcon(AtlLoadIconImage(IDI_OK));

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

	SetAlwaysOnTop(settings.AlwaysOnTop());

	m_Tabs.m_bTabCloseButton = false;
	m_hWndClient = m_Tabs.Create(m_hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
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
		if (settings.CaptureSession0() || settings.CaptureKernel())
			AtlMessageBox(m_hWnd, L"Running with standard user rights. Session 0 and Kernel captures will not be available.",
				IDS_TITLE, MB_ICONWARNING);
		UIEnable(ID_CAPTURE_CAPTURESESSION0, false);
		UIEnable(ID_CAPTURE_CAPTUREKERNEL, false);
	}
	auto view = new CDebugView;
	view->Create(m_Tabs, 0, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	view->SetFont(m_Font);
	m_pActiveView = view;
	m_Tabs.AddPage(view->m_hWnd, L"Real-time Log", 0, view);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	AppSettings::Get().SaveToKey();

	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
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
	static BOOL bVisible = TRUE;	// initially visible
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST);	// toolbar is first 1st band
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
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
	auto active = m_Tabs.GetActivePage() >= 0 && m_Tabs.GetPageData(m_Tabs.GetActivePage()) == m_pActiveView;
	auto& settings = AppSettings::Get();
	UIEnable(ID_CAPTURE_CAPTUREOUTPUT, active);
	UIEnable(ID_CAPTURE_CAPTUREUSERMODE, active);
	if (SecurityHelper::IsRunningElevated()) {
		UIEnable(ID_CAPTURE_CAPTUREKERNEL, active);
		UIEnable(ID_CAPTURE_CAPTURESESSION0, active);
	}
}

LRESULT CMainFrame::OnMenuSelect(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	return 0;
}

