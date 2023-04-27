@echo off
if exist c:\msys64\usr\bin\sh.exe c:\msys64\usr\bin\sh.exe -c "%*" & goto end
if exist d:\a\_temp\msys64\usr\bin\sh.exe d:\a\_temp\msys64\usr\bin\sh.exe -c "%*" & goto end

:end
