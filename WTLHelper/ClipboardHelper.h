#pragma once

struct ClipboardHelper abstract final {
	static bool CopyText(HWND hWnd, PCWSTR text);
};

