@echo off
rem Reset PATH to bare minimum so vcvars64.bat can append without hitting the 8191-char limit
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 > nul 2>&1
"C:\Program Files\CMake\bin\cmake.exe" -S "C:\Users\jeffm\Documents\BaySickDAW" -B "C:\Users\jeffm\Documents\BaySickDAW\build" -G Ninja -DCMAKE_BUILD_TYPE=Release > "C:\Users\jeffm\Documents\BaySickDAW\configure_log.txt" 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> "C:\Users\jeffm\Documents\BaySickDAW\configure_log.txt"
