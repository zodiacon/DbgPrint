#pragma once

#include "VirtualListView.h"
#include "UserModeDebugOutput.h"
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
	int GetRowImage(HWND, int row) const;

	void DebugOutput(DWORD pid, PCSTR text, FILETIME const&) override;

protected:
	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CDebugView>)
		CHAIN_MSG_MAP(CCustomDraw<CDebugView>)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

	enum class ColumnType {
		ProcessId,
		Time,
		Index,
		Text,
		ProcessName,
	};

	struct DebugItem {
		DWORD Pid;
		CString Text;
		FILETIME SystemTime;
		mutable CString LocalTimeAsString;
		CString ProcessName;
		DWORD Index;
		CString Comment;
		int Image{ -1 };
	};

private:
	void UpdateList();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<std::shared_ptr<DebugItem>> m_Items, m_TempItems;
	std::mutex m_Lock;
	std::unique_ptr<UserModeDebugOutput> m_UserMode;
	std::unique_ptr<UserModeDebugOutput> m_UserModeSession0;
	inline static ULONG s_Index;
};

