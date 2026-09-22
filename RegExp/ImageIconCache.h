#pragma once

struct ImageIconCache {
	int GetIconIndex(CString const& path) const;
	int GetIconIndex(DWORD pid) const;
	HIMAGELIST GetImageList() const;
	static ImageIconCache& Get();
	void Destroy();

private:
	ImageIconCache();
	mutable CImageListManaged m_Images;
	mutable std::map<CString, int> m_Icons;
};
