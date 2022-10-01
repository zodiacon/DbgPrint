#include "pch.h"
#include "ImageIconCache.h"
#include "resource.h"

int ImageIconCache::GetIcon(std::wstring_view path, HICON* phIcon) const {
	if (path.empty() || path.find(L'\\') != std::wstring::npos)
		return 0;

	std::wstring wspath(path);
	{
		std::shared_lock locker(m_lock);
		auto it = m_icons.find(wspath);
		if (it != m_icons.end()) {
			int index = it->second;
			if (phIcon)
				*phIcon = m_images.GetIcon(index);
			return index;
		}
	}
	WORD index = 0;
	CString spath(path.data());
	auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), spath.GetBufferSetLength(MAX_PATH), &index);

	if (hIcon) {
		std::lock_guard locker(m_lock);
		int index = m_images.AddIcon(hIcon);
		if (phIcon)
			*phIcon = hIcon;
		m_icons.insert({ wspath, index });
		return index;
	}
	return 0;
}

ImageIconCache::Map::const_iterator ImageIconCache::begin() const {
	return m_icons.begin();
}

ImageIconCache::Map::const_iterator ImageIconCache::end() const {
	return m_icons.end();
}

ImageIconCache::ImageIconCache() {
	m_images.Create(16, 16, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 16, 16);
	m_images.AddIcon(AtlLoadSysIcon(IDI_APPLICATION));
	m_images.AddIcon(AtlLoadIconImage(IDI_KERNEL, 0, 16, 16));
	m_images.AddIcon(AtlLoadIconImage(IDI_USERMODE, 0, 16, 16));
	m_images.AddIcon(AtlLoadIconImage(IDI_BOOKMARK, 0, 16, 16));
}

HIMAGELIST ImageIconCache::GetImageList() const {
	return m_images;
}


