@echo off
rem Wrapper for cf-support.ps1.
rem
rem Lets it run from cmd.exe without the default execution policy blocking the
rem unsigned script. Bypass applies to this process only.

setlocal
set "SCRIPT=%~dp0cf-support.ps1"

if not exist "%SCRIPT%" (
  echo Could not find "%SCRIPT%"
  exit /b 1
)

rem Accept the POSIX spellings; PowerShell only understands -Yes.
set "ARGS=%*"
if /i "%~1"=="--yes" set "ARGS=-Yes"
if /i "%~1"=="-y"    set "ARGS=-Yes"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %ARGS%
exit /b %ERRORLEVEL%
