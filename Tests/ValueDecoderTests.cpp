#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "ValueDecoder.h"
#include "Registry.h"
#include <sddl.h>
#include <random>

using namespace ValueDecoder;

namespace {
	// little-endian byte builder for the resource structures
	struct Bytes {
		std::vector<BYTE> data;
		Bytes& u8(BYTE v) { data.push_back(v); return *this; }
		Bytes& u16(USHORT v) { return raw(&v, sizeof(v)); }
		Bytes& u32(ULONG v) { return raw(&v, sizeof(v)); }
		Bytes& u64(ULONGLONG v) { return raw(&v, sizeof(v)); }
		Bytes& raw(void const* p, size_t size) { data.insert(data.end(), (BYTE const*)p, (BYTE const*)p + size); return *this; }
	};

	// CM_PARTIAL_RESOURCE_DESCRIPTOR (x64: 20 bytes)
	Bytes& Range(Bytes& b, BYTE type, ULONGLONG start, ULONG length, USHORT flags = 0) {
		return b.u8(type).u8(0).u16(flags).u64(start).u32(length).u32(0);
	}
	Bytes& Irq(Bytes& b, USHORT level, ULONG vector, USHORT flags = 0) {
		return b.u8(2).u8(0).u16(flags).u16(level).u16(0).u32(vector).u64(1);
	}
	// CM_FULL_RESOURCE_DESCRIPTOR header
	Bytes& FullHeader(Bytes& b, LONG interfaceType, ULONG bus, ULONG count) {
		return b.u32(interfaceType).u32(bus).u16(1).u16(1).u32(count);
	}

	std::vector<BYTE> FileTimeBytes(WORD year, WORD month, WORD day) {
		SYSTEMTIME st{ year, month, 0, day, 12 };
		FILETIME ft;
		::SystemTimeToFileTime(&st, &ft);
		return Bytes().raw(&ft, sizeof(ft)).data;
	}

	CString DecodeAs(DWORD type, std::vector<BYTE> const& data, PCWSTR name = L"value") {
		return Decode(name, type, data.data(), (DWORD)data.size());
	}
}

TEST_CASE("Numbers in hex and decimal", "[decoder]") {
	CHECK(FormatNumber(42, 4, false) == L"0x0000002A (42)");
	CHECK(FormatNumber(42, 4, true) == L"42 (0x0000002A)");
	CHECK(FormatNumber(0x123456789, 8, false) == L"0x0000000123456789 (4886718345)");
	CHECK(FormatNumber(7, 8, false) == L"0x00000007 (7)");
	CHECK(FormatNumber(0xFFFFFFFF, 4, true) == L"4294967295 (0xFFFFFFFF)");
}

TEST_CASE("Times", "[decoder]") {
	SECTION("FILETIME in a plausible range") {
		auto data = FileTimeBytes(2024, 5, 1);
		CHECK(DecodeFileTime(data.data(), (DWORD)data.size()).Find(L"2024") >= 0);
		ULONGLONG zero = 0, huge = ~0ULL;
		CHECK(DecodeFileTime((BYTE*)&zero, 8).IsEmpty());
		CHECK(DecodeFileTime((BYTE*)&huge, 8).IsEmpty());
		CHECK(DecodeFileTime(data.data(), 4).IsEmpty());
	}

	SECTION("SYSTEMTIME with valid fields only") {
		SYSTEMTIME st{ 2023, 12, 3, 31, 23, 59, 59, 999 };
		CHECK(DecodeSystemTime((BYTE*)&st, sizeof(st)).Find(L"2023") >= 0);
		st.wMonth = 13;
		CHECK(DecodeSystemTime((BYTE*)&st, sizeof(st)).IsEmpty());
		GUID guid = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };
		CHECK(DecodeSystemTime((BYTE*)&guid, sizeof(guid)).IsEmpty());
	}
}

TEST_CASE("GUIDs, SIDs and security descriptors", "[decoder]") {
	SECTION("GUID") {
		GUID guid = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };
		CHECK(DecodeGuid((BYTE*)&guid, sizeof(guid)).CompareNoCase(L"{12345678-9ABC-DEF0-0102-030405060708}") == 0);
		CHECK(DecodeGuid((BYTE*)&guid, 15).IsEmpty());
	}

	SECTION("well-known SIDs are named") {
		BYTE sid[SECURITY_MAX_SID_SIZE];
		DWORD size = sizeof(sid);
		REQUIRE(::CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sid, &size));
		auto text = DecodeSid(sid, size);
		// the name is localized
		CHECK(text.Find(L"S-1-5-32-544 (") == 0);
		CHECK(text.Right(1) == L")");
		CHECK(DecodeSid(sid, size - 1).IsEmpty());
		CHECK(DecodeSid(sid, size + 4).IsEmpty());
	}

	SECTION("account SIDs are not looked up") {
		PSID sid;
		REQUIRE(::ConvertStringSidToSid(L"S-1-5-21-1-2-3-500", &sid));
		CHECK(DecodeSid((BYTE*)sid, ::GetLengthSid(sid)) == L"S-1-5-21-1-2-3-500");
		::LocalFree(sid);
	}

	SECTION("self-relative security descriptors") {
		PSECURITY_DESCRIPTOR sd;
		ULONG size;
		REQUIRE(::ConvertStringSecurityDescriptorToSecurityDescriptor(L"O:BAG:SYD:(A;;KA;;;SY)", SDDL_REVISION_1, &sd, &size));
		CHECK(DecodeSecurityDescriptor((BYTE*)sd, size) == L"O:BAG:SYD:(A;;KA;;;SY)");
		CHECK(DecodeSecurityDescriptor((BYTE*)sd, size - 8).IsEmpty());
		::LocalFree(sd);
	}
}

