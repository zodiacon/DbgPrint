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
	IDebugOutput* _sink;
	TRACEHANDLE _handle{ 0 };
	TRACEHANDLE _hTrace{ 0 };
	EVENT_TRACE_PROPERTIES* _properties;
	std::unique_ptr<BYTE[]> _propertiesBuffer;
	EVENT_TRACE_LOGFILE _traceLog = { 0 };
	wil::unique_handle _hThread, _hStop;
};

