# Virtual Dimension

A free, fast and feature-full virtual desktop manager for Windows, originally written by
Francois Ferrand (2003-2009, GPL v2), ported to 64-bit Windows 10/11.

Virtual Dimension shows a small preview window with your desktops and the icons of the
windows on each of them. You can switch desktops by clicking, with hotkeys or with the
mouse at the screen borders, drag window icons between desktops, and set per-window options
(always on top, transparency, minimize to tray, "on all desktops", remembered position...).

It is independent from the virtual desktops built into Windows (Win+Tab).

## Building

Requirements:

- Windows 10 or 11, x64
- Visual Studio 2022 or later (Community is fine) with the **Desktop development with C++**
  workload. CMake and Ninja are included with it.

### In VS Code

1. Install the recommended extensions (C/C++ and CMake Tools). VS Code suggests them when the folder is opened.
2. Select the `x64 Debug` (or `x64 Release`) configure preset in the CMake Tools status bar.
3. `Ctrl+Shift+B` builds. `F5` builds and starts the program under the debugger.

### From a command prompt

```bat
build.cmd            :: debug build   -> build\x64-debug\VirtualDimension.exe
build.cmd release    :: release build -> build\x64-release\VirtualDimension.exe
build.cmd debug clean
```

`build.cmd` locates Visual Studio with `vswhere` and loads its build environment, so it works
from any prompt. From a *Developer Command Prompt* you can also use CMake directly:
`cmake --preset x64-release && cmake --build --preset x64-release`.

The result is a single self-contained executable (static runtime, resources embedded).

## Usage

Start `VirtualDimension.exe`. Right-click the preview window or the tray icon for the menus,
and use *Configure* to add desktops, hotkeys, wallpapers, etc.

Command line (forwarded to the running instance):

| Option | Effect |
|---|---|
| `-s N` | switch to desktop number `N` (0-based) |
| `-d N -x program` | start `program` and put its windows on desktop `N` |

Settings are stored in `HKEY_CURRENT_USER\Software\Typz Software\Virtual Dimension`, the
same place as in the original 0.94 version.

## What changed compared to Virtual Dimension 0.94

Fixes for modern Windows:

- **No more ghost windows.** Windows 10/11 keep "closed" Store applications (Calculator,
  Settings...) and many shell windows *cloaked*: they exist and are "visible" for the
  window manager, but are not displayed. The old version managed them, and showed their
  empty frames again when switching desktops. Windows are now selected like the taskbar
  and Alt+Tab do (cloaking, tool/owned windows, shell windows, empty windows are ignored),
  and a window hidden or closed by its own application is no longer managed.
- Window tracking uses WinEvents (`SetWinEventHook`, out of context) and the public shell
  hook API, instead of an undocumented `shell32.dll` ordinal.
- Desktop switching never blocks on a hung application: unresponsive windows are changed
  asynchronously. The thread pool used for switching (and its 2-second stalls) is gone.
- Taskbar buttons are hidden with `ITaskbarList` (the old trick does not work with the
  Windows 11 taskbar). Wallpapers use `IDesktopWallpaper`; JPEG/PNG images are used directly.
- Multi-monitor and DPI aware (per-monitor v2): docking, auto-hide, maximize height/width,
  "move" hiding method, mouse warp and icon sizes follow the monitor of the window.
- If Virtual Dimension crashes or is killed, the windows it hid are restored (crash
  handler, and recovery of the hidden windows on the next start).
- 64-bit, Unicode (window titles in any language), visual styles, Segoe UI dialogs.
- Several 64-bit pointer truncation bugs fixed (menus, tray icons, balloon notifications).

Removed:

- *Shell integration*: the old version injected machine code into every application to add
  a "Virtual Dimension" entry to their system menus. This cannot work with 64-bit, Store and
  protected applications, and is treated as malicious behavior by security software. All
  those actions remain available from the preview window and tray menus. "Minimize to
  tray" still works: it now reacts to the window being minimized.
- Languages other than English (the resources are embedded in the executable).
- The Windows 9x code paths, the MinGW makefile and the NSIS installer script.

## Known limitations

- Windows of programs running **as administrator** cannot be managed unless Virtual
  Dimension also runs as administrator (Windows prevents it: UIPI). They stay visible on
  all desktops.
- Windows placed on another *Windows* virtual desktop (Win+Tab) are ignored.
- Programs started with `-d N -x program` are matched by process, or by executable name
  for packaged applications (eg, Notepad on Windows 11), during 15 seconds.

## Debugging

Debug builds write a trace of what Virtual Dimension does with the windows to
`%TEMP%\VirtualDimension-debug.log` (and to the debugger output).

## History

The original CVS history (2003-2009) has been converted to git and is part of this
repository; the release tags `RELEASE_0_94`, `RELEASE_0_94_BETA1` and
`BEFORE_RMA_MODS_MERGE` are preserved.

## License

GNU General Public License, version 2 or later. See [LICENSE](LICENSE).

- Original program © 2003–2008 Francois Ferrand.
- 64-bit Windows 10/11 port © 2026 Daniel Filkovic ([@DanielCro](https://github.com/DanielCro)).

This port was made with heavy use of an AI coding agent (Anthropic's Claude), which did the
investigation, the porting and the fixes under the maintainer's direction and review.
