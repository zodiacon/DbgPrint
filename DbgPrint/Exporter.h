#pragma once

#include "DebugOutputBase.h"

class Exporter final abstract {
public:
	bool Save(std::vector<std::unique_ptr<DebugItem>> const& items, PCWSTR file);
	std::vector<std::unique_ptr<DebugItem>> Load(PCWSTR file);
};

