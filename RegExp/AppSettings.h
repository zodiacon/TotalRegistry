#pragma once

#include "Settings.h"

struct AppSettings : Settings {
	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(ShowExtraHives, false, SettingType::Bool);
		SETTING(AlwaysOnTop, false, SettingType::Bool);
	END_SETTINGS

	DEF_SETTING(ShowExtraHives, bool)
	DEF_SETTING(AlwaysOnTop, bool)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
};
