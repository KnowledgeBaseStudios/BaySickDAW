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
