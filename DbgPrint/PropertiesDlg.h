#pragma once

#include "DialogHelper.h"

struct DebugItem;
class ImageIconCache;
class ProcessManager;

class CPropertiesDlg :
	public CDialogImpl<CPropertiesDlg>,
	public CDialogHelper<CPropertiesDlg>,
	public CDynamicDialogLayout<CPropertiesDlg> {
public:
	enum { IDD = IDD_PROPERTIES };

	CPropertiesDlg(DebugItem const& item, ImageIconCache const& iconCache, ProcessManager const& pm);

	CString const& GetComment() const {
		return m_Comment;
	}

	CString const& GetText() const {
		return m_Text;
	}

protected:
	BEGIN_MSG_MAP(CPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CPropertiesDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	DebugItem const& m_Item;
	CString m_Comment, m_Text;
	ImageIconCache const& m_IconCache;
	ProcessManager const& m_pm;
};
