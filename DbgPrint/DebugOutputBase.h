#pragma once

struct IDebugOutput {
	virtual void DebugOutput(DWORD pid, PCSTR text, FILETIME const&) = 0;
};

struct DebugOutputBase abstract {
	virtual bool Run(IDebugOutput* sink) = 0;
	virtual bool Stop() = 0;
	virtual bool IsRunning() const = 0;
};