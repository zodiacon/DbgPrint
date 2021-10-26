#include "pch.h"
#include "Helpers.h"
#include "resource.h"

void Helpers::ReportError(PCWSTR text, DWORD error) {
    PWSTR buffer;
    CString msg;
    if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, nullptr)) {
        msg = buffer;
        ::LocalFree(buffer);
        msg.Trim(L"\n\r");
    }
    AtlMessageBox(nullptr, (PCWSTR)(text + msg), IDS_TITLE, MB_ICONERROR);
}
