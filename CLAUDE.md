# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

DbgPrint is a native Win32 GUI (C++20, ATL/WTL) that shows debug output live: user-mode `OutputDebugString` (which includes .NET `Debug.Write*` / `Trace.Write*`) and kernel-mode `DbgPrint`/`DbgPrintEx`, with no driver needed. The *Kernel / Enable All Components* menu shows output from every kernel component without editing the Registry or rebooting.

## Build

- `WTLHelper` is a git submodule (https://github.com/zodiacon/WTLHelper). It holds the WTL headers (`WTLHelper/WTL`) and the `WTLHelper` static library. Run `git submodule update --init` before you build.
- Solution `DbgPrint.sln` builds two projects: `DbgPrint` and `WTLHelper\WTLHelper\WTLHelper.vcxproj` (DbgPrint references it as a project).
- Toolset is `v145` (Visual Studio 2026). Configurations are `Debug`, `Release` and `ReleaseSigned`; platforms are `x64`, `x86` (Win32) and `ARM64`.
- Build from a Developer command prompt:
  ```
  msbuild DbgPrint.sln /p:Configuration=Debug /p:Platform=x64 /m
  ```
- The repo has no tests and no lint setup.

## Architecture

**Capture sources** (`DebugOutputBase.h`): each source implements `DebugOutputBase` (`Run(IDebugOutput* sink)` / `Stop()` / `IsRunning()`). It runs its own worker thread and calls `IDebugOutput::DebugOutput(pid, text, time, flags)` for each message.
- `UserModeDebugOutput` uses the classic DBWIN protocol: the `DBWIN_BUFFER_READY` / `DBWIN_DATA_READY` events plus the `DBWIN_BUFFER` 4KB shared section. The constructor takes a namespace prefix (`Local` or `Global`), and `Global` captures Session 0 (services). That needs admin rights.
- `KernelModeDebugOutput` starts a real-time ETW kernel-logger session with `EVENT_TRACE_FLAG_DBGPRINT` (a system-logger-mode session on Win8+). It needs admin rights.

**View** (`DebugView.*`): `CDebugView` implements `IDebugOutput` and is a virtual (owner-data) list view built on WTLHelper's `CVirtualListView`. Worker threads add items to `m_TempItems` under `m_Lock`. A 1-second UI timer then moves them into `m_Items`, a filterable collection that `SetFilter` uses. Any code that touches the item collections must take `m_Lock`. A view is either a real-time log or a file log (`ViewType`).

**Frame** (`MainFrm.*`): `CMainFrame` hosts the views as tabs (`CNativeCustomTabView`) and owns the quick-filter edit in the rebar. Its message map sends `WM_COMMAND` to the active `CDebugView` first (`ProcessWindowMessage` with map ID 1), so per-view commands go in the view's `ALT_MSG_MAP(1)` section. Views reach back to the frame through `IMainFrame` (`Interfaces.h`). Menu items that need elevation are enabled or disabled based on `SecurityHelper::IsRunningElevated()`, and *Run as Administrator* relaunches the app.

**Other pieces**
- `ProcessManager` / `ImageIconCache` resolve PID → process name and icon. `ProcessKey` (PID + start time) is the identity, so a reused PID is not mistaken for the old process.
- `DebugLogPersist` saves and loads logs as CSV or a native format.
- `AppSettings` uses WTLHelper's `Settings` macros (`BEGIN_SETTINGS`/`SETTING`) and persists to `HKCU\Software\ScorpioSoftware\DbgPrint`.
- Dark mode, owner-drawn menus, custom controls, the INI and registry helpers and `SecurityHelper` all come from WTLHelper. Look there before adding a new UI or helper utility.

## Conventions

- Tabs for indentation. Member variables use `m_PascalCase`. WTL message maps use handler signatures like `LRESULT OnX(WORD, WORD, HWND, BOOL&)`.
- Every `.cpp` includes `pch.h` first.
- Command IDs and resources live in `DbgPrint.rc` / `resource.h`. Edit them together.
