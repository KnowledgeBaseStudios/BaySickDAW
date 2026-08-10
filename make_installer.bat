@echo off
setlocal EnableExtensions

rem ============================================================================
rem BaySickDAW TESTER installer build script.
rem
rem THIS DOES NOT BUILD THE SHIPPING INSTALLER.  It packages the current Release
rem build so a tester can be handed one file instead of a build tree.  The real
rem release installer belongs to the QA-Installer batch, which is blocked on
rem QA-Manuals, QA-Templates and QA-LegalReview - none of which have run.  There
rem is no manual, no license page, no legal review and no code signature in what
rem this produces.  See Installer\BaySickDAW-Tester.nsi for the full list of
rem what is and is not in the package.
rem
rem Run do_build.bat FIRST.  This script only packages what is already built; it
rem never compiles anything, and it refuses to run if any input is missing
rem rather than producing a package that fails at run time on a tester's PC.
rem
rem PORTABLE, same as do_build.bat: the repo root comes from this file's own
rem location and NSIS is located at run time, so a checkout at any path,
rem including one with spaces, packages without editing this file.
rem
rem NO "call :label" ANYWHERE IN THIS FILE, DELIBERATELY, for the same reason
rem do_build.bat avoids it: .gitattributes sets "* text=auto", so a clone made
rem with core.autocrlf off lands this script on disk with LF-only line endings,
rem and cmd.exe cannot resolve a CALL target in an LF-only batch file.  GOTO
rem survives that; CALL does not.
rem ============================================================================

rem %~dp0 always ends in a backslash; strip it so "%REPO%\x" never doubles up.
set "REPO=%~dp0"
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"

set "LOG=%REPO%\installer_log.txt"
set "NSI=%REPO%\Installer\BaySickDAW-Tester.nsi"
set "REL=%REPO%\build\BaySickDAWStandalone_artefacts\Release"

rem ----------------------------------------------------------------------------
rem The log is created and emptied HERE, before anything can exit, for the same
rem reason do_build.bat does it: a leftover log from an earlier run would
rem describe a package this run never produced.
rem ----------------------------------------------------------------------------
del /f /q "%LOG%" >nul 2>&1
if exist "%LOG%" goto err_log_locked
echo === BaySickDAW tester installer started %DATE% %TIME% ===          > "%LOG%"
echo Repo root: %REPO%                                                 >> "%LOG%"
if not exist "%LOG%" goto err_log_locked

set "PF=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"
if not defined PF86 set "PF86=%PF%"

rem ----------------------------------------------------------------------------
rem Locate NSIS.  PATH first, then the two standard install folders, then the
rem key the NSIS installer writes, so a non-standard install location is still
rem found without anyone editing this file.
rem ----------------------------------------------------------------------------
set "MAKENSIS="
for %%I in (makensis.exe) do if not defined MAKENSIS set "MAKENSIS=%%~$PATH:I"
if not defined MAKENSIS if exist "%PF86%\NSIS\makensis.exe" set "MAKENSIS=%PF86%\NSIS\makensis.exe"
if not defined MAKENSIS if exist "%PF%\NSIS\makensis.exe"   set "MAKENSIS=%PF%\NSIS\makensis.exe"
if not defined MAKENSIS for /f "usebackq tokens=2,*" %%A in (`reg query "HKLM\SOFTWARE\NSIS" /ve 2^>nul`) do if exist "%%B\makensis.exe" set "MAKENSIS=%%B\makensis.exe"
if not defined MAKENSIS for /f "usebackq tokens=2,*" %%A in (`reg query "HKLM\SOFTWARE\WOW6432Node\NSIS" /ve 2^>nul`) do if exist "%%B\makensis.exe" set "MAKENSIS=%%B\makensis.exe"
if not defined MAKENSIS goto err_no_nsis

if not exist "%NSI%" goto err_no_script

rem ----------------------------------------------------------------------------
rem Every input, checked before anything runs.  The .nsi checks these again at
rem compile time and refuses to build without them; this pass exists so the
rem message a person reads is in plain English rather than a makensis error.
rem ----------------------------------------------------------------------------
set "MISSING="
if not exist "%REL%\BaySickDAW.exe"           set "MISSING=BaySickDAW.exe"
if not exist "%REL%\BaySickPluginHost64.exe"  set "MISSING=BaySickPluginHost64.exe"
if not exist "%REL%\BaySickPluginHost32.exe"  set "MISSING=BaySickPluginHost32.exe"
if not exist "%REL%\Resources"                set "MISSING=the Resources folder next to the app"
if defined MISSING goto err_no_build

