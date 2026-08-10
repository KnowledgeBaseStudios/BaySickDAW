; =============================================================================
; BaySickDAW TESTER INSTALLER
;
; THIS IS NOT THE SHIPPING INSTALLER.  It exists so testers can be handed one
; file instead of a build tree, and nothing more.  The real release installer
; is owned by the QA-Installer batch, which cannot run until QA-Manuals,
; QA-Templates and QA-LegalReview have run - none of them have.  So this
; package carries no manual, no license or EULA page, no legal review of the
; third-party notices, and no code signature.  Do not ship it, do not put it
; on a download page, and do not treat anything in here as a decision about
; what the final installer will do.
;
; Build it with make_installer.bat at the repo root.  That script finds NSIS,
; checks every input file is really on disk, and reads the version out of
; CMakeLists.txt so this package can never claim a version the app does not.
;
; WHAT IT INSTALLS
;   Into the install folder (per user, no administrator rights):
;     BaySickDAW.exe
;     BaySickPluginHost64.exe, BaySickPluginHost32.exe - the plugin sandbox
;       helpers.  Without them bridged and 32-bit plugins do not load at all.
;     Resources\ - cassette IRs, hiss beds and acoustic IRs, which the effect
;       engines load at run time from <exe folder>\Resources.  Without it
;       those effects silently fall back to doing nothing.
;   Into Documents\BaySickDAW:
;     Presets\   - the factory preset library
;     Templates\ - the factory project templates
;   Nothing installed those two before this script existed; the app only ever
;   read them from a folder someone had to populate by hand.
;
; WHAT IT DELIBERATELY DOES NOT CARRY
;   The sound library, roughly 4 GB.  The app fetches that itself on first
;   launch and from Options > Get Sound Content (Source\CoreLibraryInstaller.h).
;   Putting it here would quadruple the package and duplicate a downloader
;   that already resumes, verifies and installs pack by pack.
;
; USER DATA IS NEVER TOUCHED
;   The package contains factory content only: every "My Presets" and
;   "My Templates" folder is excluded at build time, so a tester's own saves
;   cannot be overwritten by an install.  The uninstaller removes only the
;   files this installer put in the install folder, and never goes near
;   Documents\BaySickDAW - projects, presets, templates and the downloaded
;   sound library all survive an uninstall, so a reinstall does not cost
;   another 4 GB download.
; =============================================================================

Unicode true
SetCompressor /SOLID lzma

; Per-user by ruling: no elevation prompt, nothing in Program Files.
RequestExecutionLevel user

!include "MUI2.nsh"
!include "FileFunc.nsh"
!insertmacro GetSize
!insertmacro DirState

; -----------------------------------------------------------------------------
; Identity.  APP_VER is normally supplied by make_installer.bat, which reads it
; from the project() line in CMakeLists.txt so the two cannot drift.  The
; fallback below only applies when makensis is driven by hand.
; -----------------------------------------------------------------------------
!ifndef APP_VER
  !define APP_VER "1.2.0"
!endif

!define APP_NAME      "BaySickDAW"
!define APP_PUBLISHER "KnowledgeBase Studios"
!define APP_EXE       "BaySickDAW.exe"
!define HELPER64_EXE  "BaySickPluginHost64.exe"
!define HELPER32_EXE  "BaySickPluginHost32.exe"
!define UNINST_NAME   "Uninstall BaySickDAW"
!define UNINST_EXE    "${UNINST_NAME}.exe"

!define REG_APP    "Software\KnowledgeBase Studios\BaySickDAW"
!define REG_UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\BaySickDAW"

; -----------------------------------------------------------------------------
; Where the inputs come from.  Derived from this script's own location, the way
; do_build.bat derives the repo root from its own, so a checkout at any path
; builds without editing this file.  make_installer.bat overrides it anyway.
; -----------------------------------------------------------------------------
!ifndef REPO_ROOT
  !define REPO_ROOT "${__FILEDIR__}\.."
