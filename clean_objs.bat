@echo off
set BASE=C:\Users\jeffm\Documents\Vibesynth\build\CMakeFiles\VibesynthStandalone.dir\Source
del /f /q "%BASE%\Standalone\StandaloneApp.cpp.obj" 2>nul
del /f /q "%BASE%\Standalone\StandaloneEditor.cpp.obj" 2>nul
del /f /q "%BASE%\Standalone\LayersPage.cpp.obj" 2>nul
del /f /q "%BASE%\Standalone\BassPage.cpp.obj" 2>nul
del /f /q "%BASE%\Standalone\DrumsPage.cpp.obj" 2>nul
del /f /q "%BASE%\Standalone\PianoRoll.cpp.obj" 2>nul
del /f /q "%BASE%\PluginProcessor.cpp.obj" 2>nul
echo OBJ_CLEAN_DONE
