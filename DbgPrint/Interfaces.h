#pragma once

struct ProcessKey {
	DWORD ProcessId;
	FILETIME StartTime{};
	bool operator==(const ProcessKey& other) const {
		return other.ProcessId == ProcessId &&
			other.StartTime.dwLowDateTime == StartTime.dwLowDateTime &&
			other.StartTime.dwHighDateTime == StartTime.dwHighDateTime;
	}
};

template<>
struct std::hash<ProcessKey> {
	size_t operator()(ProcessKey const& key) const {
		return (key.ProcessId << 16LL) ^ key.StartTime.dwLowDateTime ^ (key.StartTime.dwHighDateTime << 16LL);
	}
};

struct IMainFrame {
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y, HWND hWnd = nullptr) = 0;
};
