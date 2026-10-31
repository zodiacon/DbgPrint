#pragma once

#include "AppDialog.h"

#include "DialogHelper.h"
#include <wil\resource.h>

struct DebugItem;
class ImageIconCache;
class ProcessManager;

class CPropertiesDlg :
	public CAppDialogImpl<CPropertiesDlg>,
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
	// posted by the symbol loading thread; lParam is a heap-allocated std::vector<CString>
	enum { WM_SYMBOLS_RESOLVED = WM_APP + 1 };

	BEGIN_MSG_MAP(CPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SYMBOLS_RESOLVED, OnSymbolsResolved)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
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
	LRESULT OnSymbolsResolved(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void InitStack();
	void FitStackColumn();
	void SetSymbolHeader(PCWSTR text);

	DebugItem const& m_Item;
	CString m_Comment, m_Text;
	CString m_DisplayText;	// text as shown in the edit control (CRLF line breaks)
	wil::unique_hicon m_hIcon;	// the window does not own its icon
	ImageIconCache const& m_IconCache;
	ProcessManager const& m_pm;
};