TEST_CASE("Strings stored in binary values", "[decoder]") {
	WCHAR two[] = L"one\0two\0";
	CHECK(DecodeStrings((BYTE*)two, sizeof(two)) == L"\"one\" | \"two\"");
	WCHAR one[] = L"C:\\Windows";
	CHECK(DecodeStrings((BYTE*)one, sizeof(one)) == L"\"C:\\Windows\"");
	WCHAR control[] = L"ab\x0001";
	CHECK(DecodeStrings((BYTE*)control, sizeof(control)).IsEmpty());
	CHECK(DecodeStrings((BYTE*)one, sizeof(one) - 2).IsEmpty());		// not terminated
	WCHAR shortText[] = L"ab";
	CHECK(DecodeStrings((BYTE*)shortText, sizeof(shortText)).IsEmpty());
	WCHAR gap[] = L"one\0\0two";
	CHECK(DecodeStrings((BYTE*)gap, sizeof(gap)).IsEmpty());
}

TEST_CASE("Decode picks an interpretation by type and data", "[decoder]") {
	CHECK(DecodeAs(REG_BINARY, FileTimeBytes(2024, 5, 1)).Find(L"2024") >= 0);
	GUID guid = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };
	CHECK(DecodeAs(REG_BINARY, Bytes().raw(&guid, sizeof(guid)).data).Left(1) == L"{");
	CHECK(DecodeAs(REG_BINARY, { 1, 2, 3 }).IsEmpty());
	CHECK(DecodeAs(REG_NONE, FileTimeBytes(2024, 5, 1)).Find(L"2024") >= 0);

	CHECK(DecodeAs(REG_DWORD, Bytes().u32(0xFFFFFFFF).data) == L"Signed: -1");
	CHECK(DecodeAs(REG_DWORD, Bytes().u32(5).data).IsEmpty());

	// QWORDs are only taken for times when the name says so
	CHECK(DecodeAs(REG_QWORD, FileTimeBytes(2024, 5, 1), L"InstallTime").Find(L"2024") >= 0);
	CHECK(DecodeAs(REG_QWORD, FileTimeBytes(2024, 5, 1), L"Count").IsEmpty());
	CHECK(DecodeAs(REG_QWORD, Bytes().u64(~0ULL).data, L"Count") == L"Signed: -1");

	WCHAR multi[] = L"a\0b\0c\0";
	CHECK(DecodeAs(REG_MULTI_SZ, Bytes().raw(multi, sizeof(multi)).data) == L"3 strings");
	CHECK(DecodeAs(REG_SZ, Bytes().raw(multi, sizeof(multi)).data).IsEmpty());
}

