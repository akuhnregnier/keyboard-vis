@echo off
rem Build KeyViz.exe from keyviz.c. Uses MSVC if available, otherwise MinGW gcc.
where cl >nul 2>nul && (
    cl /nologo /O2 keyviz.c user32.lib gdi32.lib /Fe:KeyViz.exe /link /SUBSYSTEM:WINDOWS
    goto :eof
)
where gcc >nul 2>nul && (
    gcc keyviz.c -o KeyViz.exe -O2 -s -mwindows -luser32 -lgdi32
    goto :eof
)
echo No C compiler found. Run this from a "Developer Command Prompt for VS", or install MinGW-w64.
