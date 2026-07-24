@echo off
setlocal

if "%~1"=="-?" goto :help
if "%~1"=="/?" goto :help
if /I "%~1"=="-h" goto :help
if /I "%~1"=="--help" goto :help

set "SCRIPT_DIR=%~dp0"
where pwsh.exe >nul 2>nul
if %ERRORLEVEL%==0 (
    set "POWERSHELL_EXE=pwsh.exe"
) else (
    set "POWERSHELL_EXE=powershell.exe"
)

"%POWERSHELL_EXE%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build_windows.ps1" %*
exit /b %ERRORLEVEL%

:help
echo Usage: app\build_windows.cmd [build_windows.ps1 options]
echo Example: app\build_windows.cmd -Configuration Release -Variants Standard -BuildTests -EnableLiveLlmTests
echo This wrapper runs app\build_windows.ps1 through PowerShell for cmd.exe users.
exit /b 0
