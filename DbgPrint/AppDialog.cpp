#include "pch.h"
#include "AppDialog.h"
#include "AppSettings.h"

namespace {
	// skips a sz_Or_Ord member of a dialog template
	WORD const* SkipNameOrOrdinal(WORD const* p) {
		if (*p == 0)
			return p + 1;
		if (*p == 0xFFFF)
			return p + 2;
		return p + wcslen((PCWSTR)p) + 1;
	}

	size_t AlignDword(size_t offset) {
		return (offset + 3) & ~size_t(3);
	}
}

std::vector<BYTE> DialogFont::CreateTemplate(PCWSTR id) {
	auto font = AppSettings::Get().Font();
	if (font.lfHeight == 0 || font.lfFaceName[0] == 0)
		return {};

	auto hInstance = _AtlBaseModule.GetResourceInstance();
	auto hRes = ::FindResource(hInstance, id, RT_DIALOG);
	if (!hRes)
		return {};
	auto hGlobal = ::LoadResource(hInstance, hRes);
	auto data = hGlobal ? (BYTE const*)::LockResource(hGlobal) : nullptr;
	auto size = (size_t)::SizeofResource(hInstance, hRes);
	if (!data || size < 26)
		return {};

	//
	// DLGTEMPLATEEX: dlgVer, signature, helpID, exStyle, style, cDlgItems, x, y, cx, cy (26 bytes),
	// then menu, windowClass, title and, with DS_SETFONT, pointsize, weight, italic, charset, typeface;
	// the items follow, DWORD aligned
	//
	auto words = (WORD const*)data;
	if (words[0] != 1 || words[1] != 0xFFFF)
		return {};
	auto style = *(DWORD const*)(data + 12);
	if ((style & DS_SETFONT) == 0)
		return {};

	auto p = SkipNameOrOrdinal((WORD const*)(data + 26));	// menu
	p = SkipNameOrOrdinal(p);								// window class
	p += wcslen((PCWSTR)p) + 1;								// title
	auto fontOffset = (BYTE const*)p - data;
	auto face = (PCWSTR)(data + fontOffset + 6);
	auto itemsOffset = AlignDword((BYTE const*)(face + wcslen(face) + 1) - data);
	if (itemsOffset > size)
		return {};

	auto dc = ::GetDC(nullptr);
	auto dpi = ::GetDeviceCaps(dc, LOGPIXELSY);
	::ReleaseDC(nullptr, dc);
	auto points = (WORD)::MulDiv(abs(font.lfHeight), 72, dpi);

	std::vector<BYTE> result(data, data + fontOffset);
	// DS_FIXEDSYS with DS_SETFONT means "MS Shell Dlg"; the typeface is explicit now
	*(DWORD*)(result.data() + 12) = style & ~DS_FIXEDSYS;
	auto append = [&](void const* src, size_t count) {
		result.insert(result.end(), (BYTE const*)src, (BYTE const*)src + count);
	};
	WORD weight = (WORD)(font.lfWeight == 0 ? FW_NORMAL : font.lfWeight);
	append(&points, sizeof(points));
	append(&weight, sizeof(weight));
	append(&font.lfItalic, 1);
	append(&font.lfCharSet, 1);
	append(font.lfFaceName, (wcslen(font.lfFaceName) + 1) * sizeof(WCHAR));
	result.resize(AlignDword(result.size()));
	append(data + itemsOffset, size - itemsOffset);
	return result;
}
