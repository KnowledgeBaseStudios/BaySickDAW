@echo off
setlocal EnableExtensions

rem ============================================================================
rem BaySickDAW build script.
rem
rem PORTABLE (2026-08-09): every path is derived at run time -- the repo root
rem from this file's own location, Visual Studio from vswhere, CMake from PATH
rem with fallbacks -- so a checkout on someone else's machine, at any path,
rem including one with spaces, builds without editing this file.
rem
rem THE BUILD GATE CONTRACT (do not change): build_log.txt at the repo root must
rem contain, spelled exactly, RELEASE_EXIT_CODE, DEBUG_EXIT_CODE,
rem HELPER64_EXIT_CODE, HELPER32_CONFIG_EXIT_CODE and HELPER32_EXIT_CODE, and
rem all five targets must build.  Everything downstream greps those strings.
rem The other half of that contract: no run may leave a log that could be read
rem as a pass when it was not one.  The log is emptied at the top of this file,
rem before any exit is possible, and every early exit stamps all five strings
rem non-zero on its way out.
rem
rem NO "call :label" ANYWHERE IN THIS FILE, DELIBERATELY.  .gitattributes sets
rem "* text=auto", so a clone made with core.autocrlf off lands this script on
rem disk with LF-only line endings, and cmd.exe cannot resolve a CALL target in
rem an LF-only batch file ("The system cannot find the batch label specified").
rem GOTO survives that; CALL does not.  Subroutine-shaped work therefore uses a
rem return-label variable and GOTO.
rem ============================================================================

rem %~dp0 always ends in a backslash; strip it so "%REPO%\x" never doubles up.
set "REPO=%~dp0"
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"

set "LOG=%REPO%\build_log.txt"

rem ----------------------------------------------------------------------------
rem BUILD GATE INTEGRITY: the log is created and emptied HERE, before anything
rem else can exit.  Every toolchain check below quits before a compiler ever
rem runs, and a build_log.txt left over from an earlier run still carries the
rem five zero exit codes that every downstream gate greps for, so a machine
rem missing the toolchain would report a passing build.  The log on disk must
rem always describe THIS run.
rem
rem The delete-then-test is part of that: if another process holds the file open
rem the delete fails, and a log that cannot be replaced is the same lie.
rem ----------------------------------------------------------------------------
del /f /q "%LOG%" >nul 2>&1
if exist "%LOG%" goto err_log_locked
echo === BaySickDAW build started %DATE% %TIME% ===                      > "%LOG%"
echo Repo root: %REPO%                                                  >> "%LOG%"
echo Looking for Visual Studio and CMake...                             >> "%LOG%"
if not exist "%LOG%" goto err_log_locked

set "BUILD=%REPO%\build"
set "BUILD32=%REPO%\build32"
set "HELPER_SRC=%REPO%\Source\Hosting\Helper"
set "SYMSTORE=%REPO%\SymbolStore"
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

set "PF=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"
if not defined PF86 set "PF86=%PF%"

rem ----------------------------------------------------------------------------
rem Locate CMake FIRST, while the caller's PATH is still intact.  Further down
rem PATH gets reset to a bare minimum (see the vcvars comment), which would
rem otherwise hide a CMake the user installed somewhere non-standard.
rem ----------------------------------------------------------------------------
set "CMAKE="
for %%I in (cmake.exe) do if not defined CMAKE set "CMAKE=%%~$PATH:I"

rem ----------------------------------------------------------------------------
rem Locate Visual Studio.  vswhere.exe ships with the VS installer on every
rem machine carrying VS 2017 or newer, and finds ANY edition and version --
rem Community, Professional, Enterprise, Build Tools -- so nothing here is
rem pinned to one install.  VSMAJOR drives the CMake generator name below.
rem ----------------------------------------------------------------------------
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%PF%\Microsoft Visual Studio\Installer\vswhere.exe"

set "VSPATH="
set "VSMAJOR="
set "ANYVS="
if not exist "%VSWHERE%" goto vs_scan

