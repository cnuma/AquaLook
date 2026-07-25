@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-all.ps1" %*
set EXIT_CODE=%ERRORLEVEL%
if not "%EXIT_CODE%"=="0" echo.
if not "%EXIT_CODE%"=="0" echo build-all a echoue avec le code %EXIT_CODE%.
exit /b %EXIT_CODE%
