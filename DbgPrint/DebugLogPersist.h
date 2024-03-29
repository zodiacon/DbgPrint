#pragma once

#include <span>

struct DebugItem;
class ImageIconCache;
class ProcessManager;

enum class PersistFormat {
	Native,
	CSV,
};

class DebugLogPersist abstract final {
public:
	static bool Save(PersistFormat format, std::vector<std::shared_ptr<DebugItem>> const& items, ImageIconCache const& iconCache, ProcessManager const& pm, std::wstring_view path);
	static bool Load(std::wstring_view path, std::vector<std::shared_ptr<DebugItem>>& items, ProcessManager& pm);

private:
	static bool SaveCSV(std::vector<std::shared_ptr<DebugItem>> const& items, std::wstring_view path);
	static bool SaveNative(std::vector<std::shared_ptr<DebugItem>> const& items, ImageIconCache const& iconCache, ProcessManager const& pm, std::wstring_view path);
};

