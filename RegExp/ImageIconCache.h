#pragma once

struct ImageIconCache {
	int GetIconIndex(CString const& path) const;
	int GetIconIndex(DWORD pid) const;
	HIMAGELIST GetImageList() const;
	static ImageIconCache& Get();
	void Destroy();

private:
	ImageIconCache();
	mutable CImageListManaged _images;
	mutable std::map<CString, int> _icons;
};