!endif

!define RELEASE_DIR "${REPO_ROOT}\build\BaySickDAWStandalone_artefacts\Release"

; -----------------------------------------------------------------------------
; FAIL LOUDLY AT BUILD TIME.  A missing input would otherwise produce a package
; that installs and then fails at run time in a way a tester cannot diagnose:
; no helper means plugins quietly refuse to load, no Resources means the tape
; and acoustic effects quietly do nothing.  Better to refuse to build.
; The directory checks use a wildcard, which is false for an EMPTY folder too.
; -----------------------------------------------------------------------------
!macro RequireInput _PATH _WHAT
  !if /FileExists "${_PATH}"
  !else
    !error "BaySickDAW installer cannot be built: ${_WHAT} is missing. Expected it at ${_PATH} - run do_build.bat first, then build the installer again."
  !endif
!macroend

!insertmacro RequireInput "${RELEASE_DIR}\${APP_EXE}"      "the Release build of BaySickDAW.exe"
!insertmacro RequireInput "${RELEASE_DIR}\${HELPER64_EXE}" "the 64-bit plugin host helper"
!insertmacro RequireInput "${RELEASE_DIR}\${HELPER32_EXE}" "the 32-bit plugin host helper"
!insertmacro RequireInput "${RELEASE_DIR}\Resources\*.*"   "the Resources folder next to the Release exe"
!insertmacro RequireInput "${REPO_ROOT}\Presets\*.*"       "the Presets folder at the repository root"
!insertmacro RequireInput "${REPO_ROOT}\Templates\*.*"     "the Templates folder at the repository root"

; -----------------------------------------------------------------------------
; Package metadata
; -----------------------------------------------------------------------------
Name    "${APP_NAME} ${APP_VER} (tester build)"
Caption "${APP_NAME} ${APP_VER} tester install"
BrandingText "${APP_PUBLISHER} - tester build, not for release"

!ifndef OUT_FILE
  !define OUT_FILE "${REPO_ROOT}\Installer\BaySickDAW-${APP_VER}-Tester-Setup.exe"
!endif
OutFile "${OUT_FILE}"

; Per-user location by ruling.  $LOCALAPPDATA\Programs is where per-user
; Windows apps go; it needs no elevation and is not roamed.
InstallDir "$LOCALAPPDATA\Programs\${APP_NAME}"
InstallDirRegKey HKCU "${REG_APP}" "InstallDir"

; Windows only accepts a strictly four-part version here.
VIProductVersion "${APP_VER}.0"
VIAddVersionKey "ProductName"     "${APP_NAME}"
VIAddVersionKey "CompanyName"     "${APP_PUBLISHER}"
VIAddVersionKey "FileDescription" "${APP_NAME} tester installer - internal testing only, not a release"
VIAddVersionKey "FileVersion"     "${APP_VER}.0"
VIAddVersionKey "ProductVersion"  "${APP_VER}.0"
; A statement of authorship, not a reviewed legal notice.  The wording that
; ships is QA-LegalReview's call; this only exists because Windows warns about
; a version block with no copyright key at all, and a spurious warning on every
; build is how a real one gets missed.
VIAddVersionKey "LegalCopyright"  "Copyright KnowledgeBase Studios"

; An icon is used when someone drops one here; there is no .ico in the tree
; today, and the stock NSIS icon is fine for a tester package.
!if /FileExists "${__FILEDIR__}\BaySickDAW.ico"
  !define MUI_ICON   "${__FILEDIR__}\BaySickDAW.ico"
  !define MUI_UNICON "${__FILEDIR__}\BaySickDAW.ico"
!endif

