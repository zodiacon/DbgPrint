#pragma once

#include <Settings.h>
#include "Interfaces.h"

struct AppSettings : Settings {
	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(Font, LOGFONT{}, SettingType::Binary);
		SETTING(MenuFont, LOGFONT{}, SettingType::Binary);
		SETTING(ViewToolBar, 1, SettingType::Bool);
		SETTING(ViewStatusBar, 1, SettingType::Bool);
		SETTING(SingleInstance, 0, SettingType::Bool);
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(Capture, 1, SettingType::Bool);
		SETTING(ConfirmErase, 0, SettingType::Bool);
		SETTING(CaptureUserMode, 1, SettingType::Bool);
		SETTING(CaptureSession0, 0, SettingType::Bool);
		SETTING(CaptureKernel, 0, SettingType::Bool);
		SETTING(CaptureStacks, 0, SettingType::Bool);
		SETTING(SymbolServerEnabled, 1, SettingType::Bool);
		// SETTING_STRING copies the text; SETTING with a PCWSTR would store the pointer's bytes
		SETTING_STRING(SymbolServerUrl, DefaultSymbolServerUrl);
		SETTING_STRING(SymbolCache, DefaultSymbolCache);
		SETTING_STRING(SymbolExtraPaths, L"");
		SETTING(SymbolUseEnvPath, 1, SettingType::Bool);
		SETTING(AutoScroll, 0, SettingType::Bool);
		SETTING(BookmarkColor, RGB(0, 255, 0), SettingType::Int32);
		SETTING_STRING(HighlightRules, L"");	// see FormatHighlights
	END_SETTINGS

	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING(Font, LOGFONT)
	DEF_SETTING(ViewToolBar, int)
	DEF_SETTING(ViewStatusBar, int)
	DEF_SETTING(SingleInstance, int)
	DEF_SETTING(DarkMode, int)
	DEF_SETTING(Capture, int)
	DEF_SETTING(CaptureUserMode, int)
	DEF_SETTING(CaptureSession0, int)
	DEF_SETTING(CaptureKernel, int)
	DEF_SETTING(CaptureStacks, int)
	DEF_SETTING(AutoScroll, int)
	DEF_SETTING_STRING(HighlightRules)
	DEF_SETTING(ConfirmErase, bool);
	DEF_SETTING(SymbolServerEnabled, bool)
	DEF_SETTING_STRING(SymbolServerUrl)
	DEF_SETTING_STRING(SymbolCache)
	DEF_SETTING_STRING(SymbolExtraPaths)
	DEF_SETTING(SymbolUseEnvPath, bool)

	static constexpr PCWSTR DefaultSymbolServerUrl = L"https://msdl.microsoft.com/download/symbols";
	static constexpr PCWSTR DefaultSymbolCache = L"%LOCALAPPDATA%\\DbgPrint\\Symbols";

	// the search path handed to DbgHelp ("extra paths;_NT_SYMBOL_PATH;srv*cache*url"); empty: none
	std::wstring BuildSymbolSearchPath() const;
	static std::wstring BuildSymbolSearchPath(bool useServer, std::wstring const& url, std::wstring const& cache,
		std::wstring const& extra, bool useEnv);

	// highlight rules as text, one per line: "light|dark|text" with the colors as COLORREF hex
	// (the same format is used by the highlight dialog's Save and Load)
	static std::wstring FormatHighlights(std::vector<HighlightItem> const& items);
	static std::vector<HighlightItem> ParseHighlights(std::wstring const& text);
};

