@echo off
setlocal EnableExtensions

rem QA-EqPro: build + run the EQ engine proof target (Tools/EqTests).
rem Separate from do_build.bat by design - the app gate stays six exit codes.
rem Result contract: eq_tests_log.txt carries EQTESTS_BUILD_EXIT_CODE and
rem EQTESTS_RUN_EXIT_CODE; both must be 0.

set "REPO=%~dp0"
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"
set "LOG=%REPO%\eq_tests_log.txt"
type nul > "%LOG%"

for %%I in (cmake.exe) do if not defined CMAKE set "CMAKE=%%~$PATH:I"
if not defined CMAKE if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
if not defined CMAKE (
    echo EQTESTS_BUILD_EXIT_CODE=99 >> "%LOG%"
    echo EQTESTS_RUN_EXIT_CODE=99 >> "%LOG%"
    exit /b 99
)

"%CMAKE%" --build "%REPO%\build" --target BaySickEqTests --config Release -j4 >> "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"
echo EQTESTS_BUILD_EXIT_CODE=%RC% >> "%LOG%"
if not "%RC%"=="0" (
    echo EQTESTS_RUN_EXIT_CODE=98 >> "%LOG%"
    exit /b %RC%
)

"%REPO%\build\Release\BaySickEqTests.exe" >> "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"
echo EQTESTS_RUN_EXIT_CODE=%RC% >> "%LOG%"
exit /b %RC%
