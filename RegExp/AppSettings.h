#pragma once

#include "Settings.h"
#include "FindOptions.h"

struct AppSettings : Settings {
	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(ShowExtraHives, 0, SettingType::Bool);
		SETTING(ShowKeysInList, 0, SettingType::Bool);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(Find, FindOptions::SearchKeys | FindOptions::SearchValues | FindOptions::SearchStdRegistry | FindOptions::SearchSelected, SettingType::Int32);
	END_SETTINGS

	DEF_SETTING(ShowExtraHives, int)
	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING(ShowKeysInList, int)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING(Find, FindOptions)
};
