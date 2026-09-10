@echo off
rem ============================================================
rem  MeetingGrid build script (Windows + MSVC + Qt 6.10.3)
rem  Usage:  build.bat             Release build
rem          build.bat Debug       Debug build
rem ============================================================

setlocal

set "VSDIR=D:\Program Files\Microsoft Visual Studio\18\Community"
set "VCVARS=%VSDIR%\VC\Auxiliary\Build\vcvars64.bat"
set "QTDIR=D:\Qt\6.10.3\msvc2022_64"
set "NINJA=D:\Qt\Tools\Ninja\ninja.exe"

if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found: %VCVARS%
    exit /b 1
)
if not exist "%QTDIR%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [ERROR] Qt6 CMake config not found: %QTDIR%
    exit /b 1
)

call "%VCVARS%" >nul

set "BUILD_TYPE=Release"
if /I "%~1"=="Debug" set "BUILD_TYPE=Debug"

cd /d "%~dp0"

echo [1/2] CMake configure ...
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH=%QTDIR% -DCMAKE_MAKE_PROGRAM=%NINJA%
if errorlevel 1 exit /b 1

echo [2/2] Build ...
cmake --build build
if errorlevel 1 exit /b 1

echo.
echo Build OK: %~dp0build\MeetingGrid.exe
endlocal
