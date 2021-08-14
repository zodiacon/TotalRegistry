#pragma once

#include <vector>
#include <atlstr.h>

class IniFile {
public:
	IniFile(PCWSTR path);

	bool IsValid() const;

	CString ReadString(PCWSTR section, PCWSTR name, PCWSTR defaultValue = nullptr);
	int ReadInt(PCWSTR section, PCWSTR name, int defaultValue = 0);
	COLORREF ReadColor(PCWSTR section, PCWSTR name, COLORREF defaultValue = CLR_INVALID);
	std::vector<CString> ReadSection(PCWSTR section);
	bool ReadBool(PCWSTR section, PCWSTR name, bool defaultValue = false);
	bool ReadFont(PCWSTR section, PCWSTR name, LOGFONT& font);
	std::unique_ptr<uint8_t[]> ReadBinary(PCWSTR section, PCWSTR name, unsigned& size);

	bool WriteString(PCWSTR section, PCWSTR name, PCWSTR value);
	bool WriteInt(PCWSTR section, PCWSTR name, int value, bool hex = false);
	bool WriteBool(PCWSTR section, PCWSTR name, bool value);
	bool WriteColor(PCWSTR section, PCWSTR name, COLORREF color);
	bool WriteFont(PCWSTR section, PCWSTR name, const LOGFONT& font);
	bool WriteBinary(PCWSTR section, PCWSTR name, void* data, unsigned size);

protected:
	COLORREF ParseHexColor(const CString& hex);
	COLORREF ParseDecColor(const CString& text);

private:
	CString _path;
};

