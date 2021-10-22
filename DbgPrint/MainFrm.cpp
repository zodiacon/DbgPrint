// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "MainFrm.h"
#include "IconHelper.h"

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
		{ ID_CAPTURE_CAPTUREKERNEL, IDI_KERNEL },
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
	InitMenu();

	CToolBarCtrl tb;
	auto hWndToolBar = tb.Create(m_hWnd, nullptr, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE, 0, ATL_IDW_TOOLBAR);
	InitToolBar(tb);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndToolBar);

	CreateSimpleStatusBar();

	UIAddToolBar(hWndToolBar);

	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	m_hWndClient = m_DebugView.Create(m_hWnd, 0, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
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
