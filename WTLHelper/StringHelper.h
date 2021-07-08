#pragma once

#include <atlstr.h>

struct StringHelper {
	static CString Format(PCWSTR format, ...);
	static CString& Format(CString& text, PCWSTR format, ...);
};
