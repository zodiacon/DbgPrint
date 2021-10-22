#include "pch.h"
#include "ImageIconCache.h"
#include "resource.h"

int ImageIconCache::GetIcon(const CString& path, HICON* phIcon) const {
	std::wstring wspath(path);
	{
		std::shared_lock locker(_lock);
		auto it = _icons.find(wspath);
		if (it != _icons.end()) {
			int index = it->second;
			if (phIcon)
				*phIcon = _images.GetIcon(index);
			return index;
		}
	}
	WORD index = 0;
	CString spath(path);
	auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), spath.GetBufferSetLength(MAX_PATH), &index);

	if (hIcon) {
		std::lock_guard locker(_lock);
		int index = _images.AddIcon(hIcon);
		if (phIcon)
			*phIcon = hIcon;
		_icons.insert({ wspath, index });
		return index;
	}
	return 0;
}

ImageIconCache::Map::const_iterator ImageIconCache::begin() const {
	return _icons.begin();
}

ImageIconCache::Map::const_iterator ImageIconCache::end() const {
	return _icons.end();
}

ImageIconCache::ImageIconCache() {
	_images.Create(16, 16, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 16, 16);
	_images.AddIcon(AtlLoadSysIcon(IDI_APPLICATION));
	_images.AddIcon(AtlLoadIconImage(IDI_KERNEL, 0, 16, 16));
}

HIMAGELIST ImageIconCache::GetImageList() const {
	return _images;
}

ImageIconCache& ImageIconCache::Get() {
	static ImageIconCache cache;
	return cache;
}

