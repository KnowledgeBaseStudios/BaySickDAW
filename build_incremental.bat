@echo off
setlocal enabledelayedexpansion

echo === Vibesynth Incremental Build ===

:: Find vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

:: Get VS install path
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSPATH=%%i"
echo VS: %VSPATH%

:: Source vcvars64
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
if errorlevel 1 (echo ERROR: vcvars64 failed & exit /b 1)
echo MSVC environment loaded.

:: Find ninja
for /r "%VSPATH%" %%f in (ninja.exe) do set "NINJA=%%f"
echo Ninja: %NINJA%
set "PATH=%~dp0\build_ninja;%PATH%"
for %%f in ("%NINJA%") do set "PATH=%%~dpf;%PATH%"

:: Configure cmake if build.ninja doesn't exist
if not exist "build\build.ninja" (
    echo Configuring CMake...
    cmake -S "%~dp0" -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=%NINJA%"
    if errorlevel 1 (echo ERROR: CMake configure failed & exit /b 1)
)

:: Build
echo Building...
cmake --build "%~dp0build" -j8
if errorlevel 1 (echo ERROR: Build failed & exit /b 1)

echo.
echo === Build Complete ===
echo Executable: %~dp0build\VibesynthStandalone_artefacts\Release\Vibesynth.exe