set "VSWARGS=-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64"

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -prerelease -products * -property installationPath 2^>nul`) do set "ANYVS=%%I"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" %VSWARGS% -property installationPath 2^>nul`) do set "VSPATH=%%I"
if defined VSPATH goto vs_version

rem Nothing stable carries the C++ tools; accept a Preview install rather than
rem stopping a tester who only has one.
set "VSWARGS=-latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" %VSWARGS% -property installationPath 2^>nul`) do set "VSPATH=%%I"
if not defined VSPATH goto vs_scan

:vs_version
for /f "usebackq tokens=1 delims=." %%I in (`"%VSWHERE%" %VSWARGS% -property installationVersion 2^>nul`) do set "VSMAJOR=%%I"
goto vs_done

rem ----------------------------------------------------------------------------
rem Fallback for a machine where vswhere is missing: walk the known install
rem layouts.  Microsoft used year-named folders through 2022 and switched to
rem version-numbered folders at 18, so both shapes are covered.  Lowest version
rem is probed first so the highest one found wins by overwriting.
rem ----------------------------------------------------------------------------
:vs_scan
for %%E in (BuildTools Community Professional Enterprise Preview) do if exist "%PF86%\Microsoft Visual Studio\2017\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%PF86%\Microsoft Visual Studio\2017\%%E"
for %%E in (BuildTools Community Professional Enterprise Preview) do if exist "%PF86%\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%PF86%\Microsoft Visual Studio\2019\%%E"
for %%E in (BuildTools Community Professional Enterprise Preview) do if exist "%PF%\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%PF%\Microsoft Visual Studio\2019\%%E"
for %%E in (BuildTools Community Professional Enterprise Preview) do if exist "%PF%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%PF%\Microsoft Visual Studio\2022\%%E"
for %%E in (BuildTools Community Professional Enterprise Preview) do if exist "%PF%\Microsoft Visual Studio\18\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%PF%\Microsoft Visual Studio\18\%%E"
for %%E in (BuildTools Community Professional Enterprise Preview) do if exist "%PF%\Microsoft Visual Studio\19\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%PF%\Microsoft Visual Studio\19\%%E"
if not defined VSPATH goto vs_done

rem Recover the major version from the install folder that matched.
if not defined VSMAJOR if not "%VSPATH:\2017\=%"=="%VSPATH%" set "VSMAJOR=15"
if not defined VSMAJOR if not "%VSPATH:\2019\=%"=="%VSPATH%" set "VSMAJOR=16"
if not defined VSMAJOR if not "%VSPATH:\2022\=%"=="%VSPATH%" set "VSMAJOR=17"
if not defined VSMAJOR if not "%VSPATH:\18\=%"=="%VSPATH%" set "VSMAJOR=18"
if not defined VSMAJOR if not "%VSPATH:\19\=%"=="%VSPATH%" set "VSMAJOR=19"

:vs_done
if defined VSPATH goto vs_ok
rem Visual Studio present but without the C++ tools is a different fix, so it
rem gets a different message.
if defined ANYVS goto err_no_workload
goto err_no_vs

:vs_ok
set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" goto err_no_cpp

rem CMake generator name per VS major version.  An unknown (future) major leaves
rem GEN empty, which makes a fresh configure fall through to CMake's own default
rem generator rather than failing on a name this script cannot know yet.
set "GEN="
if "%VSMAJOR%"=="15" set "GEN=Visual Studio 15 2017"
if "%VSMAJOR%"=="16" set "GEN=Visual Studio 16 2019"
if "%VSMAJOR%"=="17" set "GEN=Visual Studio 17 2022"
if "%VSMAJOR%"=="18" set "GEN=Visual Studio 18 2026"

rem Recent Visual Studio ships its own CMake with the C++ workload; use it when
rem the user has no standalone CMake installed.
if not defined CMAKE if exist "%PF%\CMake\bin\cmake.exe" set "CMAKE=%PF%\CMake\bin\cmake.exe"
if not defined CMAKE if exist "%PF86%\CMake\bin\cmake.exe" set "CMAKE=%PF86%\CMake\bin\cmake.exe"
if not defined CMAKE if exist "%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined CMAKE goto err_no_cmake

echo.
echo Building BaySickDAW.  The first run takes a long time - go make a coffee.
echo   Repo root      : %REPO%
echo   Visual Studio  : %VSPATH%
echo   CMake          : %CMAKE%
echo   Full log       : %LOG%
echo.

rem ----------------------------------------------------------------------------
rem Reset PATH to the bare minimum so vcvars64.bat can append the compiler paths
rem without blowing past the 8191-character command-line limit.
rem ----------------------------------------------------------------------------
set "PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem;%SystemRoot%\System32\WindowsPowerShell\v1.0"
call "%VCVARS%" > nul 2>&1
if not defined VCINSTALLDIR goto err_vcvars

rem Appends: the log was already created at the top of this script so that the
rem toolchain failures above cannot leave a stale one behind.
echo === Toolchain found, starting the build %DATE% %TIME% ===          >> "%LOG%"
echo Visual Studio: %VSPATH%                                            >> "%LOG%"
echo Visual Studio major version: %VSMAJOR%                             >> "%LOG%"
echo CMake: %CMAKE%                                                     >> "%LOG%"
echo Generator for fresh trees: %GEN%                                   >> "%LOG%"

rem ----------------------------------------------------------------------------
rem QA-0a (2026-05-07): build BOTH configs.  Release is the shipping exe and
rem the one used for music production.  Debug ships internal jassert checks
rem -- a failing condition pops a dialog with file:line so regressions are
rem visible to a non-coder solo developer.  Standing workflow: verify Claude
rem fixes in Debug FIRST, then re-run in Release for the actual user test.
rem ----------------------------------------------------------------------------

rem ----------------------------------------------------------------------------
rem Smoke round 3 (crash hunt, 2026-07-24): archive the OUTGOING Release
rem exe+pdb pair BEFORE the build overwrites them, so a WER crash dump from
rem any earlier build can always be symbolized against its matching pair
rem (matched by the PE TimeDateStamp inside the exe, not the folder name).
rem Keeps the 5 newest pairs (~850 MB); prunes older automatically.
rem
rem Opt-in by folder: it only runs when a SymbolStore folder already exists at
rem the repo root, so a tester's checkout never silently grows by a gigabyte.
rem Wrapped in try/catch and skipped when PowerShell is absent -- a local
rem debugging convenience must never be able to fail the build.
rem ----------------------------------------------------------------------------
if not exist "%SYMSTORE%" goto skip_symbols
if not exist "%PS%" goto skip_symbols
"%PS%" -NoProfile -ExecutionPolicy Bypass -Command "try { $r='%BUILD%\BaySickDAWStandalone_artefacts\Release'; $s='%SYMSTORE%'; if (Test-Path (Join-Path $r 'BaySickDAW.exe')) { $t=(Get-Item (Join-Path $r 'BaySickDAW.exe')).LastWriteTime.ToString('yyyyMMdd-HHmmss'); $d=Join-Path $s $t; if (!(Test-Path $d)) { New-Item -ItemType Directory -Force $d | Out-Null; Copy-Item (Join-Path $r 'BaySickDAW.exe') $d; Copy-Item (Join-Path $r 'BaySickDAW.pdb') $d -ErrorAction SilentlyContinue }; Get-ChildItem $s -Directory | Sort-Object Name -Descending | Select-Object -Skip 5 | Remove-Item -Recurse -Force } } catch { }" > nul 2>&1
:skip_symbols

rem ----------------------------------------------------------------------------
rem A fresh checkout has no build tree at all.  Configure it here rather than
rem letting "cmake --build" fail with a message a non-coder cannot act on.
rem ----------------------------------------------------------------------------
if exist "%BUILD%\CMakeCache.txt" goto build_release
echo First build detected - setting the project up.  This takes a few minutes.
echo === Configuring main project ===                                   >> "%LOG%"
set "CFG_B=%BUILD%"
set "CFG_S=%REPO%"
set "CFG_P=x64"
set "CFG_RET=cfg_back_main"
goto do_configure
:cfg_back_main
if not "%CFG_RC%"=="0" goto err_configure

:build_release
echo Building Release...
echo === Building Release ===                                           >> "%LOG%"
"%CMAKE%" --build "%BUILD%" --target BaySickDAWStandalone --config Release -j4 >> "%LOG%" 2>&1
set "RC_REL=%ERRORLEVEL%"
echo RELEASE_EXIT_CODE=%RC_REL%                                         >> "%LOG%"

echo Building Debug...
echo === Building Debug ===                                             >> "%LOG%"
"%CMAKE%" --build "%BUILD%" --target BaySickDAWStandalone --config Debug   -j4 >> "%LOG%" 2>&1
set "RC_DBG=%ERRORLEVEL%"
echo DEBUG_EXIT_CODE=%RC_DBG%                                           >> "%LOG%"

rem ----------------------------------------------------------------------------
rem QA-ModelShell TS6 (BLU-302, 2026-07-29): the plugin-sandbox helper.  A
rem SEPARATE target, so the two lines above would never have built it -- and a
rem broken helper would have gone unnoticed until a bridged plugin failed to
rem start at runtime.  Release only: the helper is a plain IPC + hosting shim
rem with no jasserts of ours to catch, and the app locates it by filename
rem regardless of which config launched it.
rem ----------------------------------------------------------------------------
echo Building plugin host helper, 64-bit...
echo === Building Plugin Host helper (x64) ===                          >> "%LOG%"
"%CMAKE%" --build "%BUILD%" --target BaySickPluginHost --config Release -j4 >> "%LOG%" 2>&1
set "RC_H64=%ERRORLEVEL%"
echo HELPER64_EXIT_CODE=%RC_H64%                                        >> "%LOG%"

rem ----------------------------------------------------------------------------
rem QA-ModelShell TS6 (BLU-302 step 7b): the 32-bit helper.  A 64-bit process
rem cannot load a 32-bit VST3 at all, so this binary IS our 32-bit support.
rem
rem It needs its OWN build tree because an MSVC generator is single-platform per
rem tree -- hence the separate configure below against the standalone helper
rem project at Source\Hosting\Helper, which depends on vendored JUCE and nothing
rem else (configuring the ROOT project as Win32 would drag in x86 builds of
rem sfizz / NAM / RubberBand / LAME / WORLD / lunasvg).
rem Configure is idempotent; CMake no-ops when the cache is already good.
rem ----------------------------------------------------------------------------
echo Configuring plugin host helper, 32-bit...
echo === Configuring Plugin Host helper (x86) ===                       >> "%LOG%"
set "CFG_B=%BUILD32%"
set "CFG_S=%HELPER_SRC%"
set "CFG_P=Win32"
set "CFG_RET=cfg_back_helper32"
goto do_configure
:cfg_back_helper32
echo HELPER32_CONFIG_EXIT_CODE=%CFG_RC%                                 >> "%LOG%"
set "RC_H32C=%CFG_RC%"

echo Building plugin host helper, 32-bit...
echo === Building Plugin Host helper (x86) ===                          >> "%LOG%"
"%CMAKE%" --build "%BUILD32%" --target BaySickPluginHost --config Release -j4 >> "%LOG%" 2>&1
set "RC_H32=%ERRORLEVEL%"
echo HELPER32_EXIT_CODE=%RC_H32%                                        >> "%LOG%"

rem Stage the x86 helper beside the app exes (its own build tree cannot see
rem BaySickDAWStandalone's output dir, so the copy happens here rather than in
rem a POST_BUILD command).
copy /Y "%BUILD32%\BaySickPluginHost_artefacts\Release\BaySickPluginHost32.exe" "%BUILD%\BaySickDAWStandalone_artefacts\Release\" >> "%LOG%" 2>&1
copy /Y "%BUILD32%\BaySickPluginHost_artefacts\Release\BaySickPluginHost32.exe" "%BUILD%\BaySickDAWStandalone_artefacts\Debug\"   >> "%LOG%" 2>&1

rem ----------------------------------------------------------------------------
rem The five exit codes can all be zero while the result is still unusable: the
rem two copies above report failure only through a message with no exit code the
rem gate reads, and a locked destination is the normal way they fail.  The app
rem loads the sandbox helpers by filename from its own folder, so a helper that
rem never landed is a runtime failure.  Check for the real files.
rem ----------------------------------------------------------------------------
set "RC_ART=0"
if not exist "%BUILD%\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe" set "RC_ART=1"
if not exist "%BUILD%\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe" set "RC_ART=1"
if not exist "%BUILD%\BaySickDAWStandalone_artefacts\Release\BaySickPluginHost64.exe" set "RC_ART=1"
if not exist "%BUILD%\BaySickDAWStandalone_artefacts\Debug\BaySickPluginHost64.exe" set "RC_ART=1"
if not exist "%BUILD%\BaySickDAWStandalone_artefacts\Release\BaySickPluginHost32.exe" set "RC_ART=1"
if not exist "%BUILD%\BaySickDAWStandalone_artefacts\Debug\BaySickPluginHost32.exe" set "RC_ART=1"
echo ARTEFACTS_EXIT_CODE=%RC_ART%                                       >> "%LOG%"

set "FAILED=0"
if not "%RC_REL%"=="0" set "FAILED=1"
if not "%RC_DBG%"=="0" set "FAILED=1"
if not "%RC_H64%"=="0" set "FAILED=1"
if not "%RC_H32C%"=="0" set "FAILED=1"
if not "%RC_H32%"=="0" set "FAILED=1"
if not "%RC_ART%"=="0" set "FAILED=1"

echo.
echo ==================== BUILD SUMMARY ====================
echo   Release app                : exit %RC_REL%
echo   Debug app                  : exit %RC_DBG%
echo   Plugin host 64-bit         : exit %RC_H64%
echo   Plugin host 32-bit setup   : exit %RC_H32C%
echo   Plugin host 32-bit build   : exit %RC_H32%
echo   0 means that step worked.
echo.
if not "%RC_ART%"=="0" echo   A file that should have been built or copied is not there.
if not "%RC_ART%"=="0" echo   If every step above says 0, close BaySickDAW and any plugin it
if not "%RC_ART%"=="0" echo   was running, then build again - a locked file blocks the copy.
if "%FAILED%"=="0" echo   ALL FIVE STEPS SUCCEEDED.
if "%FAILED%"=="0" echo   The app is at: %BUILD%\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe
if not "%FAILED%"=="0" echo   SOMETHING FAILED.  Open build_log.txt and search for the word "error".
if not "%FAILED%"=="0" echo   Log file: %LOG%
echo =======================================================
echo.

rem One unambiguous verdict at the end of the log, so a reader who does not know
rem which strings to grep still gets the right answer from the last line.
if "%FAILED%"=="0" echo BUILD SUCCEEDED                                 >> "%LOG%"
if not "%FAILED%"=="0" echo BUILD FAILED                                >> "%LOG%"

if /i "%~1"=="pause" pause
exit /b %FAILED%

rem ============================================================================
rem GOTO-style subroutine: configure one CMake build tree.
rem   in  CFG_B build dir, CFG_S source dir, CFG_P platform, CFG_RET return label
rem   out CFG_RC 0 on success
rem
rem An existing cache already pins its generator and platform, so reconfiguring
rem such a tree passes NEITHER -G nor -A: re-stating them hard-errors the moment
rem the machine's newest Visual Studio stops matching the one that first created
rem the tree.
rem
rem Every result here is tested as a string against "0", never with the
rem "if errorlevel 1" form: that form means "is the code 1 OR MORE", so a CMake
rem that dies on an access violation and returns a NEGATIVE code passes it, and
rem a configure that never finished would be recorded as a success.
rem ============================================================================
:do_configure
if exist "%CFG_B%\CMakeCache.txt" goto cfg_reuse
if not defined GEN goto cfg_default_gen
"%CMAKE%" -S "%CFG_S%" -B "%CFG_B%" -G "%GEN%" -A %CFG_P% >> "%LOG%" 2>&1
if "%ERRORLEVEL%"=="0" goto cfg_ok
echo   Setup did not work with the "%GEN%" setting.
echo   Trying again and letting CMake choose for itself.
:cfg_default_gen
"%CMAKE%" -S "%CFG_S%" -B "%CFG_B%" -A %CFG_P% >> "%LOG%" 2>&1
if not "%ERRORLEVEL%"=="0" goto cfg_fail
goto cfg_ok
:cfg_reuse
"%CMAKE%" -S "%CFG_S%" -B "%CFG_B%" >> "%LOG%" 2>&1
if not "%ERRORLEVEL%"=="0" goto cfg_fail
:cfg_ok
set "CFG_RC=0"
goto %CFG_RET%
:cfg_fail
set "CFG_RC=1"
goto %CFG_RET%

rem ============================================================================
rem Failure exits.  Every one prints what is missing and what to install in
rem plain English, because the people hitting them do not read code.
rem ============================================================================

:err_no_vs
echo.
echo   BUILD CANNOT START - Visual Studio was not found on this PC.
echo.
echo   BaySickDAW needs Microsoft's free C++ compiler in order to build.
echo   1. Go to https://visualstudio.microsoft.com/downloads/
echo   2. Download "Visual Studio Community" and run the installer.
echo   3. On the "Workloads" screen, tick "Desktop development with C++".
echo   4. Finish the install, then run this script again.
echo.
echo   See BUILDING.md next to this script for the full walkthrough.
echo.
echo REASON: Visual Studio was not found on this PC.                    >> "%LOG%"
goto fail_exit

:err_no_workload
echo.
echo   BUILD CANNOT START - Visual Studio is installed, but the C++ part is not.
echo.
echo   Found Visual Studio here: %ANYVS%
echo   That install has no C++ compiler, so nothing here can build BaySickDAW.
echo.
echo   Fix it: open "Visual Studio Installer" from the Start menu, click
echo   "Modify" on your Visual Studio, tick the workload called
echo   "Desktop development with C++", then click Modify to install it.
echo   It is a large download.  Run this script again when it finishes.
echo.
echo   See BUILDING.md next to this script for the full walkthrough.
echo.
echo REASON: Visual Studio is installed without the C++ workload.       >> "%LOG%"
echo Visual Studio found at: %ANYVS%                                    >> "%LOG%"
goto fail_exit

:err_no_cpp
echo.
echo   BUILD CANNOT START - the Visual Studio C++ compiler setup is missing.
echo.
echo   Found Visual Studio here: %VSPATH%
echo   But this file, which switches the compiler on, is not there:
echo   %VCVARS%
echo.
echo   Fix it: open "Visual Studio Installer" from the Start menu, click
echo   "Modify" on your Visual Studio, tick "Desktop development with C++",
echo   then click Modify to install it.  Run this script again afterwards.
echo.
echo   See BUILDING.md next to this script for the full walkthrough.
echo.
echo REASON: the C++ compiler setup file is missing.                    >> "%LOG%"
echo Visual Studio found at: %VSPATH%                                   >> "%LOG%"
echo Missing file: %VCVARS%                                             >> "%LOG%"
goto fail_exit

:err_no_cmake
echo.
echo   BUILD CANNOT START - CMake was not found on this PC.
echo.
echo   CMake is the tool that turns this project into something Visual Studio
echo   can build.  There are two ways to get it, either is fine:
echo     A. Download it from https://cmake.org/download/ - pick the Windows
echo        x64 Installer, and on the options page choose
echo        "Add CMake to the system PATH for all users".
echo     B. Or open "Visual Studio Installer", click Modify, go to the
echo        "Individual components" tab and tick "C++ CMake tools for Windows".
echo.
echo   Then run this script again.
echo.
echo   See BUILDING.md next to this script for the full walkthrough.
echo.
echo REASON: CMake was not found on this PC.                            >> "%LOG%"
goto fail_exit

:err_vcvars
echo.
echo   BUILD CANNOT START - the Visual Studio C++ environment failed to load.
echo.
echo   Tried to run: %VCVARS%
echo.
echo   The install looks damaged.  Open "Visual Studio Installer" from the
echo   Start menu, click the three dots next to your Visual Studio and choose
echo   "Repair", then run this script again.
echo.
echo REASON: the Visual Studio C++ environment failed to load.          >> "%LOG%"
echo Tried to run: %VCVARS%                                             >> "%LOG%"
goto fail_exit

:err_configure
echo.
echo   BUILD CANNOT START - the first-time project setup step failed.
echo.
echo   Open build_log.txt in this folder and look near the bottom for a line
echo   starting with "CMake Error".  Common causes:
echo     - The checkout is incomplete.  This project carries its own copies of
echo       the libraries it needs, so download the whole repository.
echo     - CMake is too old for your Visual Studio.  Version 3.22 is the bare
echo       minimum, but install the newest one from https://cmake.org/download/
echo       if the log mentions the words "generator" or "Could not create".
echo.
echo   Log file: %LOG%
echo.
echo REASON: the first-time project setup step failed.                  >> "%LOG%"
goto fail_exit

rem ============================================================================
rem This one cannot write to the log, because being unable to write the log is
rem what it reports.  Stopping is deliberate: an untouched log from an earlier
rem run is exactly the file that makes a failed build read as a passing one.
rem ============================================================================
:err_log_locked
echo.
echo   BUILD CANNOT START - build_log.txt cannot be replaced.
echo.
echo   The file is: %LOG%
echo   Something else is holding it open, or this folder is read-only.
echo.
echo   Close whatever has build_log.txt open and run this script again.
echo   The build stops here on purpose: an out-of-date log would report the
echo   result of the LAST build, and could tell you this one worked.
echo.
rem The log could not be TRUNCATED, but appending is a different operation and
rem usually still succeeds (a reader holding it with FILE_SHARE_WRITE, an editor,
rem an indexer).  Try, because a stale log left intact is exactly the false pass
rem this exit exists to prevent: every gate reader greps these codes.
>> "%LOG%" echo === BUILD DID NOT START - THIS LOG IS STALE ABOVE THIS LINE ===
>> "%LOG%" echo RELEASE_EXIT_CODE=1
>> "%LOG%" echo DEBUG_EXIT_CODE=1
>> "%LOG%" echo HELPER64_EXIT_CODE=1
>> "%LOG%" echo HELPER32_CONFIG_EXIT_CODE=1
>> "%LOG%" echo HELPER32_EXIT_CODE=1
>> "%LOG%" echo ARTEFACTS_EXIT_CODE=1
if /i "%~1"=="pause" pause
exit /b 1

rem ============================================================================
rem Shared failure tail for every "cannot start" exit above.  It stamps the five
rem gate strings with non-zero values, so the log a stopped run leaves behind
rem states a failure in the exact terms every downstream check reads, rather
rem than leaving those checks to interpret an absence.  Callers log their own
rem REASON line first.
rem ============================================================================
:fail_exit
echo Nothing was compiled on this run.                                  >> "%LOG%"
echo RELEASE_EXIT_CODE=1                                                >> "%LOG%"
echo DEBUG_EXIT_CODE=1                                                  >> "%LOG%"
echo HELPER64_EXIT_CODE=1                                               >> "%LOG%"
echo HELPER32_CONFIG_EXIT_CODE=1                                        >> "%LOG%"
echo HELPER32_EXIT_CODE=1                                               >> "%LOG%"
echo ARTEFACTS_EXIT_CODE=1                                              >> "%LOG%"
echo BUILD FAILED                                                       >> "%LOG%"
if /i "%~1"=="pause" pause
exit /b 1