; -----------------------------------------------------------------------------
; Pages
; -----------------------------------------------------------------------------
!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TITLE "${APP_NAME} ${APP_VER} - tester build"
!define MUI_WELCOMEPAGE_TEXT \
"This is a TEST build of ${APP_NAME}. It is not the finished product and it is not the release installer.$\r$\n\
$\r$\n\
It installs for you only, so Windows will not ask for an administrator password and nothing is written to Program Files.$\r$\n\
$\r$\n\
This installer is not code signed. Windows will most likely show a blue box saying $\"Windows protected your PC$\". Click $\"More info$\", then $\"Run anyway$\". That warning means the file is not signed yet, not that anything is wrong with it.$\r$\n\
$\r$\n\
The sound library is about 4 GB and is NOT in this installer. ${APP_NAME} downloads it itself the first time you start it, and you can start or finish that download at any time from Options > Get Sound Content.$\r$\n\
$\r$\n\
Uninstalling later leaves your projects, presets, templates and the downloaded sounds exactly where they are."

!define MUI_DIRECTORYPAGE_TEXT_TOP \
"${APP_NAME} will be installed in the folder below. It is inside your own user folder, which is why no administrator password is needed.$\r$\n$\r$\n\
Your projects, presets and templates do NOT go here - they live in your Documents folder, under Documents\${APP_NAME}."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_TITLE "${APP_NAME} is installed"
!define MUI_FINISHPAGE_TEXT \
"${APP_NAME} ${APP_VER} has been installed.$\r$\n$\r$\n\
The first time you start it, it will offer to download the sound library, about 4 GB. Until that finishes, the sampled instruments - drums, guitars, basses, keys, strings, brass and woodwinds - have nothing to play and will be silent. Everything else works normally. The download can be stopped and picked up later.$\r$\n$\r$\n\
Remember this is a test build. Report anything odd."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${APP_NAME} now"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_FINISH

!define MUI_UNCONFIRMPAGE_TEXT_TOP \
"This removes the ${APP_NAME} program files and its shortcuts.$\r$\n$\r$\n\
It does NOT touch anything in your Documents\${APP_NAME} folder. Your projects, your presets, your templates and the sound library you downloaded are all left alone, so reinstalling does not cost you another 4 GB download."

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; -----------------------------------------------------------------------------
; Install
; -----------------------------------------------------------------------------
Function .onInit
  ; Explicit even though it is the default: every path this installer writes to
  ; is a per-user one, and the uninstaller sets the same thing.
  SetShellVarContext current
FunctionEnd

; REFUSE A POPULATED FOLDER THAT IS NOT ALREADY A BaySickDAW INSTALL.
; This is what makes the uninstaller's one recursive delete safe.  That delete
; is bounded by the NAME "$INSTDIR\Resources", not by a record of what was
; written, so pointing the install at a folder that already held a Resources
; subtree would arm the uninstaller to take it.  Blocking the input is a far
; better guarantee than trying to make the delete clever.
; DirState: 0 = empty, 1 = has content, -1 = does not exist.  Empty folders and
; folders that do not exist yet are fine, and reinstalling over an existing
; BaySickDAW install is fine; anything else is refused.
Function .onVerifyInstDir
  IfFileExists "$INSTDIR\${APP_EXE}" ok
  ${DirState} "$INSTDIR" $R0
  StrCmp $R0 "1" 0 ok
    Abort
  ok:
FunctionEnd

