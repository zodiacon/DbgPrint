#include "pch.h"
#include "resource.h"
#include "CommentDlg.h"

LRESULT CCommentDlg::OnInitDialog(UINT, WPARAM, LPARAM lp, BOOL&) {
    InitDynamicLayout();
    AdjustOKCancelButtons(IDI_OK, IDI_CANCEL);

    m_Edit.Attach(GetDlgItem(IDC_COMMENT));

    auto hIcon = reinterpret_cast<HICON>(lp);
    if (hIcon)
        SetDialogIcon(hIcon);

    m_Edit.SetWindowTextW(m_Comment.c_str());
    
    return 0;
}

LRESULT CCommentDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    if (wID == IDOK) {
        CString comment;
        GetDlgItemText(IDC_COMMENT, comment);
        m_Comment = comment;
    }
    EndDialog(wID);
    return 0;
}
