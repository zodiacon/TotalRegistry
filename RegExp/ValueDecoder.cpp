#include "pch.h"
#include "ValueDecoder.h"
#include "Helpers.h"
#include <sddl.h>

#pragma comment(lib, "ntdll")

// unlike IsValidSecurityDescriptor, checks the descriptor fits the buffer
extern "C" BOOLEAN NTAPI RtlValidRelativeSecurityDescriptor(PSECURITY_DESCRIPTOR descriptor, ULONG length, SECURITY_INFORMATION required);

namespace {
	//
	// the kernel's resource structures (wdm.h), which user mode headers don't define; packed to 4 like the originals
	//
#pragma pack(push, 4)
	struct PartialDescriptor {
		UCHAR Type;
		UCHAR ShareDisposition;
		USHORT Flags;
		union {
			struct { LARGE_INTEGER Start; ULONG Length; } Generic;
			struct { USHORT Level; USHORT Group; ULONG Vector; ULONG_PTR Affinity; } Interrupt;
			struct { ULONG Channel; ULONG Port; ULONG Reserved; } Dma;
			struct { ULONG Start; ULONG Length; ULONG Reserved; } BusNumber;
			struct { ULONG DataSize; ULONG Reserved1; ULONG Reserved2; } DeviceSpecificData;
		} u;
	};

	struct FullDescriptorHeader {
		LONG InterfaceType;
		ULONG BusNumber;
		USHORT Version;
		USHORT Revision;
		ULONG Count;
	};

	struct IoDescriptor {
		UCHAR Option;
		UCHAR Type;
		UCHAR ShareDisposition;
		UCHAR Spare1;
		USHORT Flags;
		USHORT Spare2;
		union {
			struct { ULONG Length; ULONG Alignment; LARGE_INTEGER Minimum; LARGE_INTEGER Maximum; } Range;
			struct { ULONG MinimumVector; ULONG MaximumVector; ULONG Policy; ULONG Priority; ULONG_PTR TargetedProcessors; } Interrupt;
			struct { ULONG MinimumChannel; ULONG MaximumChannel; } Dma;
			struct { ULONG Length; ULONG MinBusNumber; ULONG MaxBusNumber; ULONG Reserved; } BusNumber;
		} u;
	};

	struct RequirementsListHeader {
		ULONG ListSize;
		LONG InterfaceType;
		ULONG BusNumber;
		ULONG SlotNumber;
		ULONG Reserved[3];
		ULONG AlternativeLists;
	};

	struct IoListHeader {
		USHORT Version;
		USHORT Revision;
		ULONG Count;
	};
#pragma pack(pop)
#ifdef _WIN64
	static_assert(sizeof(PartialDescriptor) == 20 && sizeof(IoDescriptor) == 32);
#endif

	enum ResourceType : UCHAR {
		Port = 1, Interrupt = 2, Memory = 3, Dma = 4, DeviceSpecific = 5, BusNumber = 6, MemoryLarge = 7,
	};

	CString InterfaceTypeName(LONG type) {
		static PCWSTR const names[] = {
			L"Internal", L"ISA", L"EISA", L"MicroChannel", L"TurboChannel", L"PCI", L"VME", L"NuBus", L"PCMCIA", L"CBus",
			L"MPI", L"MPSA", L"Processor", L"Power", L"PnP ISA", L"PnP", L"VMCS", L"ACPI",
		};
		if (type >= 0 && type < (LONG)_countof(names))
			return names[type];
		CString text;
		text.Format(L"Bus type %d", type);
		return text;
	}

	CString FormatRange(PCWSTR kind, ULONGLONG start, ULONGLONG length) {
		CString text;
		if (length == 0)
			text.Format(L"%s 0x%llX (empty)", kind, start);
		else
			text.Format(L"%s 0x%llX-0x%llX", kind, start, start + length - 1);
		return text;
	}

