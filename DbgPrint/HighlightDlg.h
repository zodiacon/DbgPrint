#pragma once

#include "DialogHelper.h"
#include "VirtualListView.h"
#include "Interfaces.h"

struct CHighlightDlg :
	public CDialogImpl<CHighlightDlg>,
	public CDynamicDialogLayout<CHighlightDlg>,
	public CVirtualListView<CHighlightDlg>,
	public CCustomDraw<CHighlightDlg>,
	public CDialogHelper<CHighlightDlg> {
public:
	enum { IDD = IDD_HIGHLIGHT };

	explicit CHighlightDlg(std::vector<HighlightItem> items);

	std::vector<HighlightItem> const& GetItems() const;

	CString GetColumnText(HWND, int row, int col) const;
	
	int OnPrePaint(int, LPNMCUSTOMDRAW cd);
	int OnItemPrePaint(int, LPNMCUSTOMDRAW cd);
	int OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd);

protected:
	BEGIN_MSG_MAP(CHighlightDlg)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CCustomDraw<CHighlightDlg>)
		CHAIN_MSG_MAP(CVirtualListView<CHighlightDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CHighlightDlg>)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	END_MSG_MAP()

private:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	std::vector<HighlightItem> m_Items;
	CListViewCtrl m_List;
};
