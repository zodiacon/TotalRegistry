#include "pch.h"
#include "IniFile.h"
#include <sstream>

IniFile::IniFile(PCWSTR path) : _path(path) {
}

bool IniFile::IsValid() const {
    return ::GetFileAttributes(_path) != INVALID_FILE_ATTRIBUTES;
}

CString IniFile::ReadString(PCWSTR section, PCWSTR name, PCWSTR defaultValue) {
    CString result;
    auto count = ::GetPrivateProfileString(section, name, defaultValue, result.GetBufferSetLength(128), 128, _path);
    return result;
}

bool IniFile::ReadBool(PCWSTR section, PCWSTR name, bool defaultValue) {
    auto value = ReadInt(section, name, -1);
    if (value == -1)
        return defaultValue;
    return value ? true : false;
}

int IniFile::ReadInt(PCWSTR section, PCWSTR name, int defaultValue) {
    return ::GetPrivateProfileInt(section, name, defaultValue, _path);
}

COLORREF IniFile::ReadColor(PCWSTR section, PCWSTR name, COLORREF defaultValue) {
    auto text = ReadString(section, name);
    if (text.IsEmpty())
        return defaultValue;

    if (text.Left(2) == L"0x")
        return ParseHexColor(text.Mid(2));

    if (text.Find(L','))
        return ParseDecColor(text);

    return ParseHexColor(text);
}

COLORREF IniFile::ParseHexColor(const CString& hex) {
    std::wstringstream ss;
    DWORD color;
    ss << std::hex << hex;
    ss >> color;
    return color;
}

COLORREF IniFile::ParseDecColor(const CString& text) {
    int start = 0;
    COLORREF color = 0;
    auto str = text.Tokenize(L",", start);
    if (str.IsEmpty())
        return CLR_INVALID;
    color |= _wtoi(str);
    str = text.Tokenize(L",", start);
    if (str.IsEmpty())
        return CLR_INVALID;
    color |= _wtoi(str) << 8;
    str = text.Tokenize(L",", start);
    if (str.IsEmpty())
        return CLR_INVALID;
    color |= _wtoi(str) << 16;

    return color;
}

std::vector<CString> IniFile::ReadSection(PCWSTR section) {
    WCHAR buffer[1 << 10];
    std::vector<CString> names;
    if (0 == ::GetPrivateProfileSection(section, buffer, _countof(buffer), _path))
        return names;

    names.reserve(8);
    for (auto p = buffer; *p; ) {
        names.push_back(p);
        p += names.back().GetLength() + 1;
    }
    return names;
}

bool IniFile::WriteString(PCWSTR section, PCWSTR name, PCWSTR value) {
    return ::WritePrivateProfileString(section, name, value, _path);
}

bool IniFile::WriteInt(PCWSTR section, PCWSTR name, int value, bool hex) {
    CString text;
    text.Format(hex ? L"0x%X" : L"%d", value);
    return WriteString(section, name, text);
}

bool IniFile::WriteBool(PCWSTR section, PCWSTR name, bool value) {
    return WriteInt(section, name, value ? 1 : 0);
}

bool IniFile::WriteColor(PCWSTR section, PCWSTR name, COLORREF color) {
    CString text;
    text.Format(L"%d,%d,%d", GetRValue(color), GetGValue(color), GetBValue(color));
    return WriteString(section, name, text);
}

bool IniFile::WriteFont(PCWSTR section, PCWSTR name, const LOGFONT& font) {
    return ::WritePrivateProfileStruct(section, name, (PVOID)&font, sizeof(font), _path);
}

bool IniFile::WriteBinary(PCWSTR section, PCWSTR name, void* data, unsigned size) {
    WriteInt(section, name + CString(L"_size"), size);
    return ::WritePrivateProfileStruct(section, name, data, size, _path);
}

bool IniFile::ReadFont(PCWSTR section, PCWSTR name, LOGFONT& font) {
    return ::GetPrivateProfileStruct(section, name, &font, sizeof(font), _path);
}

std::unique_ptr<uint8_t[]> IniFile::ReadBinary(PCWSTR section, PCWSTR name, unsigned& size) {
    size = (unsigned)ReadInt(section, name + CString(L"_size"));
    if (size == 0)
        return nullptr;
    auto buffer = std::make_unique<uint8_t[]>(size);
    if (buffer == nullptr)
        return nullptr;
    ::GetPrivateProfileStruct(section, name, buffer.get(), size, _path);
    return buffer;
}

