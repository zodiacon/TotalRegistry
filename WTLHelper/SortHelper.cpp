#include "pch.h"
#include "SortHelper.h"

bool SortHelper::Sort(const CString& s1, const CString& s2, bool ascending) {
	if (s1.IsEmpty() && s2.IsEmpty())
		return false;
	if (s1.IsEmpty())
		return false;
	if (s2.IsEmpty())
		return true;

	return ascending ? s2.CompareNoCase(s1) > 0 : s2.CompareNoCase(s1) < 0;
}

bool SortHelper::Sort(const std::string& s1, const std::string& s2, bool ascending) {
	if (s1.empty() && s2.empty())
		return false;
	if (s1.empty())
		return false;
	if (s2.empty())
		return true;

	auto compare = ::_stricmp(s2.c_str(), s1.c_str());
	return ascending ? compare > 0 : compare < 0;
}

bool SortHelper::Sort(const std::wstring& s1, const std::wstring& s2, bool ascending) {
	if (s1.empty() && s2.empty())
		return false;
	if (s1.empty())
		return false;
	if (s2.empty())
		return true;

	auto compare = ::_wcsicmp(s2.c_str(), s1.c_str());
	return ascending ? compare > 0 : compare < 0;
}

bool SortHelper::Sort(PCWSTR s1, PCWSTR s2, bool ascending) {
	if ((s1 == nullptr || *s1 == 0) && (s2 == nullptr || *s2 == 0))
		return false;
	if (s1 == nullptr || *s1 == 0)
		return false;
	if (s2 == nullptr || *s2 == 0)
		return true;

	auto compare = ::_wcsicmp(s2, s1);
	return ascending ? compare > 0 : compare < 0;
}

bool SortHelper::Sort(bool a, bool b, bool asc) {
	return asc ? b > a : a > b;
}
