#pragma once

#include <memory>
#include <string>

class VersionResourceHelper {
public:
	VersionResourceHelper(PCWSTR path);
	bool IsValid() const;
	operator bool() const {
		return IsValid();
	}
	CString GetValue(const std::wstring& name) const;
	const CString& GetPath() const {
		return _path;
	}

private:
	std::unique_ptr<BYTE[]> _buffer;
	CString _path;
};

