#pragma once

#include "DialogHelper.h"

struct CCommentDlg :
	public CDialogImpl<CCommentDlg>,
	public CDynamicDialogLayout<CCommentDlg>,
	public CDialogHelper<CCommentDlg> {
public:
	enum { IDD = IDD_COMMENT };

	CCommentDlg(std::wstring const& comment) : m_Comment(comment) {}

	std::wstring const& GetComment() const {
		return m_Comment;
	}

protected:
	BEGIN_MSG_MAP(CCommentDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CCommentDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	std::wstring m_Comment;
	CEdit m_Edit;
};
