@echo off
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 > nul 2>&1
cd /d C:\Users\jeffm\Documents\Vibesynth\build
ninja VibesynthStandalone -v > C:\Users\jeffm\Documents\Vibesynth\build_verbose.txt 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> C:\Users\jeffm\Documents\Vibesynth\build_verbose.txt
