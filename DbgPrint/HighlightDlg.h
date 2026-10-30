#pragma once

#include "DialogHelper.h"
#include "VirtualListView.h"
#include "Interfaces.h"

//
// edits the highlight rules: a row whose message contains a rule's text (any case) gets its colors
//
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

	// the background for dark mode, from the color picked for light mode
	static COLORREF DarkVariant(COLORREF light);

protected:
	BEGIN_MSG_MAP(CHighlightDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_ADD, OnAdd)
		COMMAND_ID_HANDLER(IDC_DELETE, OnDelete)
		COMMAND_ID_HANDLER(IDC_COLOR, OnColor)
		COMMAND_ID_HANDLER(IDC_RESET, OnReset)
		COMMAND_ID_HANDLER(IDC_SAVE, OnSave)
		COMMAND_ID_HANDLER(IDC_LOAD, OnLoad)
		COMMAND_HANDLER(IDC_TEXT, EN_CHANGE, OnTextChanged)
		NOTIFY_HANDLER(IDC_LIST, LVN_ITEMCHANGED, OnItemChanged)
		CHAIN_MSG_MAP(CCustomDraw<CHighlightDlg>)
		CHAIN_MSG_MAP(CVirtualListView<CHighlightDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CHighlightDlg>)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoad(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	int GetSelected() const;
	void Select(int index);
	void Refresh();
	void UpdateButtons();

	std::vector<HighlightItem> m_Items;
	CListViewCtrl m_List;
	bool m_Updating{ false };	// setting the edit's text from the selection, not the user typing
};
