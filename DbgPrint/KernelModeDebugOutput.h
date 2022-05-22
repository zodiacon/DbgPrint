#pragma once

#include "DebugOutputBase.h"
#include <wil\resource.h>

class KernelModeDebugOutput : public DebugOutputBase {
public:
	// Inherited via DebugOutputBase
	bool Run(IDebugOutput* sink) override;
	bool Stop() override;
	bool IsRunning() const override;

protected:
	DWORD Process();

private:
	IDebugOutput* m_sink;
	TRACEHANDLE m_handle{ 0 };
	TRACEHANDLE m_hTrace{ 0 };
	EVENT_TRACE_PROPERTIES* m_properties;
	std::unique_ptr<BYTE[]> m_propertiesBuffer;
	EVENT_TRACE_LOGFILE m_traceLog = { 0 };
	wil::unique_handle m_hThread, m_hStop;
};

