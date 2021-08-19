#include "pch.h"
#include "ImageIconCache.h"

int ImageIconCache::GetIconIndex(CString const& path) const {
	if (auto it = _icons.find(path); it != _icons.end())
		return it->second;

	auto hIcon = ::ExtractIcon(ModuleHelper::GetModuleInstance(), path, 0);
	if (hIcon) {
		int index;
		_icons.insert({ path, index = _images.AddIcon(hIcon) });
		return index;
	}
	_icons.insert({ path, 0 });
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
	return _images.m_hImageList;
}

ImageIconCache& ImageIconCache::Get() {
	static ImageIconCache cache;
	return cache;
}

void ImageIconCache::Destroy() {
	_images.Destroy();
}

ImageIconCache::ImageIconCache() {
	_images.Create(16, 16, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 50, 10);
	_images.AddIcon(AtlLoadSysIcon(IDI_APPLICATION));
}
