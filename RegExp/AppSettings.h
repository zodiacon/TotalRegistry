#pragma once

#include "Settings.h"
#include "FindOptions.h"

struct AppSettings : Settings {
	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(ShowExtraHives, 0, SettingType::Bool);
		SETTING(ShowKeysInList, 0, SettingType::Bool);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(ViewAddressBar, 1, SettingType::Bool);
		SETTING(ViewToolBar, 1, SettingType::Bool);
		SETTING(ViewStatusBar, 1, SettingType::Bool);
		SETTING(ReadOnly, 0, SettingType::Bool);
		SETTING(ReplaceRegEdit, 0, SettingType::Bool);
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(SingleInstance, 0, SettingType::Bool);
		SETTING(Find, FindOptions::SearchKeys | FindOptions::SearchValues | FindOptions::SearchStdRegistry | FindOptions::SearchSelected, SettingType::Int32);
	END_SETTINGS

	DEF_SETTING(ShowExtraHives, int)
	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING(ShowKeysInList, int)
	DEF_SETTING(ReadOnly, int)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING(Find, FindOptions)
	DEF_SETTING(ReplaceRegEdit, int)
	DEF_SETTING(DarkMode, int)
	DEF_SETTING(SingleInstance, int)
	DEF_SETTING(ViewAddressBar, int)
	DEF_SETTING(ViewToolBar, int)
	DEF_SETTING(ViewStatusBar, int)
};