Section "${APP_NAME} (required)" SEC_APP

  SectionIn RO

  ; A running copy holds its own exe open, and the overwrite below would fail
  ; part way through, leaving a half-installed folder.  Catch it before the
  ; first byte is written and let the tester close the app and retry.
  check_running:
  IfFileExists "$INSTDIR\${APP_EXE}" 0 not_running
    ClearErrors
    FileOpen $R9 "$INSTDIR\${APP_EXE}" a
    IfErrors 0 unlocked
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
        "${APP_NAME} looks like it is still running, or another program is holding its file open.$\r$\n$\r$\nClose ${APP_NAME} and click Retry." \
        IDRETRY check_running
      Abort "Install stopped: ${APP_NAME} is still running."
    unlocked:
    FileClose $R9
  not_running:

  SetOutPath "$INSTDIR"
  SetOverwrite on

  DetailPrint "Installing the application..."
  File "${RELEASE_DIR}\${APP_EXE}"

  ; The plugin sandbox helpers.  The app locates them by filename in its own
  ; folder, so they are not optional and are not renameable.
  File "${RELEASE_DIR}\${HELPER64_EXE}"
  File "${RELEASE_DIR}\${HELPER32_EXE}"

  ; NSIS TRAP, and it is a silent one: "File /r <dir>" does NOT mean "that one
  ; directory".  It searches the source's PARENT recursively for every directory
  ; whose name matches, and packages all of them, rebuilding their relative
  ; paths under $OUTDIR.  Written the obvious way, "File /r ...\Presets" swept
  ; in "Files For Claude\...\Presets" from elsewhere in the repository, and
  ; "File /r ...\Templates" swept in the Projucer's template folder out of the
  ; vendored JUCE tree.  Naming the CONTENTS with a trailing \* makes the named
  ; directory the search root, which is what was meant.  Keep the \*.
  DetailPrint "Installing audio resources..."
  SetOutPath "$INSTDIR\Resources"
  File /r /x "Thumbs.db" /x "desktop.ini" "${RELEASE_DIR}\Resources\*"

  ; ---------------------------------------------------------------------------
  ; Factory content into the user data root.  Documents\BaySickDAW is what
  ; Source\AppPaths.h resolves to, and every preset and template reader in the
  ; app goes through it.
  ;
  ; The /x exclusions are the whole safety story here: "My Presets" and
  ; "My Templates" are where the app saves a user's own patches and templates,
  ; so excluding them means this package physically cannot contain, and
  ; therefore cannot overwrite, anything a tester made.  Everything that does
  ; get written is factory content being refreshed to match this build.
  ; ---------------------------------------------------------------------------
  DetailPrint "Installing factory presets and templates..."

  SetOutPath "$DOCUMENTS\${APP_NAME}\Presets"
  ; factory_seed_version.txt is app-GENERATED state, not factory content: it is
  ; the stamp EffectPresetIO compares against kFactorySeedVersion to decide
  ; whether to rebuild the factory effect presets.  Shipping it pre-satisfied
  ; would tell a fresh install the reseed had already happened.
  File /r /x "My Presets" /x "factory_seed_version.txt" /x "Thumbs.db" /x "desktop.ini" "${REPO_ROOT}\Presets\*"

  SetOutPath "$DOCUMENTS\${APP_NAME}\Templates"
  File /r /x "My Templates" /x "Thumbs.db" /x "desktop.ini" "${REPO_ROOT}\Templates\*"

  ; Back to the install folder so the shortcuts below get it as their
  ; start-in directory.
  SetOutPath "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"   "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${UNINST_NAME}.lnk" "$INSTDIR\${UNINST_EXE}"

  WriteRegStr HKCU "${REG_APP}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${REG_APP}" "Version"    "${APP_VER}"

  ; Add/Remove Programs.  HKCU, not HKLM: an HKLM write needs elevation and
  ; this installer deliberately never asks for any.
  WriteRegStr   HKCU "${REG_UNINST}" "DisplayName"      "${APP_NAME} ${APP_VER} (tester build)"
  WriteRegStr   HKCU "${REG_UNINST}" "DisplayVersion"   "${APP_VER}"
  WriteRegStr   HKCU "${REG_UNINST}" "Publisher"        "${APP_PUBLISHER}"
  WriteRegStr   HKCU "${REG_UNINST}" "InstallLocation"  "$INSTDIR"
  WriteRegStr   HKCU "${REG_UNINST}" "DisplayIcon"      "$INSTDIR\${APP_EXE}"
  WriteRegStr   HKCU "${REG_UNINST}" "UninstallString"      "$\"$INSTDIR\${UNINST_EXE}$\""
  WriteRegStr   HKCU "${REG_UNINST}" "QuietUninstallString" "$\"$INSTDIR\${UNINST_EXE}$\" /S"
  WriteRegDWORD HKCU "${REG_UNINST}" "NoModify" 1
  WriteRegDWORD HKCU "${REG_UNINST}" "NoRepair" 1

  ; Measured after the files are in place rather than guessed, so the size
  ; shown in Add/Remove Programs is the real one.
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${REG_UNINST}" "EstimatedSize" "$0"

  WriteUninstaller "$INSTDIR\${UNINST_EXE}"