	CString FormatPartial(PartialDescriptor const& d) {
		CString text;
		switch (d.Type) {
			case Port:
				return FormatRange(L"I/O", d.u.Generic.Start.QuadPart, d.u.Generic.Length);
			case Memory:
				return FormatRange(L"Memory", d.u.Generic.Start.QuadPart, d.u.Generic.Length);
			case MemoryLarge:
			{
				// the length is stored shifted, per the flags
				ULONGLONG length = d.u.Generic.Length;
				if (d.Flags & 0x200)
					length <<= 8;
				else if (d.Flags & 0x400)
					length <<= 16;
				else if (d.Flags & 0x800)
					length <<= 32;
				return FormatRange(L"Memory", d.u.Generic.Start.QuadPart, length);
			}
			case Interrupt:
				text.Format(L"Interrupt %u", d.u.Interrupt.Vector);
				if (d.Flags & 0x2)
					text += L" (MSI)";
				else if (d.u.Interrupt.Level != d.u.Interrupt.Vector)
					text.AppendFormat(L" (level %u)", d.u.Interrupt.Level);
				return text;
			case Dma:
				text.Format(L"DMA %u", d.u.Dma.Channel);
				return text;
			case DeviceSpecific:
				text.Format(L"Device data (%u bytes)", d.u.DeviceSpecificData.DataSize);
				return text;
			case BusNumber:
				if (d.u.BusNumber.Length <= 1)
					text.Format(L"Bus %u", d.u.BusNumber.Start);
				else
					text.Format(L"Bus %u-%u", d.u.BusNumber.Start, d.u.BusNumber.Start + d.u.BusNumber.Length - 1);
				return text;
		}
		text.Format(L"Resource type %u", d.Type);
		return text;
	}

	CString FormatIo(IoDescriptor const& d) {
		CString text;
		switch (d.Type) {
			case Port:
			case Memory:
			case MemoryLarge:
				text.Format(L"%s 0x%llX-0x%llX length 0x%X", d.Type == Port ? L"I/O" : L"Memory",
					d.u.Range.Minimum.QuadPart, d.u.Range.Maximum.QuadPart, d.u.Range.Length);
				return text;
			case Interrupt:
				if (d.u.Interrupt.MinimumVector == d.u.Interrupt.MaximumVector)
					text.Format(L"Interrupt %u", d.u.Interrupt.MinimumVector);
				else
					text.Format(L"Interrupt %u-%u", d.u.Interrupt.MinimumVector, d.u.Interrupt.MaximumVector);
				return text;
			case Dma:
				if (d.u.Dma.MinimumChannel == d.u.Dma.MaximumChannel)
					text.Format(L"DMA %u", d.u.Dma.MinimumChannel);
				else
					text.Format(L"DMA %u-%u", d.u.Dma.MinimumChannel, d.u.Dma.MaximumChannel);
				return text;
			case BusNumber:
				text.Format(L"Bus %u-%u", d.u.BusNumber.MinBusNumber, d.u.BusNumber.MaxBusNumber);
				return text;
		}
		text.Format(L"Resource type %u", d.Type);
		return text;
	}

	bool ContainsNoCase(PCWSTR text, PCWSTR part) {
		return CString(text).MakeLower().Find(CString(part).MakeLower()) >= 0;
	}

	CString FormatSystemTime(SYSTEMTIME const& st) {
		WCHAR date[64], time[64];
		if (::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, _countof(date), nullptr) == 0
			|| ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, nullptr, time, _countof(time)) == 0)
			return L"";
		return CString(date) + L" " + time;
	}

	// 1980..2100, to tell real times from arbitrary 8 bytes
	bool IsPlausibleTime(ULONGLONG value) {
		static auto const [low, high] = [] {
			SYSTEMTIME st{ 1980, 1, 0, 1 };
			FILETIME ft1, ft2;
			::SystemTimeToFileTime(&st, &ft1);
			st.wYear = 2100;
			::SystemTimeToFileTime(&st, &ft2);
			return std::pair(ULARGE_INTEGER{ ft1.dwLowDateTime, ft1.dwHighDateTime }.QuadPart,
				ULARGE_INTEGER{ ft2.dwLowDateTime, ft2.dwHighDateTime }.QuadPart);
		}();
		return value >= low && value < high;
	}
}

