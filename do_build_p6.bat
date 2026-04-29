@echo off
cd /d C:\Users\jeffm\Documents\Vibesynth
del /f /q "build\CMakeFiles\VibesynthStandalone.dir\Source\Standalone\StandaloneApp.cpp.obj" 2>nul
del /f /q "build\CMakeFiles\VibesynthStandalone.dir\Source\Standalone\StandaloneEditor.cpp.obj" 2>nul
del /f /q "build\CMakeFiles\VibesynthStandalone.dir\Source\Standalone\LayersPage.cpp.obj" 2>nul
del /f /q "build\CMakeFiles\VibesynthStandalone.dir\Source\Standalone\BassPage.cpp.obj" 2>nul
del /f /q "build\CMakeFiles\VibesynthStandalone.dir\Source\Standalone\DrumsPage.cpp.obj" 2>nul
del /f /q "build\CMakeFiles\VibesynthStandalone.dir\Source\Standalone\PianoRoll.cpp.obj" 2>nul
del /f /q "build\CMakeFiles\VibesynthStandalone.dir\Source\PluginProcessor.cpp.obj" 2>nul
echo [Phase6 build started] > build_log.txt
cmake --build build --config Release -j4 >> build_log.txt 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> build_log.txt
