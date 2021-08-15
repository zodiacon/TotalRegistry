#pragma once

struct Theme;

enum class DCOperation {
	None,
	SetTextColor = 1,
};
DEFINE_ENUM_FLAG_OPERATORS(DCOperation);

struct ThemeHelper abstract final {
	static bool LoadFromFile(PCWSTR path, Theme& theme);
	static bool SaveToFile(Theme const& theme, PCWSTR path);
	static bool Init(HANDLE hThread = ::GetCurrentThread());
	static int Suspend();
	static int Resume();
	static bool SuspendDCOperation(DCOperation op, HDC hdc);
	static bool ResumeDCOperation(DCOperation op, HDC hdc);

	static const Theme* GetCurrentTheme();
	static void SetCurrentTheme(const Theme& theme);
};

