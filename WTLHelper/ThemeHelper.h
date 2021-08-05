#pragma once

struct Theme;

struct ThemeHelper abstract final {
	static bool LoadFromFile(PCWSTR path, Theme& theme);
	static bool SaveToFile(Theme const& theme, PCWSTR path);
	static bool Init(HANDLE hThread = ::GetCurrentThread());

	static const Theme* GetCurrentTheme();
	static void SetCurrentTheme(const Theme& theme);
};

