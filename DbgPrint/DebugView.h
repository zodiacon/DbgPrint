#pragma once

#include "VirtualListView.h"
#include "UserModeDebugOutput.h"
#include "KernelModeDebugOutput.h"
#include <mutex>

class CDebugView :
	public CFrameWindowImpl<CDebugView, CWindow, CControlWinTraits>,
	public IDebugOutput,
	public CVirtualListView<CDebugView>,
	public CCustomDraw<CDebugView> {
public:
	using BaseFrame = CFrameWindowImpl<CDebugView, CWindow, CControlWinTraits>;

	CString GetColumnText(HWND, int row, int col) const;
	PCWSTR GetExistingColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo* const);
	void Capture(bool capture);
	void CaptureKernel(bool capture);
	void CaptureUser(bool capture);
	void CaptureSession0(bool capture);
	
	void DebugOutput(DWORD pid, PCSTR text, FILETIME const&, DebugOutputFlags flags) override;

protected:
	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CVirtualListView<CDebugView>)
		CHAIN_MSG_MAP(CCustomDraw<CDebugView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
	END_MSG_MAP()

	enum class ColumnType {
		ProcessId, Time, Index,	Text, ProcessName, Comment
	};

	struct DebugItem {
		DWORD Pid;
		CString Text;
		FILETIME SystemTime;
		mutable CString LocalTimeAsString;
		CString ProcessName;
		DWORD Index;
		DebugOutputFlags Flags;
		CString Comment;
		int Image{ -1 };
	};

private:
	void UpdateList();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<std::shared_ptr<DebugItem>> m_Items, m_TempItems;
	std::mutex m_Lock;
	UserModeDebugOutput m_UserMode;
	UserModeDebugOutput m_UserModeSession0{ L"Global" };
	KernelModeDebugOutput m_KernelMode;
	inline static ULONG s_Index;
};

