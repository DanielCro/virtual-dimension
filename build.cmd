@echo off
rem Build Virtual Dimension from a plain command prompt.
rem Usage: build.cmd [debug^|release] [clean]
setlocal

set "PRESET=x64-debug"
if /i "%~1"=="release" set "PRESET=x64-release"

rem Load the Visual Studio x64 build environment, unless already loaded.
if defined VSCMD_VER goto :build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo error: Visual Studio with the "Desktop development with C++" workload was not found.
    exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

:build
cd /d "%~dp0"
if /i "%~2"=="clean" if exist "build\%PRESET%" rmdir /s /q "build\%PRESET%"

cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
echo.
echo Built: build\%PRESET%\VirtualDimension.exe
