@echo off
rem ============================================================================
rem Self-healing CMake reconfigure.
rem
rem Run this if do_build.bat fails after a Visual Studio toolset update -- the
rem symptom is a "CMAKE_CXX_COMPILER ... is not a full path to an existing
rem compiler tool" error (the cached cl.exe path points at a toolset VS removed).
rem
rem It first tries a normal reconfigure (cheap, reuses the cache).  If that fails
rem -- which is exactly what a stale cached compiler path does -- it clears the
rem CMake cache metadata (NOT the compiled objects) and reconfigures fresh with
rem the current toolset.  After it succeeds, run do_build.bat as normal.
rem
rem All output goes to configure_log.txt so nothing is lost when the window
rem closes -- paste that file if you need help.
rem ============================================================================
setlocal
set "REPO=C:\Users\jeffm\Documents\BaySickDAW"
set "BUILD=%REPO%\build"
set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
set "GEN=Visual Studio 18 2026"
set "LOG=%REPO%\configure_log.txt"

rem Reset PATH so vcvars64 can append without hitting the 8191-char limit.
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 > nul 2>&1

echo === Reconfigure attempt 1 (reuse existing cache) ===> "%LOG%"
"%CMAKE%" -S "%REPO%" -B "%BUILD%" -G "%GEN%" -A x64 >> "%LOG%" 2>&1
if errorlevel 1 goto fresh
echo CONFIGURE_EXIT_CODE=0 >> "%LOG%"
echo Success (reused cache).  Now run do_build.bat.  Details in configure_log.txt
goto end

:fresh
echo.>> "%LOG%"
echo === Attempt 1 failed (likely stale toolset); clearing CMake cache + retrying fresh ===>> "%LOG%"
del /q "%BUILD%\CMakeCache.txt" 2>nul
rmdir /s /q "%BUILD%\CMakeFiles" 2>nul
del /q "%BUILD%\juce_build\tools\CMakeCache.txt" 2>nul
rmdir /s /q "%BUILD%\juce_build\tools\CMakeFiles" 2>nul
echo === Reconfigure attempt 2 (fresh cache) ===>> "%LOG%"
"%CMAKE%" -S "%REPO%" -B "%BUILD%" -G "%GEN%" -A x64 >> "%LOG%" 2>&1
echo CONFIGURE_EXIT_CODE=%ERRORLEVEL% >> "%LOG%"
echo Retried with a fresh cache.  If CONFIGURE_EXIT_CODE=0 run do_build.bat.  Details in configure_log.txt

:end
endlocal
