#include "pch.h"
#include <strsafe.h>
#include "VersionResourceHelper.h"

#pragma comment(lib, "version")

VersionResourceHelper::VersionResourceHelper(PCWSTR path) : _path(path) {
	DWORD zero;
	auto infoSize = ::GetFileVersionInfoSize(path, &zero);
	if (infoSize) {
		_buffer = std::make_unique<BYTE[]>(infoSize);
		if (!::GetFileVersionInfo(path, 0, infoSize, _buffer.get()))
			_buffer.reset();
	}
}

bool VersionResourceHelper::IsValid() const {
	return _buffer != nullptr;
}

CString VersionResourceHelper::GetValue(const std::wstring& name) const {
	CString result;
	if (_buffer) {
		WORD* langAndCodePage;
		UINT len;
		if (::VerQueryValue(_buffer.get(), L"\\VarFileInfo\\Translation", (void**)&langAndCodePage, &len)) {
			WCHAR text[256];
			::StringCchPrintf(text, _countof(text), L"\\StringFileInfo\\%04x%04x\\%s", langAndCodePage[0], langAndCodePage[1], name.c_str());
			WCHAR* desc;
			if (::VerQueryValue(_buffer.get(), text, (void**)&desc, &len))
				result = desc;
		}
	}
	return result;
}



