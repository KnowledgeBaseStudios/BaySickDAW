; ── VibeDAW Installer ─────────────────────────────────────────────────────────
; NSIS script — requires NSIS 3.x + inetc plugin
;
; Sample ZIPs hosted at github.com/JMG707/vibedaw-samples
;
; Build command (from NSIS script editor or command line):
;   makensis vibedam_installer.nsi
; ─────────────────────────────────────────────────────────────────────────────

!define APP_NAME    "VibeDAW"
!define APP_VER     "1.9"
!define APP_EXE     "Vibesynth.exe"
!define REG_KEY     "Software\KnowledgeBaseStudios\VibeDAW"
!define UNINSTALLER "Uninstall VibeDAW.exe"

; ── Sample download URLs ──────────────────────────────────────────────────────
!define URL_STRINGS    "https://github.com/JMG707/vibedaw-samples/raw/refs/heads/main/Strings%20Package.zip?download="
!define URL_BRASS      "https://github.com/JMG707/vibedaw-samples/raw/refs/heads/main/Brass%20Package.zip?download="
!define URL_WOODWINDS  "https://github.com/JMG707/vibedaw-samples/raw/refs/heads/main/Woodwinds%20Package.zip?download="
!define URL_KEYS       "https://github.com/JMG707/vibedaw-samples/raw/refs/heads/main/Keys%20Package.zip?download="
!define URL_PERCUSSION "https://github.com/JMG707/vibedaw-samples/raw/refs/heads/main/Percussion%20Package.zip?download="

; ── Installer metadata ────────────────────────────────────────────────────────
Name                "${APP_NAME} ${APP_VER}"
OutFile             "VibeDAW_${APP_VER}_Setup.exe"
InstallDir          "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey    HKCU "${REG_KEY}" "InstallDir"
RequestExecutionLevel admin
SetCompressor       /SOLID lzma
Unicode             True

; ── Modern UI ────────────────────────────────────────────────────────────────
!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON    "..\Assets\Icons\vibedam.ico"  ; supply your icon here

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE     "..\LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
Page custom pg_SampleSelect pg_SampleSelectLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Sample selection state ────────────────────────────────────────────────────
Var dlStrings
Var dlBrass
Var dlWoodwinds
Var dlKeys
Var dlPercussion

; ── Sample selection page ─────────────────────────────────────────────────────
Function pg_SampleSelect
    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 16u "Select sample packs to download (internet connection required):"
    Pop $0

    ${NSD_CreateCheckbox} 10 22u 90% 12u "Strings"
    Pop $dlStrings
    ${NSD_SetState} $dlStrings ${BST_CHECKED}

    ${NSD_CreateCheckbox} 10 36u 90% 12u "Brass"
    Pop $dlBrass
    ${NSD_SetState} $dlBrass ${BST_CHECKED}

    ${NSD_CreateCheckbox} 10 50u 90% 12u "Woodwinds"
    Pop $dlWoodwinds
    ${NSD_SetState} $dlWoodwinds ${BST_CHECKED}

    ${NSD_CreateCheckbox} 10 64u 90% 12u "Keys"
    Pop $dlKeys
    ${NSD_SetState} $dlKeys ${BST_CHECKED}

    ${NSD_CreateCheckbox} 10 78u 90% 12u "Percussion"
    Pop $dlPercussion
    ${NSD_SetState} $dlPercussion ${BST_CHECKED}

    ${NSD_CreateLabel} 0 96u 100% 28u \
        "Unchecked packs will not be downloaded. You can re-run this installer at any time to download additional packs."
    Pop $0

    nsDialogs::Show
FunctionEnd

Function pg_SampleSelectLeave
    ${NSD_GetState} $dlStrings   $dlStrings
    ${NSD_GetState} $dlBrass     $dlBrass
    ${NSD_GetState} $dlWoodwinds $dlWoodwinds
    ${NSD_GetState} $dlKeys      $dlKeys
    ${NSD_GetState} $dlPercussion $dlPercussion
FunctionEnd

; ── Macro: download + extract a sample ZIP ───────────────────────────────────
; Usage: !insertmacro DownloadSamples "Category" "URL" $CheckboxState
!macro DownloadSamples Category Url State
    ${If} ${State} == ${BST_CHECKED}
        DetailPrint "Downloading ${Category} samples..."
        inetc::get /CAPTION "Downloading ${Category}..." \
                   /RESUME "" \
                   "${Url}" \
                   "$TEMP\VibeDAW_${Category}.zip" \
                   /END
        Pop $R0
        ${If} $R0 == "OK"
            DetailPrint "Extracting ${Category}..."
            nsisunz::Unzip "$TEMP\VibeDAW_${Category}.zip" \
                           "$APPDATA\VibeDAW\CoreLibrary\${Category}"
            Delete "$TEMP\VibeDAW_${Category}.zip"
        ${Else}
            MessageBox MB_OK|MB_ICONEXCLAMATION \
                "Download failed for ${Category} (${R0}).$\nYou can re-run the installer later to retry."
        ${EndIf}
    ${EndIf}
