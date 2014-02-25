@echo off
if exist c:\mingw\msys\1.0\bin\sh.exe c:\mingw\msys\1.0\bin\sh.exe -c "%*" & goto end
if exist c:\msys\1.0\bin\sh.exe c:\msys\1.0\bin\sh.exe -c "%*" & goto end
if exist c:\msys\bin\sh.exe c:\msys\bin\sh.exe -c "%*" & goto end

:end