TEST_CASE("Resource lists", "[decoder][resources]") {
	SECTION("REG_RESOURCE_LIST") {
		Bytes b;
		b.u32(1);
		FullHeader(b, 5, 0, 3);
		Range(b, 3, 0xFE000000, 0x400000);
		Range(b, 1, 0x3F8, 8);
		Irq(b, 16, 16);
		CHECK(DecodeAs(REG_RESOURCE_LIST, b.data) == L"PCI bus 0: Memory 0xFE000000-0xFE3FFFFF; I/O 0x3F8-0x3FF; Interrupt 16");
	}

	SECTION("REG_FULL_RESOURCE_DESCRIPTOR with device specific data and large memory") {
		Bytes b;
		FullHeader(b, 0, 2, 3);
		Range(b, 7, 0x100000000, 0x10, 0x200);		// length in 256 byte units
		Irq(b, 0, 40, 0x2);
		b.u8(5).u8(0).u16(0).u32(4).u32(0).u32(0).u32(0);	// device specific: 4 bytes of data follow the descriptors
		b.u32(0xDEADBEEF);
		CHECK(DecodeAs(REG_FULL_RESOURCE_DESCRIPTOR, b.data) == L"Internal bus 2: Memory 0x100000000-0x100000FFF; Interrupt 40 (MSI); Device data (4 bytes)");

		auto truncated = b.data;
		truncated.resize(truncated.size() - 1);
		CHECK(DecodeAs(REG_FULL_RESOURCE_DESCRIPTOR, truncated).IsEmpty());
	}

	SECTION("REG_RESOURCE_REQUIREMENTS_LIST") {
		Bytes b;
		b.u32(32 + 8 + 2 * 32).u32(5).u32(0).u32(0).u32(0).u32(0).u32(0).u32(1);
		b.u16(1).u16(1).u32(2);
		b.u8(0).u8(3).u8(0).u8(0).u16(0).u16(0).u32(0x1000).u32(0x1000).u64(0x1000).u64(0x1FFF);
		b.u8(0).u8(2).u8(0).u8(0).u16(0).u16(0).u32(5).u32(9).u32(0).u32(0).u64(0);
		CHECK(DecodeAs(REG_RESOURCE_REQUIREMENTS_LIST, b.data) == L"PCI bus 0: Memory 0x1000-0x1FFF length 0x1000; Interrupt 5-9");
	}

	SECTION("bad counts are rejected") {
		Bytes b;
		b.u32(1);
		FullHeader(b, 5, 0, 1000);
		Range(b, 3, 0, 1);
		CHECK(DecodeAs(REG_RESOURCE_LIST, b.data).IsEmpty());
		CHECK(DecodeAs(REG_RESOURCE_LIST, Bytes().u32(0).data) == L"(no resources)");
	}
}

TEST_CASE("Decoders never read past the data", "[decoder]") {
	Bytes b;
	b.u32(1);
	FullHeader(b, 5, 0, 2);
	Range(b, 3, 0xFE000000, 0x400000);
	b.u8(5).u8(0).u16(0).u32(4).u32(0).u32(0).u32(0).u32(0);

	DWORD const types[] = { REG_NONE, REG_BINARY, REG_DWORD, REG_QWORD, REG_MULTI_SZ, REG_RESOURCE_LIST, REG_FULL_RESOURCE_DESCRIPTOR, REG_RESOURCE_REQUIREMENTS_LIST };
	SECTION("every truncation of valid data") {
		for (auto type : types) {
			for (size_t size = 0; size <= b.data.size(); size++) {
				// a copy of exactly this size, so reading past it is caught by the debug heap
				std::vector<BYTE> copy(b.data.begin(), b.data.begin() + size);
				Decode(L"value", type, copy.data(), (DWORD)copy.size());
			}
		}
	}

	SECTION("random data") {
		std::mt19937 rng(12345);
		for (int i = 0; i < 20000; i++) {
			std::vector<BYTE> data(rng() % 200);
			for (auto& byte : data)
				byte = (BYTE)rng();
			// sometimes plausible counts, to get past the first checks
			if (data.size() >= 4 && i % 2)
				data[0] = 1, data[1] = data[2] = data[3] = 0;
			Decode(L"value", types[i % _countof(types)], data.data(), (DWORD)data.size());
		}
	}
	SUCCEED();
}

TEST_CASE("Real resource data decodes", "[decoder][resources]") {
	// the hardware resource map and the system's configuration data are readable without elevation
	int found = 0, decoded = 0;
	std::function<void(HKEY, int)> walk = [&](HKEY hKey, int depth) {
		Registry::EnumKeyValues(hKey, [&](auto type, auto name, auto size) {
			if (type == REG_RESOURCE_LIST || type == REG_FULL_RESOURCE_DESCRIPTOR || type == REG_RESOURCE_REQUIREMENTS_LIST) {
				std::vector<BYTE> data(size);
				DWORD bytes = size;
				if (ERROR_SUCCESS == ::RegQueryValueEx(hKey, name, nullptr, nullptr, data.data(), &bytes)) {
					found++;
					auto text = Decode(name, type, data.data(), bytes);
					if (!text.IsEmpty())
						decoded++;
					else
						UNSCOPED_INFO("not decoded: " << TestHelpers::ToUtf8(name) << " type " << type << " size " << bytes);
				}
			}
			return true;
			});
		if (depth < 6)
			Registry::EnumSubKeys(hKey, [&](auto name, const auto&) {
				CRegKey sub;
				if (ERROR_SUCCESS == sub.Open(hKey, name, KEY_READ))
					walk(sub, depth + 1);
				return true;
				});
	};
	for (auto path : { L"HARDWARE\\RESOURCEMAP", L"HARDWARE\\DESCRIPTION\\System" }) {
		CRegKey key;
		if (ERROR_SUCCESS == key.Open(HKEY_LOCAL_MACHINE, path, KEY_READ))
			walk(key, 0);
	}
	if (found == 0)
		SKIP("no resource values readable on this machine");
	INFO(decoded << " of " << found << " decoded");
	CHECK(decoded == found);
}
