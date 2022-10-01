#include "pch.h"
#include "Interfaces.h"

std::wostream& operator<<(std::wostream& out, ProcessKey const& key) {
	 out << key.ProcessId << L":" << *(uint64_t*)&key.StartTime;
	 return out;
}
