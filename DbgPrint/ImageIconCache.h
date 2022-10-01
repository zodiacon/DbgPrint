#pragma once

#include <shared_mutex>

class ImageIconCache {
public:
	ImageIconCache();
	ImageIconCache(const ImageIconCache&) = delete;
	ImageIconCache& operator=(const ImageIconCache&) = delete;

	HIMAGELIST GetImageList() const;
	int GetIcon(std::wstring_view path, HICON* phIcon = nullptr) const;

	using Map = std::unordered_map<std::wstring, int>;
	Map::const_iterator begin() const;
	Map::const_iterator end() const;

private:
	mutable CImageList m_images;
	mutable Map m_icons;
	mutable std::shared_mutex m_lock;
};
