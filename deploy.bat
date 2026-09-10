@echo off
rem Deploy Qt runtime next to MeetingGrid.exe (uses windeployqt)
setlocal
set "VCVARS=D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "QTDIR=D:\Qt\6.10.3\msvc2022_64"
call "%VCVARS%" >nul
cd /d "%~dp0"
"%QTDIR%\bin\windeployqt.exe" --release --no-translations build\MeetingGrid.exe
echo.
echo Deploy done. Qt runtime DLLs copied into build\.
endlocal
