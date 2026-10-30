#include "pch.h"
#include "resource.h"
#include "PropertiesDlg.h"
#include "DebugOutputBase.h"
#include "ImageIconCache.h"
#include "ProcessManager.h"
#include "AppSettings.h"
#include "StackSymbols.h"
#include <thread>

CPropertiesDlg::CPropertiesDlg(DebugItem const& item, ImageIconCache const& iconCache, ProcessManager const& pm) 
    : m_Item(item), m_IconCache(iconCache), m_pm(pm) {
}

LRESULT CPropertiesDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout();
    AdjustOKCancelButtons(IDI_OK, IDI_CANCEL);

    AppSettings::Get().LoadWindowPosition(m_hWnd, L"PropertiesDlg");

    CImageList images(m_IconCache.GetImageList());
    m_hIcon.reset(images.GetIcon(m_Item.Image));
    SetDialogIcon(m_hIcon.get());

    SetDlgItemInt(IDC_PID, m_Item.Process.ProcessId, FALSE);
    SetDlgItemInt(IDC_INDEX, m_Item.Index, FALSE);
    SetDlgItemText(IDC_TIME, m_Item.LocalTime());
    SetDlgItemText(IDC_NAME, m_Item.ProcessName.c_str());
    if (auto pi = m_pm.GetProcessInfo(m_Item.Process)) {
        SetDlgItemText(IDC_PATH, pi->FullPath.c_str());
        SetDlgItemText(IDC_CMDLINE, pi->CommandLine.c_str());
    }

    //
    // the edit control needs CRLF line breaks; messages may use either LF or CRLF
    //
    m_DisplayText = m_Item.Text.c_str();
    m_DisplayText.Replace(L"\r\n", L"\n");
    m_DisplayText.Replace(L"\n", L"\r\n");

    SetDlgItemText(IDC_TEXT, m_DisplayText);
    SetDlgItemText(IDC_COMMENT, m_Item.Comment.c_str());
    InitStack();

    return 0;
}

void CPropertiesDlg::InitStack() {
    CListViewCtrl lv(GetDlgItem(IDC_STACK));
    lv.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    lv.InsertColumn(0, L"#", LVCFMT_RIGHT, 30);
    lv.InsertColumn(1, L"Address", LVCFMT_LEFT, 130);
    lv.InsertColumn(2, L"Symbol", LVCFMT_LEFT, 200);

    auto symbols = StackSymbols().ModuleOffsets(m_Item.Stack, m_Item.Process);
    CString text;
    for (int i = 0; i < (int)m_Item.Stack.size(); i++) {
        text.Format(L"%d", i);
        int n = lv.InsertItem(i, text);
        text.Format(L"0x%016llX", m_Item.Stack[i]);
        lv.SetItemText(n, 1, text);
        lv.SetItemText(n, 2, symbols[i]);
    }
    FitStackColumn();

    if (m_Item.Stack.empty())
        return;

    //
    // module+offset is shown meanwhile; loading symbols may download them
    //
    SetSymbolHeader(L"Symbol (loading symbols...)");
    std::thread([hDlg = m_hWnd, stack = m_Item.Stack, process = m_Item.Process] {
        auto names = std::make_unique<std::vector<CString>>(StackSymbols().FunctionNames(stack, process));
        //
        // fails if the dialog was closed meanwhile, and the names are freed here
        //
        if (::PostMessage(hDlg, WM_SYMBOLS_RESOLVED, 0, reinterpret_cast<LPARAM>(names.get())))
            names.release();
        }).detach();
}

LRESULT CPropertiesDlg::OnSymbolsResolved(UINT, WPARAM, LPARAM lParam, BOOL&) {
    std::unique_ptr<std::vector<CString>> names(reinterpret_cast<std::vector<CString>*>(lParam));
    CListViewCtrl lv(GetDlgItem(IDC_STACK));
    for (int i = 0; i < (int)names->size() && i < lv.GetItemCount(); i++) {
        auto& name = (*names)[i];
        if (!name.IsEmpty())
            lv.SetItemText(i, 2, name);
    }
    SetSymbolHeader(L"Symbol");
    FitStackColumn();
    return 0;
}

LRESULT CPropertiesDlg::OnSize(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& handled) {
    //
    // lay out the controls first, so the list has its new width
    //
    auto result = CDynamicDialogLayout<CPropertiesDlg>::OnSize(msg, wParam, lParam, handled);
    FitStackColumn();
    return result;
}

void CPropertiesDlg::FitStackColumn() {
    CListViewCtrl lv(GetDlgItem(IDC_STACK));
    if (lv && lv.GetHeader().GetItemCount() > 2)
        lv.SetColumnWidth(2, LVSCW_AUTOSIZE_USEHEADER);
}

void CPropertiesDlg::SetSymbolHeader(PCWSTR text) {
    CListViewCtrl lv(GetDlgItem(IDC_STACK));
    LVCOLUMN column{ LVCF_TEXT };
    column.pszText = const_cast<PWSTR>(text);
    lv.SetColumn(2, &column);
}

LRESULT CPropertiesDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    AppSettings::Get().SaveWindowPosition(m_hWnd, L"PropertiesDlg");

    if (wID == IDOK) {
        GetDlgItemText(IDC_COMMENT, m_Comment);
        GetDlgItemText(IDC_TEXT, m_Text);
        //
        // keep the original text (and its line breaks) unless it was edited
        //
        if (m_Text == m_DisplayText)
            m_Text = m_Item.Text.c_str();
        else
            m_Text.Replace(L"\r\n", L"\n");
    }
    EndDialog(wID);
    return 0;
}