set "MISSING="
if not exist "%REPO%\Presets"   set "MISSING=Presets"
if not exist "%REPO%\Templates" set "MISSING=Templates"
if defined MISSING goto err_no_content

rem ----------------------------------------------------------------------------
rem The version comes out of CMakeLists.txt, never out of a second copy that
rem could drift from it.  The line looks like: project(BaySickDAW VERSION 1.2.0)
rem Failing here is deliberate: a package that claims the wrong version is worse
rem than no package, because a tester's bug report would name a build that does
rem not exist.
rem ----------------------------------------------------------------------------
set "APP_VER="
for /f "usebackq tokens=4 delims=() " %%I in (`findstr /b /c:"project(BaySickDAW VERSION" "%REPO%\CMakeLists.txt"`) do if not defined APP_VER set "APP_VER=%%I"
if not defined APP_VER goto err_no_version

rem ----------------------------------------------------------------------------
rem BUILD STAMP IN THE FILENAME, and it is load-bearing.  APP_VER moves only when
rem CMakeLists changes, so every package built between version bumps had an
rem IDENTICAL name.  A tester who already had one on disk would re-run the old
rem package, install the old app, and have nothing on screen to tell them -- that
rem cost a whole diagnosis cycle on 2026-08-10, chasing a fixed bug that was
rem simply not in the binary being run.  The stamp is the APP EXE's own
rem last-write time, so it identifies the BUILD rather than the packaging run.
rem ----------------------------------------------------------------------------
set "BUILD_STAMP="
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "(Get-Item '%REL%\BaySickDAW.exe').LastWriteTime.ToString('yyyyMMdd-HHmm')"`) do set "BUILD_STAMP=%%I"
if not defined BUILD_STAMP goto err_no_stamp

set "OUT=%REPO%\Installer\BaySickDAW-%APP_VER%-%BUILD_STAMP%-Tester-Setup.exe"

echo.
echo Packaging the BaySickDAW TESTER installer.  This is not the release build.
echo   Repo root  : %REPO%
echo   NSIS       : %MAKENSIS%
echo   Version    : %APP_VER%
echo   Output     : %OUT%
echo   Full log   : %LOG%
echo.
echo Compressing.  This takes a minute or two.

echo === Toolchain found, packaging %DATE% %TIME% ===                   >> "%LOG%"
echo NSIS: %MAKENSIS%                                                   >> "%LOG%"
echo Version from CMakeLists.txt: %APP_VER%                             >> "%LOG%"
echo Output: %OUT%                                                      >> "%LOG%"

rem A stale package left on disk would survive a failed run and read as a fresh
rem one, exactly like a stale log.
del /f /q "%OUT%" >nul 2>&1

rem OUT_FILE must be passed or the .nsi falls back to its own undated default and
rem every build lands on the SAME filename -- the exact trap that had a stale
rem package reinstalled on 2026-08-10.  Quoted whole because the path has spaces.
"%MAKENSIS%" /V3 /DAPP_VER=%APP_VER% "/DOUT_FILE=%OUT%" "%NSI%"         >> "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"
echo MAKENSIS_EXIT_CODE=%RC%                                            >> "%LOG%"
if not "%RC%"=="0" goto err_makensis

rem The exit code can be zero while the file is not there - a locked or
rem read-only output path is the usual way that happens.
if not exist "%OUT%" goto err_no_output
echo PACKAGE_EXIT_CODE=0                                                >> "%LOG%"

echo.
echo ================= TESTER INSTALLER BUILT =================
echo   %OUT%
echo.
echo   This is a TESTER package, not a release.  It is not signed, so
echo   Windows will show "Windows protected your PC" when a tester runs
echo   it - they click "More info" then "Run anyway".
echo.
echo   It installs for the current user only and never asks for an
echo   administrator password.  Uninstalling leaves Documents\BaySickDAW
echo   alone, so projects, presets and the downloaded sounds all survive.
echo ==========================================================
echo.
echo INSTALLER BUILD SUCCEEDED                                           >> "%LOG%"

if /i "%~1"=="pause" pause
exit /b 0

rem ============================================================================
rem Failure exits.  Every one names what is missing and what to do about it in
rem plain English, because the people hitting them do not read code.
rem ============================================================================

:err_no_nsis
echo.
echo   INSTALLER CANNOT BE BUILT - NSIS was not found on this PC.
echo.
echo   NSIS is the free tool that turns the built app into a setup file.
echo   1. Go to https://nsis.sourceforge.io/Download
echo   2. Download the latest release and run the installer.
echo   3. Accept the default install folder.
echo   4. Run this script again.
echo.
echo   Nothing else about your machine needs changing - this script finds
echo   NSIS on its own once it is installed.
echo.
echo REASON: NSIS (makensis.exe) was not found on this PC.               >> "%LOG%"
goto fail_exit

:err_no_script
echo.
echo   INSTALLER CANNOT BE BUILT - the installer script is missing.
echo.
echo   Expected it here: %NSI%
echo.
echo   The checkout is incomplete.  Download the whole repository.
echo.
echo REASON: the installer script is missing.                            >> "%LOG%"
echo Expected: %NSI%                                                     >> "%LOG%"
goto fail_exit

:err_no_build
echo.
echo   INSTALLER CANNOT BE BUILT - the app has not been built yet.
echo.
echo   Missing: %MISSING%
echo   Looked in: %REL%
echo.
echo   Run do_build.bat first and wait for it to say ALL FIVE STEPS
echo   SUCCEEDED, then run this script again.
echo.
echo   This script only packages what is already built.  It never compiles
echo   anything, and it stops here rather than making a setup file that
echo   would fail on a tester's PC.
echo.
echo REASON: a required built file is missing: %MISSING%                 >> "%LOG%"
echo Looked in: %REL%                                                    >> "%LOG%"
goto fail_exit

:err_no_content
echo.
echo   INSTALLER CANNOT BE BUILT - the factory content folder is missing.
echo.
echo   Missing: %MISSING%
echo   Looked in: %REPO%
echo.
echo   The checkout is incomplete.  Download the whole repository, including
echo   the Presets and Templates folders.
echo.
echo REASON: the %MISSING% folder is missing from the repository.        >> "%LOG%"
goto fail_exit

:err_no_version
echo.
echo   INSTALLER CANNOT BE BUILT - the version number could not be read.
echo.
echo   This script reads the version from CMakeLists.txt, from the line
echo   that starts with:  project(BaySickDAW VERSION
echo.
echo   That line is not there, or its shape has changed.  Fix the line, or
echo   update the version lookup in this script to match it.
echo.
echo   It stops rather than guessing: a package that names the wrong version
echo   sends every tester bug report to a build that does not exist.
echo.
echo REASON: could not read the version from CMakeLists.txt.             >> "%LOG%"
goto fail_exit

:err_no_stamp
echo.
echo   INSTALLER CANNOT BE BUILT - could not read the build timestamp.
echo.
echo   The stamp comes from the last-write time of:
echo     %REL%\BaySickDAW.exe
echo.
echo   Without it every package would be named identically, and a tester
echo   holding an older one would re-run that instead and never know.
echo   Run do_build.bat first, then this script.
echo.
echo REASON: could not read the build timestamp from the app exe.        >> "%LOG%"
goto fail_exit

:err_makensis
echo.
echo   INSTALLER CANNOT BE BUILT - NSIS reported an error.
echo.
echo   Open installer_log.txt in this folder and read the bottom.  If it
echo   says a file is missing, run do_build.bat and try again.
echo.
echo   Log file: %LOG%
echo.
echo REASON: makensis failed with exit code %RC%.                        >> "%LOG%"
goto fail_exit

:err_no_output
echo.
echo   INSTALLER CANNOT BE BUILT - NSIS said it worked, but the setup file
echo   is not there.
echo.
echo   Expected: %OUT%
echo.
echo   Something is holding that file open, or the Installer folder is
echo   read-only.  Close anything using the old setup file and run this
echo   script again.
echo.
echo REASON: makensis exited 0 but the package is not on disk.           >> "%LOG%"
echo Expected: %OUT%                                                     >> "%LOG%"
goto fail_exit

rem ============================================================================
rem This one cannot write to the log, because being unable to write the log is
rem what it reports.
rem ============================================================================
:err_log_locked
echo.
echo   INSTALLER CANNOT BE BUILT - installer_log.txt cannot be replaced.
echo.
echo   The file is: %LOG%
echo   Something else is holding it open, or this folder is read-only.
echo.
echo   Close whatever has it open and run this script again.  It stops here
echo   on purpose: an out-of-date log would describe the LAST run, and could
echo   tell you this one worked.
echo.
if /i "%~1"=="pause" pause
exit /b 1

rem ============================================================================
rem Shared failure tail.  Stamps the strings a reader greps for, so a stopped
rem run leaves a log that states a failure in the same terms a successful one
rem states a pass, rather than leaving it to be inferred from an absence.
rem ============================================================================
:fail_exit
echo Nothing was packaged on this run.                                   >> "%LOG%"
echo MAKENSIS_EXIT_CODE=1                                                >> "%LOG%"
echo PACKAGE_EXIT_CODE=1                                                 >> "%LOG%"
echo INSTALLER BUILD FAILED                                              >> "%LOG%"
if /i "%~1"=="pause" pause
exit /b 1