!macroend

; ── Detect previous install ───────────────────────────────────────────────────
Function .onInit
    ReadRegStr $R0 HKCU "${REG_KEY}" "InstallDir"
    ${If} $R0 != ""
        StrCpy $INSTDIR $R0
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "VibeDAW is already installed at $R0.$\n$\nWould you like to re-download sample packs?" \
            IDYES +2
        Goto skip_redownload
        ; User chose Yes — continue to sample page
        Return
        skip_redownload:
        ; Skip sample downloads by unchecking all
        StrCpy $dlStrings   ${BST_UNCHECKED}
        StrCpy $dlBrass     ${BST_UNCHECKED}
        StrCpy $dlWoodwinds ${BST_UNCHECKED}
        StrCpy $dlKeys      ${BST_UNCHECKED}
        StrCpy $dlPercussion ${BST_UNCHECKED}
    ${EndIf}
FunctionEnd

; ── Main install section ──────────────────────────────────────────────────────
Section "VibeDAW" SEC_MAIN
    SectionIn RO  ; required, can't uncheck

    SetOutPath "$INSTDIR"

    ; ── Application files ─────────────────────────────────────────────────────
    File "..\build\VibesynthStandalone_artefacts\Release\${APP_EXE}"

    ; ── Create project folders in AppData ────────────────────────────────────
    CreateDirectory "$APPDATA\VibeDAW\Presets"
    CreateDirectory "$APPDATA\VibeDAW\Templates"
    CreateDirectory "$APPDATA\VibeDAW\Samples"
    CreateDirectory "$APPDATA\VibeDAW\Saved Projects"
    CreateDirectory "$APPDATA\VibeDAW\CoreLibrary\Strings"
    CreateDirectory "$APPDATA\VibeDAW\CoreLibrary\Brass"
    CreateDirectory "$APPDATA\VibeDAW\CoreLibrary\Woodwinds"
    CreateDirectory "$APPDATA\VibeDAW\CoreLibrary\Keys"
    CreateDirectory "$APPDATA\VibeDAW\CoreLibrary\Percussion"

    ; ── Download selected sample packs ───────────────────────────────────────
    !insertmacro DownloadSamples "Strings"   "${URL_STRINGS}"    $dlStrings
    !insertmacro DownloadSamples "Brass"     "${URL_BRASS}"      $dlBrass
    !insertmacro DownloadSamples "Woodwinds" "${URL_WOODWINDS}"  $dlWoodwinds
    !insertmacro DownloadSamples "Keys"      "${URL_KEYS}"       $dlKeys
    !insertmacro DownloadSamples "Percussion" "${URL_PERCUSSION}" $dlPercussion

    ; ── Start menu shortcut ───────────────────────────────────────────────────
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" \
                    "$INSTDIR\${APP_EXE}"
    CreateShortcut  "$SMPROGRAMS\${APP_NAME}\${UNINSTALLER}.lnk" \
                    "$INSTDIR\${UNINSTALLER}"

    ; ── Desktop shortcut ─────────────────────────────────────────────────────
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

    ; ── Registry entries ──────────────────────────────────────────────────────
    WriteRegStr HKCU "${REG_KEY}" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "${REG_KEY}" "Version"    "${APP_VER}"

    ; Add/Remove Programs entry
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                "DisplayName"     "${APP_NAME} ${APP_VER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                "UninstallString" "$INSTDIR\${UNINSTALLER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                "Publisher"       "KnowledgeBase Studios"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                "DisplayVersion"  "${APP_VER}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                "NoRepair" 1

    ; ── Write uninstaller ─────────────────────────────────────────────────────
    WriteUninstaller "$INSTDIR\${UNINSTALLER}"

SectionEnd

; ── Uninstall section ─────────────────────────────────────────────────────────
Section "Uninstall"

    Delete "$INSTDIR\${APP_EXE}"
    Delete "$INSTDIR\${UNINSTALLER}"
    RMDir  "$INSTDIR"

    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\${UNINSTALLER}.lnk"
    RMDir  "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"

    DeleteRegKey HKCU "${REG_KEY}"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

    ; NOTE: does NOT delete $APPDATA\VibeDAW (preserves user projects and samples)
    MessageBox MB_OK "VibeDAW has been uninstalled.$\nYour projects and samples at $APPDATA\VibeDAW were kept."

SectionEnd
