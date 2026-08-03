@echo off
rem Installs the Leclaude overlay handler.
rem A double click starts the installation. Windows asks for permission.

net session >nul 2>&1
if not %errorlevel% == 0 (
    rem The process has no administrator rights. Windows starts it again and asks for them.
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
echo.
pause
