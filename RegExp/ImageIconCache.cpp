#include "pch.h"
#include "ImageIconCache.h"

int ImageIconCache::GetIconIndex(CString const& path) const {
	if (auto it = m_Icons.find(path); it != m_Icons.end())
		return it->second;

	auto hIcon = ::ExtractIcon(ModuleHelper::GetModuleInstance(), path, 0);
	if (hIcon) {
		int index;
		m_Icons.insert({ path, index = m_Images.AddIcon(hIcon) });
		return index;
	}
	m_Icons.insert({ path, 0 });
	return 0;
}

int ImageIconCache::GetIconIndex(DWORD pid) const {
	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if(!hProcess)
		return 0;

	WCHAR path[MAX_PATH * 2];
	DWORD size = _countof(path);
	auto count = ::QueryFullProcessImageName(hProcess, 0, path, &size);
	::CloseHandle(hProcess);
	return count > 0 ? GetIconIndex(path) : 0;
}

HIMAGELIST ImageIconCache::GetImageList() const {
	return m_Images.m_hImageList;
}

ImageIconCache& ImageIconCache::Get() {
	static ImageIconCache cache;
	return cache;
}

void ImageIconCache::Destroy() {
	m_Images.Destroy();
}

ImageIconCache::ImageIconCache() {
	m_Images.Create(16, 16, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 50, 10);
	m_Images.AddIcon(AtlLoadSysIcon(IDI_APPLICATION));
}
