#include "pch.h"
#include "StringHelper.h"

CString StringHelper::Format(PCWSTR format, ...) {
	va_list argList;
	va_start(argList, format);
	CString text;
	text.Format(format, argList);
	va_end(argList);
	return text;
}

CString& StringHelper::Format(CString& text, PCWSTR format, ...) {
	va_list argList;
	va_start(argList, format);
	text.Format(format, argList);
	va_end(argList);
	return text;
}

