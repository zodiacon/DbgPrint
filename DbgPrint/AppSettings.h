#pragma once

#include "Settings.h"

struct AppSettings : Settings {
	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(Font, LOGFONT{}, SettingType::Binary);
		SETTING(ViewToolBar, 1, SettingType::Bool);
		SETTING(ViewStatusBar, 1, SettingType::Bool);
		SETTING(SingleInstance, 0, SettingType::Bool);
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(Capture, 1, SettingType::Bool);
		SETTING(CaptureUserMode, 1, SettingType::Bool);
		SETTING(CaptureSession0, 0, SettingType::Bool);
		SETTING(CaptureKernel, 0, SettingType::Bool);
		SETTING(AutoScroll, 0, SettingType::Bool);
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
	DEF_SETTING(AutoScroll, int)
};

