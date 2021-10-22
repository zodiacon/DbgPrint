#pragma once

#include "DebugOutputBase.h"
#include <wil\resource.h>

class UserModeDebugOutput : public DebugOutputBase {
public:
	UserModeDebugOutput(PCWSTR prefix = L"Local");
	~UserModeDebugOutput();

	bool Run(IDebugOutput* callback) override;
	bool Stop() override;
	bool IsRunning() const override;

private:
	DWORD DebugListen();

	wil::unique_handle _hDataReady, _hMemFile, _hBufferReady, _hThread, _hStop;
	PBYTE _buffer{ nullptr };
	CString _prefix;
	IDebugOutput* _sink;
};

