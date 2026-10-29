#include "pch.h"
#include "ImageIconCache.h"
#include "resource.h"

int ImageIconCache::GetIcon(std::wstring_view path, HICON* phIcon) const {
	if (path.empty() || path.find(L'\\') == std::wstring::npos)
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
		//
		// another thread may have added the same path in the meantime
		//
		int index;
		if (auto it = m_icons.find(wspath); it != m_icons.end()) {
			index = it->second;
		}
		else {
			index = m_images.AddIcon(hIcon);
			m_icons.insert({ wspath, index });
		}
		//
		// the image list keeps its own copy of the icon
		//
		if (phIcon)
			*phIcon = hIcon;
		else
			::DestroyIcon(hIcon);
		return index;
	}
	return 0;
}

bool ImageIconCache::Load(IStream* stm) {
	CImageList images(CImageList::Read(stm));
	if (!images)
		return false;

	//
	// copy into the existing image list, so controls already using it stay valid
	//
	std::lock_guard locker(m_lock);
	m_images.RemoveAll();
	m_icons.clear();
	int count = images.GetImageCount();
	for (int i = 0; i < count; i++) {
		auto hIcon = images.GetIcon(i);
		m_images.AddIcon(hIcon);
		::DestroyIcon(hIcon);
	}
	images.Destroy();
	return true;
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


