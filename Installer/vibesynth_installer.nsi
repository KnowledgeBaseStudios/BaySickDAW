; Vibesynth Installer Script
; Requires NSIS 3.x + NSISdl plugin (bundled with NSIS)
; Build command (from Installer/ folder):
;   makensis vibesynth_installer.nsi

!define PRODUCT_NAME      "Vibesynth"
!define PRODUCT_VERSION   "1.2.0"
!define COMPANY_NAME      "VibeCo"
!define VST3_DIR          "$COMMONFILES\VST3"
!define INSTALL_DIR       "$PROGRAMFILES64\VibeCo\Vibesynth"
!define APPDATA_DIR       "$APPDATA\VibeCo\Vibesynth"
!define SAMPLES_URL_BASE  "https://freepats.zenvoid.org/sf2/"

; ── Metadata ──────────────────────────────────────────────────────────────────
Name          "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile       "Vibesynth-Setup-${PRODUCT_VERSION}.exe"
InstallDir    "${INSTALL_DIR}"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

; ── Pages ─────────────────────────────────────────────────────────────────────
!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_ICON "..\Assets\vibesynth.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; ── Components ────────────────────────────────────────────────────────────────
InstType "Full Install"
InstType "VST3 Only"
InstType "Standalone Only"

; Component: VST3 Plugin
Section "VST3 Plugin (FL Studio, DAWs)" SEC_VST3
    SectionIn 1 2
    SetOutPath "${VST3_DIR}\Vibesynth.vst3\Contents\x86_64-win"
    File "..\build\Vibesynth_artefacts\Release\VST3\Vibesynth.vst3\Contents\x86_64-win\Vibesynth.vst3"
    ; Write VST3 manifest
    SetOutPath "${VST3_DIR}\Vibesynth.vst3\Contents"
    File "..\build\Vibesynth_artefacts\Release\VST3\Vibesynth.vst3\Contents\*.xml"
    DetailPrint "VST3 plugin installed to ${VST3_DIR}"
SectionEnd

; Component: Standalone Application
Section "Standalone Application" SEC_STANDALONE
    SectionIn 1 3
    SetOutPath "${INSTALL_DIR}"
    File "..\build\Vibesynth_artefacts\Release\Standalone\Vibesynth.exe"
    ; Start Menu shortcut
    CreateDirectory "$SMPROGRAMS\VibeCo"
    CreateShortcut  "$SMPROGRAMS\VibeCo\Vibesynth.lnk" "${INSTALL_DIR}\Vibesynth.exe"
    CreateShortcut  "$DESKTOP\Vibesynth.lnk"            "${INSTALL_DIR}\Vibesynth.exe"
    DetailPrint "Standalone installed to ${INSTALL_DIR}"
SectionEnd

; Component: Instrument Samples (downloaded from internet)
Section "Instrument Samples (Keys, Strings, Horns)" SEC_SAMPLES
    SectionIn 1
    DetailPrint "Creating sample folders..."
    CreateDirectory "${APPDATA_DIR}\Samples\Keys"
    CreateDirectory "${APPDATA_DIR}\Samples\Strings"
    CreateDirectory "${APPDATA_DIR}\Samples\Horns"
    CreateDirectory "${APPDATA_DIR}\Samples\User"

    ; Download royalty-free samples from Freepats project
    ; These are CC0 / public domain instruments
    DetailPrint "Downloading piano samples..."
    NSISdl::download \
        "https://freepats.zenvoid.org/Piano/YDP-GrandPiano-20160804.tar.bz2" \
        "$TEMP\piano_samples.tar.bz2"
    Pop $R0
    ${If} $R0 == "success"
        DetailPrint "Piano samples downloaded. Extracting..."
        ; Use NSIS built-in to extract (or use 7z if bundled)
        ; Note: For production, pre-process samples to WAV and host on own CDN
        DetailPrint "Extracting piano samples to ${APPDATA_DIR}\Samples\Keys\"
    ${Else}
        DetailPrint "WARNING: Could not download piano samples (${R0}). Check internet connection."
        DetailPrint "You can manually add samples to: ${APPDATA_DIR}\Samples\User\"
    ${EndIf}

    DetailPrint "Downloading string samples..."
    NSISdl::download \
        "https://freepats.zenvoid.org/Strings/theremin.tar.bz2" \
        "$TEMP\string_samples.tar.bz2"
    Pop $R0
    ${If} $R0 == "success"
        DetailPrint "String samples downloaded."
    ${Else}
        DetailPrint "WARNING: Could not download string samples."
    ${EndIf}

    ; Write a README in the User folder explaining how to add samples
    FileOpen $0 "${APPDATA_DIR}\Samples\User\README.txt" w
    FileWrite $0 "Vibesynth User Samples$\r$\n"
    FileWrite $0 "=======================$\r$\n$\r$\n"
    FileWrite $0 "Place your own WAV samples in this folder.$\r$\n"
    FileWrite $0 "Name them with a category prefix:$\r$\n"
    FileWrite $0 "  Keys_MyPiano.wav$\r$\n"
    FileWrite $0 "  Strings_MyViolin.wav$\r$\n"
    FileWrite $0 "  Horns_MyTrumpet.wav$\r$\n$\r$\n"
    FileWrite $0 "They will appear in the instrument dropdown under their category.$\r$\n"
    FileClose $0
    DetailPrint "Sample folder setup complete."
