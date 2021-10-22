#pragma once

#include <shared_mutex>

struct ImageIconCache {
	static ImageIconCache& Get();

	HIMAGELIST GetImageList() const;
	int GetIcon(const CString& path, HICON* phIcon = nullptr) const;

	using Map = std::unordered_map<std::wstring, int>;
	Map::const_iterator begin() const;
	Map::const_iterator end() const;

private:
	ImageIconCache();
	ImageIconCache(const ImageIconCache&) = delete;
	ImageIconCache& operator=(const ImageIconCache&) = delete;

private:
	mutable CImageList _images;
	mutable Map _icons;
	mutable std::shared_mutex _lock;
};
