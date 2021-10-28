#include "pch.h"
#include "resource.h"
#include "CommentDlg.h"

LRESULT CCommentDlg::OnInitDialog(UINT, WPARAM, LPARAM lp, BOOL&) {
    InitDynamicLayout();
    AdjustOKCancelButtons();

    auto hIcon = reinterpret_cast<HICON>(lp);
    if (hIcon)
        SetDialogIcon(hIcon);

    SetDlgItemText(IDC_COMMENT, m_Comment);

    return 0;
}

LRESULT CCommentDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    if (wID == IDOK)
        GetDlgItemText(IDC_COMMENT, m_Comment);
    EndDialog(wID);
    return 0;
}