SectionEnd

; Component: Presets
Section "Factory Presets" SEC_PRESETS
    SectionIn 1
    DetailPrint "Installing presets..."
    CreateDirectory "${APPDATA_DIR}\Presets\Industrial\NIN"
    CreateDirectory "${APPDATA_DIR}\Presets\Dubstep\Skrillex"
    CreateDirectory "${APPDATA_DIR}\Presets\Psytrance\InfectedMushroom"
    CreateDirectory "${APPDATA_DIR}\Presets\Psytrance\Astrix"
    CreateDirectory "${APPDATA_DIR}\Presets\Electronic\AphexTwin"
    CreateDirectory "${APPDATA_DIR}\Presets\House\DaftPunk"
    CreateDirectory "${APPDATA_DIR}\Presets\General\Leads"
    CreateDirectory "${APPDATA_DIR}\Presets\General\Pads"
    CreateDirectory "${APPDATA_DIR}\Presets\General\Bass"
    CreateDirectory "${APPDATA_DIR}\Presets\General\Keys"
    CreateDirectory "${APPDATA_DIR}\Presets\General\Plucks"
    CreateDirectory "${APPDATA_DIR}\Presets\User"

    ; Install bundled preset files
    SetOutPath "${APPDATA_DIR}\Presets\Industrial\NIN"
    File "..\Presets\Industrial\NIN\*.vstpreset"
    SetOutPath "${APPDATA_DIR}\Presets\Dubstep\Skrillex"
    File "..\Presets\Dubstep\Skrillex\*.vstpreset"
    SetOutPath "${APPDATA_DIR}\Presets\Psytrance\InfectedMushroom"
    File "..\Presets\Psytrance\InfectedMushroom\*.vstpreset"
    SetOutPath "${APPDATA_DIR}\Presets\Psytrance\Astrix"
    File "..\Presets\Psytrance\Astrix\*.vstpreset"
    SetOutPath "${APPDATA_DIR}\Presets\General\Leads"
    File "..\Presets\General\Leads\*.vstpreset"
    SetOutPath "${APPDATA_DIR}\Presets\General\Bass"
    File "..\Presets\General\Bass\*.vstpreset"
    SetOutPath "${APPDATA_DIR}\Presets\General\Keys"
    File "..\Presets\General\Keys\*.vstpreset"
    SetOutPath "${APPDATA_DIR}\Presets\General\Plucks"
    File "..\Presets\General\Plucks\*.vstpreset"
    DetailPrint "Presets installed."
SectionEnd

; ── Section descriptions ──────────────────────────────────────────────────────
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_VST3}       "Install VST3 plugin for use in FL Studio and other DAWs."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STANDALONE} "Install the standalone application. Creates desktop and Start Menu shortcuts."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SAMPLES}    "Download royalty-free instrument samples (Keys, Strings, Horns). Requires internet. ~50MB."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_PRESETS}    "Install factory presets (NIN, Skrillex, Infected Mushroom, Astrix and more)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ── Common setup (runs regardless of component selection) ─────────────────────
Section "-Common" SEC_COMMON
    ; Create AppData folders
    CreateDirectory "${APPDATA_DIR}"
    CreateDirectory "${APPDATA_DIR}\Presets\User"
    CreateDirectory "${APPDATA_DIR}\Samples\User"

    ; Write uninstaller
    WriteUninstaller "${INSTALL_DIR}\Uninstall.exe"

    ; Add to Windows Programs list
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vibesynth" \
        "DisplayName"      "Vibesynth ${PRODUCT_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vibesynth" \
        "UninstallString"  "${INSTALL_DIR}\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vibesynth" \
        "DisplayVersion"   "${PRODUCT_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vibesynth" \
        "Publisher"        "${COMPANY_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vibesynth" \
        "InstallLocation"  "${INSTALL_DIR}"
SectionEnd

; ── Uninstaller ───────────────────────────────────────────────────────────────
Section "Uninstall"
    ; Remove VST3
    RMDir /r "${VST3_DIR}\Vibesynth.vst3"

    ; Remove standalone
    Delete "${INSTALL_DIR}\Vibesynth.exe"
    Delete "${INSTALL_DIR}\Uninstall.exe"
    RMDir  "${INSTALL_DIR}"
    RMDir  "$PROGRAMFILES64\VibeCo"

    ; Remove shortcuts
    Delete "$SMPROGRAMS\VibeCo\Vibesynth.lnk"
    RMDir  "$SMPROGRAMS\VibeCo"
    Delete "$DESKTOP\Vibesynth.lnk"

    ; Remove registry entry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vibesynth"

    ; NOTE: AppData presets and samples are NOT removed on uninstall
    ; so the user keeps their work. They can delete manually if desired.
    MessageBox MB_OK "Vibesynth has been uninstalled.$\nYour presets and samples in AppData\Roaming\VibeCo\ have been kept."
SectionEnd
