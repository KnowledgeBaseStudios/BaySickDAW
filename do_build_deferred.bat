@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 > nul 2>&1
cd /d C:\Users\jeffm\Documents\Vibesynth
set BASE=build\CMakeFiles\VibesynthStandalone.dir\Source
del /f /q "%BASE%\Standalone\PianoRoll.cpp.obj"        2>nul
del /f /q "%BASE%\Standalone\StandaloneApp.cpp.obj"    2>nul
del /f /q "%BASE%\Standalone\StandaloneEditor.cpp.obj" 2>nul
del /f /q "%BASE%\Standalone\LayersPage.cpp.obj"       2>nul
del /f /q "%BASE%\Standalone\BassPage.cpp.obj"         2>nul
del /f /q "%BASE%\Standalone\DrumsPage.cpp.obj"        2>nul
del /f /q "%BASE%\PluginProcessor.cpp.obj"             2>nul
echo [Deferred A+B+Rename build started] > build_log.txt
cmake --build build --target VibesynthStandalone --config Release -j4 >> build_log.txt 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> build_log.txt
