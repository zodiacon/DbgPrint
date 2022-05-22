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

	wil::unique_handle m_hDataReady, m_hMemFile, m_hBufferReady, m_hThread, m_hStop;
	PBYTE m_buffer{ nullptr };
	CString m_prefix;
	IDebugOutput* m_sink;
};