CString ValueDecoder::FormatNumber(ULONGLONG value, int bytes, bool decimalFirst) {
	CString hex;
	if (bytes <= 4 || value < (1ULL << 32))
		hex.Format(L"0x%08llX", value);
	else
		hex.Format(L"0x%016llX", value);
	CString text;
	if (decimalFirst)
		text.Format(L"%llu (%s)", value, (PCWSTR)hex);
	else
		text.Format(L"%s (%llu)", (PCWSTR)hex, value);
	return text;
}

CString ValueDecoder::DecodeFileTime(BYTE const* data, DWORD size) {
	if (size != sizeof(FILETIME))
		return L"";
	ULONGLONG value;
	memcpy(&value, data, sizeof(value));
	if (!IsPlausibleTime(value))
		return L"";

	FILETIME ft{ (DWORD)value, (DWORD)(value >> 32) }, local;
	SYSTEMTIME st;
	if (!::FileTimeToLocalFileTime(&ft, &local) || !::FileTimeToSystemTime(&local, &st))
		return L"";
	return FormatSystemTime(st);
}

CString ValueDecoder::DecodeSystemTime(BYTE const* data, DWORD size) {
	if (size != sizeof(SYSTEMTIME))
		return L"";
	SYSTEMTIME st;
	memcpy(&st, data, sizeof(st));
	// strict, so random 16 bytes (e.g. a GUID) aren't taken for a time
	if (st.wYear < 1980 || st.wYear >= 2100 || st.wMonth < 1 || st.wMonth > 12 || st.wDayOfWeek > 6 || st.wDay < 1 || st.wDay > 31
		|| st.wHour > 23 || st.wMinute > 59 || st.wSecond > 59 || st.wMilliseconds > 999)
		return L"";
	FILETIME ft;
	if (!::SystemTimeToFileTime(&st, &ft))
		return L"";
	return FormatSystemTime(st);
}

CString ValueDecoder::DecodeGuid(BYTE const* data, DWORD size) {
	if (size != sizeof(GUID))
		return L"";
	GUID guid;
	memcpy(&guid, data, sizeof(guid));
	return Helpers::GuidToString(guid);
}

CString ValueDecoder::DecodeSid(BYTE const* data, DWORD size) {
	// revision 1, sub authority count, 6 byte authority, then the sub authorities
	if (size < 8 || data[0] != SID_REVISION || data[1] > SID_MAX_SUB_AUTHORITIES || size != 8 + 4u * data[1])
		return L"";
	auto sid = (PSID)data;
	if (!::IsValidSid(sid))
		return L"";

	PWSTR str;
	if (!::ConvertSidToStringSid(sid, &str))
		return L"";
	CString text(str);
	::LocalFree(str);

	// account (S-1-5-21-...) lookups may query a domain controller, so only well-known SIDs are named, never going
	// to the network while painting; results are cached
	auto authority = ::GetSidIdentifierAuthority(sid);
	bool account = authority->Value[5] == 5 && *::GetSidSubAuthorityCount(sid) > 0 && *::GetSidSubAuthority(sid, 0) == 21;
	if (!account) {
		static std::map<CString, CString> names;
		auto it = names.find(text);
		if (it == names.end()) {
			CString display;
			WCHAR name[256], domain[256];
			DWORD nameLen = _countof(name), domainLen = _countof(domain);
			SID_NAME_USE use;
			if (::LookupAccountSid(nullptr, sid, name, &nameLen, domain, &domainLen, &use))
				display = *domain ? CString(domain) + L"\\" + name : CString(name);
			it = names.insert({ text, display }).first;
		}
		if (!it->second.IsEmpty())
			text += L" (" + it->second + L")";
	}
	return text;
}

