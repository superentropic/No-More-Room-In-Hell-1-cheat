@echo off
setlocal
rem ---- locate Visual Studio (auto-detect; override with VS_DIR/MSVC_DIR if needed) ----
set "VS="
set "MSVC="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
)
if not defined VS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community" set "VS=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if not defined VS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional" set "VS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
if not defined VS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise" set "VS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
if not defined VS if defined VS_DIR set "VS=%VS_DIR%"
if not exist "%VS%\VC\Tools\MSVC" (
    echo ERROR: Visual Studio C++ tools not found. Install VS 2022+ with the Desktop C++ workload.
    exit /b 1
)
if defined MSVC_DIR (set "MSVC=%MSVC_DIR%") else (
    for /f "usebackq tokens=*" %%i in (`dir /b /ad /o-n "%VS%\VC\Tools\MSVC" 2^>nul`) do if not defined MSVC set "MSVC=%VS%\VC\Tools\MSVC\%%i"
)
if not exist "%MSVC%\bin\Hostx86\x86\cl.exe" (
    echo ERROR: x86 MSVC toolset not found under %MSVC%
    exit /b 1
)
set "SDK=%ProgramFiles(x86)%\Windows Kits\10"
if defined WINSDK_DIR set "SDK=%WINSDK_DIR%"
set "SDKVER="
if exist "%SDK%\Include" (
    rem Skip incomplete SDK folders (for example, a partially installed preview SDK).
    for /f "usebackq tokens=*" %%i in (`dir /b /ad /o-n "%SDK%\Include\10.*" 2^>nul`) do if not defined SDKVER if exist "%SDK%\Include\%%i\um\windows.h" set "SDKVER=%%i"
)
if not defined SDKVER (
    echo ERROR: Windows SDK not found.
    exit /b 1
)
set BIN=%MSVC%\bin\Hostx64\x64
set INC=%MSVC%\include
set LIB=%MSVC%\lib\x64
set SDKINC=%SDK%\Include\%SDKVER%
set ROOT=%~dp0

echo === Building dumper.dll ===
"%BIN%\cl.exe" /nologo /O2 /GS- /c /D WIN32 /D _WIN64 /D _WINDOWS /D _USRDLL /D NMRIH_DUMPER_EXPORTS /EHsc /I"%INC%" /I"%SDKINC%\ucrt" /I"%SDKINC%\um" /I"%SDKINC%\shared" /Fo"%ROOT%build\dumper.obj" "%ROOT%src\dumper\dumper.cpp"
if errorlevel 1 goto :err
"%BIN%\link.exe" /nologo /DLL /SUBSYSTEM:WINDOWS /MACHINE:X64 /OUT:"%ROOT%build\dumper.dll" "%ROOT%build\dumper.obj" kernel32.lib user32.lib /LIBPATH:"%LIB%" /LIBPATH:"%SDK%\Lib\%SDKVER%\um\x64" /LIBPATH:"%SDK%\Lib\%SDKVER%\ucrt\x64"
if errorlevel 1 goto :err

echo === Building loader.exe ===
"%BIN%\cl.exe" /nologo /O2 /GS- /c /D WIN32 /D _WIN64 /D _CONSOLE /EHsc /I"%INC%" /I"%SDKINC%\ucrt" /I"%SDKINC%\um" /I"%SDKINC%\shared" /Fo"%ROOT%build\loader.obj" "%ROOT%src\loader\loader.cpp"
if errorlevel 1 goto :err
"%BIN%\link.exe" /nologo /SUBSYSTEM:CONSOLE /MACHINE:X64 /OUT:"%ROOT%build\loader.exe" "%ROOT%build\loader.obj" kernel32.lib user32.lib advapi32.lib /LIBPATH:"%LIB%" /LIBPATH:"%SDK%\Lib\%SDKVER%\um\x64" /LIBPATH:"%SDK%\Lib\%SDKVER%\ucrt\x64"
if errorlevel 1 goto :err

echo === OK ===
exit /b 0
:err
echo BUILD FAILED
exit /b 1
