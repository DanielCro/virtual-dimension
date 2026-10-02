cmake_minimum_required(VERSION 3.25)

project(VirtualDimension
    VERSION 0.95.0
    DESCRIPTION "A free, fast, and feature-full virtual desktop manager for Windows"
    LANGUAGES CXX RC)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Virtual Dimension only supports 64-bit builds (use an x64 toolchain).")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "Virtual Dimension is built with the MSVC toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static CRT: a single, self-contained executable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(VD_SOURCES
    src/AlwaysOnTop.cpp
    src/ApplicationListDlg.cpp
    src/BackgroundColor.cpp
    src/BackgroundDisplayMode.cpp
    src/BalloonNotif.cpp
    src/CmdLine.cpp
    src/CmdLineOptions.cpp
    src/Config.cpp
    src/ConfigBox.cpp
    src/Desktop.cpp
    src/DesktopManager.cpp
    src/deskPropsDlg.cpp
    src/ExplorerWrapper.cpp
    src/FastWindow.cpp
    src/HidingMethod.cpp
    src/HotKeyControl.cpp
    src/HotkeyConfig.cpp
    src/HotKeyManager.cpp
    src/LinkControl.cpp
    src/Locale.cpp
    src/MouseWarp.cpp
    src/movewindow.cpp
    src/OnScreenDisplay.cpp
    src/PlatformHelper.cpp
    src/Settings.cpp
    src/ShellHook.cpp
    src/stdafx.cpp
    src/ToolTip.cpp
    src/Transparency.cpp
    src/TrayIcon.cpp
    src/TrayIconsManager.cpp
    src/VirtualDimension.cpp
    src/WallPaper.cpp
    src/Window.cpp
    src/WindowDialogs.cpp
    src/WindowsList.cpp
    src/WindowsManager.cpp
)

add_executable(VirtualDimension WIN32 ${VD_SOURCES} res/VirtualDimension.rc)

target_include_directories(VirtualDimension PRIVATE src)
target_precompile_headers(VirtualDimension PRIVATE src/stdafx.h)

target_compile_definitions(VirtualDimension PRIVATE
    UNICODE _UNICODE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00 WINVER=0x0A00 NTDDI_VERSION=0x0A00000C
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:DEBUG>
)

target_compile_options(VirtualDimension PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W3 /permissive- /utf-8 /Zc:__cplusplus /MP>
)

# The application manifest is embedded by the resource script.
target_link_options(VirtualDimension PRIVATE /MANIFEST:NO)

target_link_libraries(VirtualDimension PRIVATE
    comctl32 comdlg32 shell32 shlwapi ole32 oleaut32 uuid
    dwmapi msimg32 windowscodecs version advapi32 gdi32 user32
)

# The resource compiler needs the resource ids and the icon/manifest files.
set_property(SOURCE res/VirtualDimension.rc APPEND PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/res)
set_property(SOURCE res/VirtualDimension.rc APPEND PROPERTY COMPILE_DEFINITIONS
    VD_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VD_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VD_VERSION_PATCH=${PROJECT_VERSION_PATCH})

install(TARGETS VirtualDimension RUNTIME DESTINATION .)
install(FILES LICENSE README.md DESTINATION .)
