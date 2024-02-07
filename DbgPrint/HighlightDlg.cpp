#include "pch.h"
#include "resource.h"
#include "HighlightDlg.h"
#include "AppSettings.h"

CHighlightDlg::CHighlightDlg(std::vector<HighlightItem> items) : m_Items(items) {
}

std::vector<HighlightItem> const& CHighlightDlg::GetItems() const {
    return m_Items;
}

CString CHighlightDlg::GetColumnText(HWND, int row, int col) const {
    auto& item = m_Items[row];
    switch (col) {
        case 1: return item.Text.c_str();
    }
    return L"";
}

int CHighlightDlg::OnPrePaint(int, LPNMCUSTOMDRAW cd) {
    if (cd->hdr.hwndFrom != m_List) {
        SetMsgHandled(FALSE);
        return 0;
    }

    return CDRF_NOTIFYITEMDRAW;
}

int CHighlightDlg::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
    if (cd->hdr.hwndFrom != m_List) {
        SetMsgHandled(FALSE);
        return 0;
    }
    return CDRF_NOTIFYSUBITEMDRAW;
}

int CHighlightDlg::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
    auto lv = (NMLVCUSTOMDRAW*)cd;
    if (lv->iSubItem == 0) {
        auto& item = m_Items[cd->dwItemSpec];
        CRect rc(cd->rc);
        CDCHandle dc(cd->hdc);
        rc.DeflateRect(2, 2);
        dc.FillSolidRect(&rc, AppSettings::Get().DarkMode() ? item.Dark : item.Light);
    }
    return CDRF_SKIPPOSTPAINT;
}

LRESULT CHighlightDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout();
    m_List.Attach(GetDlgItem(IDC_LIST));
    m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);

    auto cm = GetColumnManager(m_List);
    cm->AddColumn(L"Color", 0, 80);
    cm->AddColumn(L"Text", 0, 200);
    cm->UpdateColumns();

    m_List.SetItemCount((int)m_Items.size());

    return 0;
}

LRESULT CHighlightDlg::OnCloseCmd(WORD, WORD id, HWND, BOOL&) {
    EndDialog(id);
    return 0;
}
