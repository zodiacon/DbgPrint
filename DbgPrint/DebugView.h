#pragma once

#include "VirtualListView.h"
#include "UserModeDebugOutput.h"
#include "KernelModeDebugOutput.h"
#include "resource.h"
#include <mutex>
#include "Interfaces.h"
#include "ImageIconCache.h"
#include "ProcessManager.h"

class CDebugView :
	public CFrameWindowImpl<CDebugView, CWindow, CControlWinTraits>,
	public IDebugOutput,
	public CVirtualListView<CDebugView>,
	public CCustomDraw<CDebugView> {
public:
	using BaseFrame = CFrameWindowImpl<CDebugView, CWindow, CControlWinTraits>;

	CDebugView(IMainFrame* frame, bool realTime = true);

	CString GetColumnText(HWND, int row, int col) const;
	PCWSTR GetExistingColumnText(HWND, int row, int col) const;
	bool IsSortable(HWND, int col) const;
	BOOL OnDoubleClickList(HWND, int row, int col, POINT const& pt);
	BOOL OnRightClickList(HWND, int row, int col, POINT const&);
	DWORD OnPrePaint(DWORD, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(DWORD, LPNMCUSTOMDRAW cd);

	bool CanClose();
	bool IsRealTime() const;
	bool IsEmpty() const;

	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo const*);
	void Capture(bool capture);
	void CaptureKernel(bool capture);
	void CaptureUser(bool capture);
	void CaptureSession0(bool capture);
	
	void UpdateUI(CUpdateUIBase* ui);

	void DebugOutput(DWORD pid, PCSTR text, FILETIME const&, DebugOutputFlags flags) override;

protected:
	BEGIN_MSG_MAP(CDebugView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnItemChanged)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(CFindReplaceDialog::GetFindReplaceMsg(), OnFind)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SETFONT, OnSetFont)
		CHAIN_MSG_MAP(CVirtualListView<CDebugView>)
		CHAIN_MSG_MAP(CCustomDraw<CDebugView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_SEARCH_FINDNEXT, OnFindNext)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnProperties)
		COMMAND_ID_HANDLER(ID_EDIT_DELETE, OnEditDelete)
		COMMAND_ID_HANDLER(ID_EDIT_COMMENT, OnEditComment)
		COMMAND_ID_HANDLER(ID_EDIT_CLEAR_ALL, OnEditClearAll)
		COMMAND_ID_HANDLER(ID_EDIT_BOOKMARK, OnToggleBookmark)
		COMMAND_ID_HANDLER(ID_VIEW_NEXTBOOKMARK, OnNextBookmark)
		COMMAND_ID_HANDLER(ID_VIEW_PREVIOUSBOOKMARK, OnPrevBookmark)
		COMMAND_ID_HANDLER(ID_EDIT_DELETEALLBOOKMARKS, OnDeleteAllBookmarks)
		COMMAND_ID_HANDLER(ID_FILE_SAVEASTEXT, OnSaveAsText)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnSave)
		COMMAND_ID_HANDLER(ID_EDIT_HIGHLIGHT, OnHighlight)
	END_MSG_MAP()

	enum class ColumnType {
		ProcessId, Time, Index,	Text, ProcessName, Comment
	};

private:
	void UpdateList();
	void ShowProperties(int row);

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFind(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSetFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSaveAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnItemChanged(int /*idCtrl*/, LPNMHDR hdr, BOOL& /*bHandled*/);
	LRESULT OnEditDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditComment(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleBookmark(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNextBookmark(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPrevBookmark(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDeleteAllBookmarks(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHighlight(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFindNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<std::shared_ptr<DebugItem>> m_Items, m_TempItems;
	std::mutex m_Lock;
	UserModeDebugOutput m_UserMode;
	UserModeDebugOutput m_UserModeSession0{ L"Global" };
	KernelModeDebugOutput m_KernelMode;
	inline static ULONG s_Index;
	CUpdateUIBase* m_ui{ nullptr };
	IMainFrame* m_pFrame;
	std::atomic<bool> m_Running{ false };
	ImageIconCache m_IconCache;
	ProcessManager m_pm;
	std::vector<HighlightItem> m_Highlights;
	bool m_RealTime;
};

