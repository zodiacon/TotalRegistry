#pragma once

#include <string>
#include <atlstr.h>

struct SortHelper final abstract {
	static bool Sort(const ATL::CString& s1, const ATL::CString& s2, bool ascending);
	static bool Sort(const std::string& s1, const std::string& s2, bool ascending);
	static bool Sort(const std::wstring& s1, const std::wstring& s2, bool ascending);
	static bool Sort(PCWSTR s1, PCWSTR s2, bool ascending);
	static bool Sort(bool a, bool b, bool asc);

	template<typename Number>
	static bool Sort(const Number& n1, const Number& n2, bool ascending) {
		return ascending ? n2 > n1 : n2 < n1;
	}
};
