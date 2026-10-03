# KeyViz

A live keyboard visualiser for Windows. It shows a full-size keyboard and highlights every key currently held down, even while another window has focus.

Keys are tracked by scan code, so the layout stays correct regardless of the active keyboard language. Released keys fade out briefly so quick taps remain visible, and an "Always on top" toggle is available in the window's system menu.

## Build

Run `build.bat` (uses MSVC if available, otherwise MinGW gcc) to produce `KeyViz.exe`.

If it reports `No C compiler found`, install one of:

- **MinGW-w64**: `winget install BrechtSanders.WinLibs.POSIX.UCRT`, then open a new terminal so `gcc` is on the PATH.
- **MSVC**: install the Visual Studio Build Tools and run `build.bat` from a "Developer Command Prompt for VS".