CString ValueDecoder::DecodeSecurityDescriptor(BYTE const* data, DWORD size) {
	if (size < SECURITY_DESCRIPTOR_MIN_LENGTH || !::RtlValidRelativeSecurityDescriptor((PSECURITY_DESCRIPTOR)data, size, 0))
		return L"";
	PWSTR sddl;
	if (!::ConvertSecurityDescriptorToStringSecurityDescriptor((PSECURITY_DESCRIPTOR)data, SDDL_REVISION_1,
		OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &sddl, nullptr))
		return L"";
	CString text(sddl);
	::LocalFree(sddl);
	return text;
}

CString ValueDecoder::DecodeStrings(BYTE const* data, DWORD size) {
	if (size < 8 || size % sizeof(WCHAR))
		return L"";
	auto chars = (PCWSTR)data;
	auto count = size / sizeof(WCHAR);
	if (chars[count - 1] != 0)
		return L"";

	// all strings printable, separated by single NULLs, ending with one or two NULLs
	while (count > 1 && chars[count - 2] == 0)
		count--;
	std::vector<CString> strings;
	CString current;
	int printable = 0;
	for (size_t i = 0; i < count - 1; i++) {
		auto ch = chars[i];
		if (ch == 0) {
			if (current.IsEmpty())
				return L"";
			strings.push_back(current);
			current.Empty();
			continue;
		}
		if (!((ch >= 0x20 && ch < 0x7F) || ch == L'\t' || (ch >= 0xA0 && ch < 0xD800) || (ch >= 0xE000 && ch <= 0xFFFD)))
			return L"";
		current += ch;
		printable++;
	}
	if (current.IsEmpty() || printable < 3)
		return L"";
	strings.push_back(current);

	CString text;
	for (auto& s : strings) {
		if (!text.IsEmpty())
			text += L" | ";
		text += L"\"" + s + L"\"";
		if (text.GetLength() > 512)
			break;
	}
	return text;
}

CString ValueDecoder::DecodeFullResourceDescriptor(BYTE const* data, DWORD size, DWORD* used) {
	if (size < sizeof(FullDescriptorHeader))
		return L"";
	FullDescriptorHeader header;
	memcpy(&header, data, sizeof(header));
	if (header.Count > (size - sizeof(header)) / sizeof(PartialDescriptor))
		return L"";

	CString text = InterfaceTypeName(header.InterfaceType);
	text.AppendFormat(L" bus %u", header.BusNumber);
	DWORD offset = sizeof(header) + header.Count * sizeof(PartialDescriptor);
	CString parts;
	for (ULONG i = 0; i < header.Count; i++) {
		PartialDescriptor d;
		memcpy(&d, data + sizeof(header) + i * sizeof(PartialDescriptor), sizeof(d));
		if (d.Type == DeviceSpecific) {
			// its data follows the descriptors
			if (d.u.DeviceSpecificData.DataSize > size - offset)
				return L"";
			offset += d.u.DeviceSpecificData.DataSize;
		}
		parts += (parts.IsEmpty() ? L"" : L"; ") + FormatPartial(d);
	}
	if (!parts.IsEmpty())
		text += L": " + parts;
	if (used)
		*used = offset;
	return text;
}

CString ValueDecoder::DecodeResourceList(BYTE const* data, DWORD size) {
	if (size < sizeof(ULONG))
		return L"";
	ULONG count;
	memcpy(&count, data, sizeof(count));
	if (count == 0)
		return size == sizeof(ULONG) ? L"(no resources)" : L"";

	CString text;
	DWORD offset = sizeof(ULONG);
	for (ULONG i = 0; i < count; i++) {
		DWORD used = 0;
		auto full = DecodeFullResourceDescriptor(data + offset, size - offset, &used);
		if (full.IsEmpty())
			return L"";
		text += (text.IsEmpty() ? L"" : L" | ") + full;
		offset += used;
	}
	return text;
}

