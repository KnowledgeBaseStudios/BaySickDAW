@echo off
rem Reset PATH to bare minimum so vcvars64.bat can append without hitting the 8191-char limit
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 > nul 2>&1

rem QA-0a (2026-05-07): build BOTH configs.  Release is the shipping exe and
rem the one used for music production.  Debug ships internal jassert checks
rem -- a failing condition pops a dialog with file:line so regressions are
rem visible to a non-coder solo developer.  Standing workflow: verify Claude
rem fixes in Debug FIRST, then re-run in Release for the actual user test.

rem Smoke round 3 (crash hunt, 2026-07-24): archive the OUTGOING Release
rem exe+pdb pair BEFORE the build overwrites them, so a WER crash dump from
rem any earlier build can always be symbolized against its matching pair
rem (matched by the PE TimeDateStamp inside the exe, not the folder name).
rem Keeps the 5 newest pairs (~850 MB); prunes older automatically.
powershell -NoProfile -Command "$r='C:\Users\jeffm\Documents\BaySickDAW\build\BaySickDAWStandalone_artefacts\Release'; $s='C:\Users\jeffm\Documents\BaySickDAW\SymbolStore'; if (Test-Path ($r+'\BaySickDAW.exe')) { $t=(Get-Item ($r+'\BaySickDAW.exe')).LastWriteTime.ToString('yyyyMMdd-HHmmss'); $d=Join-Path $s $t; if (!(Test-Path $d)) { New-Item -ItemType Directory -Force $d | Out-Null; Copy-Item ($r+'\BaySickDAW.exe') $d; Copy-Item ($r+'\BaySickDAW.pdb') $d -ErrorAction SilentlyContinue }; Get-ChildItem $s -Directory | Sort-Object Name -Descending | Select-Object -Skip 5 | Remove-Item -Recurse -Force }"

echo === Building Release ===                                          > "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\BaySickDAW\build" --target BaySickDAWStandalone --config Release -j4 >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo RELEASE_EXIT_CODE=%ERRORLEVEL%                                   >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"

echo === Building Debug ===                                           >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\BaySickDAW\build" --target BaySickDAWStandalone --config Debug   -j4 >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo DEBUG_EXIT_CODE=%ERRORLEVEL%                                     >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"

rem QA-ModelShell TS6 (BLU-302, 2026-07-29): the plugin-sandbox helper.  A
rem SEPARATE target, so the two lines above would never have built it -- and a
rem broken helper would have gone unnoticed until a bridged plugin failed to
rem start at runtime.  Release only: the helper is a plain IPC + hosting shim
rem with no jasserts of ours to catch, and the app locates it by filename
rem regardless of which config launched it.
rem
rem GATE CRITERION, changed by this step: the log now carries THREE exit codes,
rem and the "vcxproj -> ....exe" link-line count is 3 (two BaySickDAW.exe plus
rem one BaySickPluginHost64.exe), not 2.
echo === Building Plugin Host helper (x64) ===                        >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\BaySickDAW\build" --target BaySickPluginHost --config Release -j4 >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo HELPER64_EXIT_CODE=%ERRORLEVEL%                                  >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"

rem QA-ModelShell TS6 (BLU-302 step 7b): the 32-bit helper.  A 64-bit process
rem cannot load a 32-bit VST3 at all, so this binary IS our 32-bit support.
rem
rem It needs its OWN build tree because an MSVC generator is single-platform per
rem tree -- hence the separate configure below against the standalone helper
rem project at Source\Hosting\Helper, which depends on vendored JUCE and nothing
rem else (configuring the ROOT project as Win32 would drag in x86 builds of
rem sfizz / NAM / RubberBand / LAME / WORLD / lunasvg).
rem Configure is idempotent; CMake no-ops when the cache is already good.
echo === Configuring Plugin Host helper (x86) ===                     >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" -S "C:\Users\jeffm\Documents\BaySickDAW\Source\Hosting\Helper" -B "C:\Users\jeffm\Documents\BaySickDAW\build32" -A Win32 >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo HELPER32_CONFIG_EXIT_CODE=%ERRORLEVEL%                           >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"

echo === Building Plugin Host helper (x86) ===                        >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\BaySickDAW\build32" --target BaySickPluginHost --config Release -j4 >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo HELPER32_EXIT_CODE=%ERRORLEVEL%                                  >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"

rem Stage the x86 helper beside the app exes (its own build tree cannot see
rem BaySickDAWStandalone's output dir, so the copy happens here rather than in
rem a POST_BUILD command).
copy /Y "C:\Users\jeffm\Documents\BaySickDAW\build32\BaySickPluginHost_artefacts\Release\BaySickPluginHost32.exe" "C:\Users\jeffm\Documents\BaySickDAW\build\BaySickDAWStandalone_artefacts\Release\" >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
copy /Y "C:\Users\jeffm\Documents\BaySickDAW\build32\BaySickPluginHost_artefacts\Release\BaySickPluginHost32.exe" "C:\Users\jeffm\Documents\BaySickDAW\build\BaySickDAWStandalone_artefacts\Debug\"   >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
