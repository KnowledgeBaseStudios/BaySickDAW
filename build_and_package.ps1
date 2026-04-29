# Vibesynth Build + Package Script
# Works from any PowerShell window -- auto-locates MSVC and Ninja.
# Usage: powershell -ExecutionPolicy Bypass -File build_and_package.ps1

$ErrorActionPreference = "Stop"
$ProjectRoot  = $PSScriptRoot
$BuildDir     = "$ProjectRoot\build"
$InstallerDir = "$ProjectRoot\Installer"
$PresetsDir   = "$ProjectRoot\Presets"

Write-Host "=== Vibesynth Build + Package ===" -ForegroundColor Cyan

# ── Step 0: Locate and import MSVC build environment ─────────────────────────
Write-Host "`n[0/5] Locating build tools..." -ForegroundColor Yellow

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    $vsWhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
}
if (-not (Test-Path $vsWhere)) {
    throw "Visual Studio Installer not found. Install VS 2019+ with the C++ workload."
}

$vsPath = & $vsWhere -latest -property installationPath
if (-not $vsPath) { throw "No Visual Studio installation found." }
Write-Host "  Visual Studio: $vsPath" -ForegroundColor Green

# Source vcvars64.bat to set PATH, LIB, INCLUDE for x64
$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found. Install C++ workload in VS." }
Write-Host "  Importing MSVC x64 environment..." -ForegroundColor Yellow
$envLines = cmd /c "`"$vcvars`" > nul 2>&1 && set" 2>&1
foreach ($line in $envLines) {
    if ($line -match "^([^=]+)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}
$clTest = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clTest) { throw "cl.exe not on PATH after vcvars64. Check VS installation." }
Write-Host "  Compiler: $($clTest.Source)" -ForegroundColor Green

# Find Ninja: PATH first, then VS-bundled, then download
$ninjaExe = $(if ($nc = Get-Command ninja.exe -ErrorAction SilentlyContinue) { $nc.Source } else { $null })
if (-not $ninjaExe) {
    $nf = Get-ChildItem -Path $vsPath -Filter "ninja.exe" -Recurse -ErrorAction SilentlyContinue |
          Select-Object -First 1
    if ($nf) { $ninjaExe = $nf.FullName; $env:PATH = "$(Split-Path $ninjaExe);$env:PATH" }
}
if (-not $ninjaExe) {
    Write-Host "  Downloading Ninja..." -ForegroundColor Yellow
    $nd = "$env:TEMP\vibesynth_ninja"; New-Item -ItemType Directory -Path $nd -Force | Out-Null
    Invoke-WebRequest "https://github.com/ninja-build/ninja/releases/latest/download/ninja-win.zip" `
        -OutFile "$nd\ninja.zip" -UseBasicParsing
    Expand-Archive "$nd\ninja.zip" -DestinationPath $nd -Force
    $ninjaExe = "$nd\ninja.exe"; $env:PATH = "$nd;$env:PATH"
}
Write-Host "  Ninja: $ninjaExe" -ForegroundColor Green

# ── Step 1: CMake configure ───────────────────────────────────────────────────
Write-Host "`n[1/5] Configuring CMake..." -ForegroundColor Yellow
if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Path $BuildDir | Out-Null
Push-Location $BuildDir
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="$ninjaExe"
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "CMake configure failed" }

# ── Step 2: Build ─────────────────────────────────────────────────────────────
Write-Host "`n[2/5] Building..." -ForegroundColor Yellow
cmake --build . -j8
$buildResult = $LASTEXITCODE
Pop-Location

# ── DNS / Network cleanup (run BEFORE throwing on build failure) ──────────────
# Flush DNS cache and reset Winsock to prevent DNS resolver crashes
# that can occur after sustained network activity during CMake's JUCE fetch.
# Since we now use a local JUCE copy (no FetchContent), this is belt-and-suspenders.
Write-Host "`n  Flushing DNS cache..." -ForegroundColor DarkGray
try {
    ipconfig /flushdns | Out-Null
    # Reset Winsock stack -- clears any lingering socket state
    netsh winsock reset catalog | Out-Null
    # Release and renew DNS registration (gentle -- does not drop connection)
    ipconfig /registerdns | Out-Null
    Write-Host "  DNS flushed and Winsock reset." -ForegroundColor DarkGray
} catch {
    Write-Host "  DNS flush skipped (non-fatal): $_" -ForegroundColor DarkGray
}

if ($buildResult -ne 0) { throw "Build failed" }
Write-Host "Build successful." -ForegroundColor Green

# ── Step 3: Verify outputs ────────────────────────────────────────────────────
Write-Host "`n[3/5] Verifying output files..." -ForegroundColor Yellow
$vst3 = "$BuildDir\Vibesynth_artefacts\Release\VST3\Vibesynth.vst3\Contents\x86_64-win\Vibesynth.vst3"
$exe  = "$BuildDir\VibesynthStandalone_artefacts\Release\Vibesynth.exe"
if (-not (Test-Path $vst3)) { throw "VST3 not found: $vst3" }
if (-not (Test-Path $exe))  { throw "Standalone not found: $exe" }
Write-Host "  VST3:       $vst3" -ForegroundColor Green
Write-Host "  Standalone: $exe"  -ForegroundColor Green

# ── Step 4: Organise presets ─────────────────────────────────────────────────
Write-Host "`n[4/5] Organising presets..." -ForegroundColor Yellow
$presetZip = "$ProjectRoot\Vibesynth_Presets_v2.zip"
if (Test-Path $presetZip) {
    Expand-Archive $presetZip -DestinationPath "$ProjectRoot\Presets" -Force
    Write-Host "  Presets extracted." -ForegroundColor Green
} elseif (Test-Path "$ProjectRoot\Presets") {
    Write-Host "  Using existing Presets\ folder." -ForegroundColor Green
} else {
    Write-Warning "  No presets found -- installer will skip preset component."
}

# ── Step 5: NSIS installer ────────────────────────────────────────────────────
Write-Host "`n[5/5] Building installer..." -ForegroundColor Yellow
$nsisPath = "C:\Program Files (x86)\NSIS\makensis.exe"
if (-not (Test-Path $nsisPath)) { $nsisPath = "C:\Program Files\NSIS\makensis.exe" }
if (-not (Test-Path $nsisPath)) {
    Write-Warning "  NSIS not found. Get it from https://nsis.sourceforge.io/"
    Write-Host "`n  Build complete (no installer). Re-run after installing NSIS." -ForegroundColor Yellow
    exit 0
}
Push-Location $InstallerDir
& $nsisPath vibesynth_installer.nsi
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "NSIS build failed" }
Pop-Location

$installerFile = "$InstallerDir\Vibesynth-Setup-1.2.0.exe"
if (Test-Path $installerFile) {
    Write-Host "`n=== BUILD COMPLETE ===" -ForegroundColor Green
    Write-Host "Installer: $installerFile" -ForegroundColor Green
} else {
    Write-Warning "Installer file not found -- check NSIS output above."
}
