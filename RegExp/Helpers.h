#pragma once

struct Helpers abstract final {
	static bool SaveWindowPosition(HWND hWnd, PCWSTR name);
	static bool RestoreWindowPosition(HWND hWnd, PCWSTR name);
};

