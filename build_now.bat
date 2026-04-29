@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
echo MSVC loaded > "C:\Users\jeffm\Documents\Vibesynth\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\Vibesynth\build" -j8 >> "C:\Users\jeffm\Documents\Vibesynth\build_log.txt" 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> "C:\Users\jeffm\Documents\Vibesynth\build_log.txt"