CString ValueDecoder::DecodeRequirementsList(BYTE const* data, DWORD size) {
	if (size < sizeof(RequirementsListHeader))
		return L"";
	RequirementsListHeader header;
	memcpy(&header, data, sizeof(header));
	if (header.ListSize > size)
		return L"";

	CString text = InterfaceTypeName(header.InterfaceType);
	text.AppendFormat(L" bus %u", header.BusNumber);
	if (header.AlternativeLists > 1)
		text.AppendFormat(L", %u alternatives", header.AlternativeLists);

	DWORD offset = sizeof(header);
	for (ULONG list = 0; list < header.AlternativeLists; list++) {
		if (size - offset < sizeof(IoListHeader))
			return L"";
		IoListHeader io;
		memcpy(&io, data + offset, sizeof(io));
		offset += sizeof(io);
		if (io.Count > (size - offset) / sizeof(IoDescriptor))
			return L"";
		// show the first alternative; the others only need to be valid
		if (list == 0) {
			CString parts;
			for (ULONG i = 0; i < io.Count; i++) {
				IoDescriptor d;
				memcpy(&d, data + offset + i * sizeof(IoDescriptor), sizeof(d));
				if (d.Type == 0 || d.Type >= 0x80)
					continue;	// null and private descriptors carry no resources
				parts += (parts.IsEmpty() ? L"" : L"; ") + FormatIo(d);
			}
			if (!parts.IsEmpty())
				text += L": " + parts;
		}
		offset += io.Count * sizeof(IoDescriptor);
	}
	return text;
}

CString ValueDecoder::Decode(PCWSTR name, DWORD type, BYTE const* data, DWORD size) {
	if (!data && size)
		return L"";
	CString text;
	switch (type) {
		case REG_DWORD:
			if (size == sizeof(DWORD)) {
				LONG value;
				memcpy(&value, data, sizeof(value));
				if (value < 0)
					text.Format(L"Signed: %d", value);
			}
			return text;

		case REG_QWORD:
			if (size == sizeof(ULONGLONG)) {
				if (ContainsNoCase(name, L"time") || ContainsNoCase(name, L"date") || ContainsNoCase(name, L"stamp")
					|| ContainsNoCase(name, L"last") || ContainsNoCase(name, L"install")) {
					text = DecodeFileTime(data, size);
					if (!text.IsEmpty())
						return text;
				}
				LONGLONG value;
				memcpy(&value, data, sizeof(value));
				if (value < 0)
					text.Format(L"Signed: %lld", value);
			}
			return text;

		case REG_MULTI_SZ:
		{
			// count the strings; the list ends with an empty string
			auto chars = (PCWSTR)data;
			auto count = size / sizeof(WCHAR);
			int strings = 0;
			for (size_t i = 0; i < count && chars[i]; i++) {
				strings++;
				while (i < count && chars[i])
					i++;
			}
			if (strings > 1)
				text.Format(L"%d strings", strings);
			return text;
		}

		case REG_RESOURCE_LIST:
			return DecodeResourceList(data, size);
		case REG_FULL_RESOURCE_DESCRIPTOR:
			return DecodeFullResourceDescriptor(data, size);
		case REG_RESOURCE_REQUIREMENTS_LIST:
			return DecodeRequirementsList(data, size);

		case REG_SZ:
		case REG_EXPAND_SZ:
		case REG_LINK:
			return text;
	}

	// binary and other types: guess from the data, most specific first
	for (auto decoder : { DecodeFileTime, DecodeSystemTime, DecodeSid, DecodeSecurityDescriptor, DecodeStrings, DecodeGuid }) {
		text = decoder(data, size);
		if (!text.IsEmpty())
			return text;
	}
	return text;
}
