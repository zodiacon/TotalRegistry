#pragma once

//
// readable interpretations of Registry value data, shown in the list's Details column
// every decoder checks the data fully, and returns an empty string if the data doesn't fit
//
namespace ValueDecoder {
	// the best interpretation of a value's data, or an empty string
	CString Decode(PCWSTR name, DWORD type, BYTE const* data, DWORD size);

	// a number in hex and decimal, the decimal first if requested; bytes is the value's size (4 or 8)
	CString FormatNumber(ULONGLONG value, int bytes, bool decimalFirst);

	CString DecodeFileTime(BYTE const* data, DWORD size);
	CString DecodeSystemTime(BYTE const* data, DWORD size);
	CString DecodeGuid(BYTE const* data, DWORD size);
	CString DecodeSid(BYTE const* data, DWORD size);
	CString DecodeSecurityDescriptor(BYTE const* data, DWORD size);
	// UTF-16 strings stored in a binary value, e.g. MRU lists
	CString DecodeStrings(BYTE const* data, DWORD size);
	CString DecodeResourceList(BYTE const* data, DWORD size);
	// used, if not null, receives the size of the descriptor (they're variable length)
	CString DecodeFullResourceDescriptor(BYTE const* data, DWORD size, DWORD* used = nullptr);
	CString DecodeRequirementsList(BYTE const* data, DWORD size);
}
