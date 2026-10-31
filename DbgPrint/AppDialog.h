#pragma once

//
// the dialog templates rewritten to use the font chosen in Options > Font
//
struct DialogFont {
	// a copy of the dialog's template with the font replaced; empty: use the template as is
	// (no font chosen, or a template that is not a DIALOGEX with a font)
	static std::vector<BYTE> CreateTemplate(PCWSTR id);
};

//
// a dialog created with the chosen font; its layout is in dialog units, so it scales with the font
//
template<typename T>
class CAppDialogImpl : public CDialogImpl<T> {
public:
	INT_PTR DoModal(HWND hWndParent = ::GetActiveWindow(), LPARAM dwInitParam = 0) {
		auto dlgTemplate = DialogFont::CreateTemplate(MAKEINTRESOURCE(T::IDD));
		if (dlgTemplate.empty())
			return CDialogImpl<T>::DoModal(hWndParent, dwInitParam);

		//
		// what CDialogImpl::DoModal does, with the template from memory
		//
		ATLASSUME(this->m_hWnd == nullptr);
		if (!this->m_thunk.Init(nullptr, nullptr)) {
			::SetLastError(ERROR_OUTOFMEMORY);
			return -1;
		}
		_AtlWinModule.AddCreateWndData(&this->m_thunk.cd, (CDialogImplBaseT<CWindow>*)this);
#ifdef _DEBUG
		this->m_bModal = true;
#endif
		return ::DialogBoxIndirectParam(_AtlBaseModule.GetResourceInstance(), (LPCDLGTEMPLATE)dlgTemplate.data(),
			hWndParent, T::StartDialogProc, dwInitParam);
	}
};