SectionEnd

Section "Desktop shortcut" SEC_DESKTOP

  SetOutPath "$INSTDIR"
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0

  ; Recorded so the uninstaller only deletes a desktop shortcut it created.
  WriteRegDWORD HKCU "${REG_UNINST}" "DesktopShortcut" 1

SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_APP} \
    "The application, the two plugin host helpers, the audio resources it loads at run time, and the factory presets and templates."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} \
    "Put a ${APP_NAME} shortcut on your desktop. There is a Start Menu shortcut either way."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; -----------------------------------------------------------------------------
; Uninstall
;
; The rule this section is built around: remove what the installer put in the
; install folder, and NOTHING else.  There is deliberately no
; RMDir /r "$INSTDIR" anywhere below - $INSTDIR is attacker-and-accident
; controlled (a /D switch, a hand-edited registry value, a tester who pointed
; the directory page at their Documents folder), and a recursive delete of it
; would take a user's work with it.  Named files only, one bounded subtree, and
; a plain RMDir that removes the folder only when it is already empty.
;
; The one recursive delete below is bounded by the NAME "$INSTDIR\Resources",
; not by a record of what this installer actually wrote, so on its own it would
; take a pre-existing Resources subtree in a folder the user chose.  What makes
; it safe is the INSTALL-TIME guard: .onVerifyInstDir refuses any populated
; folder that is not already a BaySickDAW install, so $INSTDIR can only ever be
; empty, new, or ours.
; -----------------------------------------------------------------------------
Function un.onInit
  SetShellVarContext current
FunctionEnd

Section "Uninstall"

  StrCmp $INSTDIR "" bad_dir
  IfFileExists "$INSTDIR\${APP_EXE}"    dir_ok
  IfFileExists "$INSTDIR\${UNINST_EXE}" dir_ok

  bad_dir:
    MessageBox MB_OK|MB_ICONSTOP \
      "This does not look like a ${APP_NAME} install folder, so nothing has been deleted.$\r$\n$\r$\nFolder: $INSTDIR"
    Abort "Nothing was removed."

  dir_ok:

  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\${HELPER64_EXE}"
  Delete "$INSTDIR\${HELPER32_EXE}"

  ; Bounded: this subtree is created by this installer and holds nothing else.
  RMDir /r "$INSTDIR\Resources"

  Delete "$INSTDIR\${UNINST_EXE}"

  ; No /r: anything a tester dropped in the install folder keeps the folder
  ; alive, and that is the correct outcome.
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\${UNINST_NAME}.lnk"
  RMDir  "$SMPROGRAMS\${APP_NAME}"

  ReadRegDWORD $0 HKCU "${REG_UNINST}" "DesktopShortcut"
  StrCmp $0 "1" 0 no_desktop_icon
    Delete "$DESKTOP\${APP_NAME}.lnk"
  no_desktop_icon:

  DeleteRegKey HKCU "${REG_UNINST}"
  DeleteRegKey HKCU "${REG_APP}"
  DeleteRegKey /ifempty HKCU "Software\KnowledgeBase Studios"

  ; Documents\BaySickDAW is NOT touched.  Projects, presets, templates and the
  ; downloaded sound library are user data and survive an uninstall by ruling.

  IfSilent done
  MessageBox MB_OK|MB_ICONINFORMATION \
    "${APP_NAME} has been removed.$\r$\n$\r$\nYour projects, presets, templates and the sound library you downloaded were left exactly as they were, in:$\r$\n$DOCUMENTS\${APP_NAME}"
  done:

SectionEnd
