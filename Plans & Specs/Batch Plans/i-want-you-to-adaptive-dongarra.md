# QA-0a Implementation Plan — Debug Build Workflow Setup

> **For agentic workers:** This plan is executed inline. Steps use checkbox
> `- [ ]` syntax. Read the three companion docs at
> `C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\*.md` BEFORE
> touching code.
>
> **Fork notice:** this batch was inserted ahead of QA-0 by user request on
> 2026-05-07. The QA-0 plan was authored earlier in this session and lives
> in chat history; Task 9 below re-creates it on disk as its own plan file
> after QA-0a closes.

**Goal:** Stand up a Debug build of BaySickDAW alongside the existing
Release build, with visual differentiation on the taskbar so the user
can pin both. Establish "Debug for verifying Claude fixes, Release for
music production" as the standing workflow. Triage any pre-existing
JUCE asserts that fire on cold-start so the Debug exe is usable.

**Architecture:** `do_build.bat` invokes `cmake --build` twice — once
each for `--config Release` and `--config Debug`. `CMakeLists.txt`
gates the embedded exe icon (`ICON_BIG`) on `CMAKE_BUILD_TYPE` so
Debug ships without a custom icon (Windows generic .exe icon on
taskbar). `Source/Standalone/StandaloneApp.cpp` appends `" [DEBUG]"`
to the window title behind `#ifdef JUCE_DEBUG`. CLAUDE.md gets a
short workflow note. New plan documents the workflow change in the
main plan as a fork.

**Tech stack:** Bat scripting, JUCE 7 CMake, MSVC /MD. No new C++
features.

---

## Context

### Why this batch exists

During the QA-0 plan-mode session on 2026-05-07, the user asked
whether the dispatcher's most-recent-wins fallback should be tightened
to `jassertfalse`. Investigation revealed:

- `jassertfalse` is compiled out of Release builds (no-op in shipping).
- `do_build.bat` only builds Release.
- Therefore tightening the assert in the QA-0 dispatcher plan would
  add zero value to the user's actual workflow — the tripwire would
  never fire.

The user's workflow is: solo developer, no coding background, relies on
Claude for all source work. Today's bug-finding loop is bottlenecked by
"describe what you see in plain English" because Release builds offer
no precise error reporting. A Debug build short-circuits that loop:
when a JUCE-internal or user-added `jassert` fires, Windows pops up a
dialog with the exact file path, line number, and failing condition.
The user can screenshot/paste it; round-trips collapse from many to
one.

### Scope

This batch is purely workflow infrastructure:
1. Build script produces Debug exe alongside Release.
2. Visual differentiation so taskbar pinning works.
3. Cold-start triage of any existing asserts that fire on a clean
   default session.
4. Workflow note in CLAUDE.md so future Claude sessions default to
   "verify in Debug first, then Release."
5. Main-plan fork annotation so the deviation from the original
   sequence is recorded.

This batch does NOT:
- Touch any audio code.
- Fix DSP-12 (that's QA-0, comes after).
- Run a deep audit of every assert in the codebase (that's part of
  Phase 6 audit + cleanup work — see main plan §5 Phase 6).

### Why before QA-0

The user explicitly chose `jassertfalse` for QA-0's dispatcher
fallback. Without QA-0a landing first, that `jassertfalse` would be
dormant in the user's workflow. Reversing the order means QA-0's
tripwire is genuinely useful from the moment QA-0 lands.

### Decisions already made (this session)

| Topic | Decision |
|-------|----------|
| Debug icon | **No icon for Debug exe** — skip `ICON_BIG` when CMAKE_BUILD_TYPE=Debug. Windows shows the generic .exe icon on taskbar / file explorer. Zero asset work. |
| Window title differentiation | Append `" [DEBUG]"` to the window title via `#ifdef JUCE_DEBUG`. |
| Triage scope | Time-boxed: cold-start default session only. Asserts that fire there get triaged inline (fix or HOLD-FOR comment). Larger triage deferred to Phase 6 audit. |
| Performance caveats | User accepts that Debug builds run slower and may glitch under heavy session load even when Release runs fine. Standing rule: re-test in Release before reporting any perf/audio issue noticed only in Debug. |
| Workflow rule | Debug for verifying Claude fixes; Release for music production. Standing rule documented in CLAUDE.md. |

---

## File structure

| Action | File | Responsibility |
|--------|------|---------------|
| Modify | `..\Main Plan.md` | Append a "Forks" section noting the QA-0a insertion + reason. |
| Modify | `C:\Users\jeffm\Documents\BaySickDAW\do_build.bat` | Build both Release and Debug configs; write build log. |
| Modify | `C:\Users\jeffm\Documents\BaySickDAW\CMakeLists.txt` | Gate `ICON_BIG` on `CMAKE_BUILD_TYPE`; only set for Release. |
| Modify | `C:\Users\jeffm\Documents\BaySickDAW\Source\Standalone\StandaloneApp.cpp` | Append `" [DEBUG]"` to window title in Debug builds. |
| Modify | `C:\Users\jeffm\Documents\BaySickDAW\CLAUDE.md` | Add Debug build workflow note under "Build System" section. |
| Modify (post) | `..\Implemented Work Log.md` | Append QA-0a entry. |
| Create (post) | `C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\Batch Plans\<silly-name>.md` | Re-author the QA-0 plan from chat history (Task 9). |

---

## Tasks

### Task 1: Hybrid fork annotation across the main plan

**Files:**
- Modify: `..\Main Plan.md`

The main plan's §0 Rule currently says the plan is "append-only on
scope changes — never overwrite prior content. Decision forks get
appended as new sections." That convention treats forks as a master
log only, which fails the linear-reader mode (read §5/§6 sequentially,
miss the fork in §9). The user agreed on 2026-05-07 to switch to a
**hybrid convention**: short inline back-references at the affected
section(s) + the full narrative in a master fork log. This task
implements that switch AND records the QA-0a fork using the new
convention.

The main plan currently has §0–§8 + "End of plan". The new master
section is §9 (Forks).

**Touchpoints for the QA-0a fork:**
- §0 Rule (convention update — document the new hybrid approach).
- §5 Batching Structure — new QA-0a inline entry before QA-0 in
  Phase 1.
- §6 Sequencing — arrow updated to include QA-0a; footnote points
  to §9.
- §7 Verification Approach — one-line note "verification now starts
  with Debug exe per QA-0a; see §9".
- §9 Forks (NEW SECTION) — master log entry with full narrative.

- [ ] **Step 1.1: Update §0 Rule to document the hybrid convention**

Find the §0 row that describes the main plan's update cadence:

```markdown
| **Plan** | `../Main Plan.md` (this file) | Master sequencing + per-batch scope + dependencies. The "what to do, in what order". | **Append-only on scope changes.** Never overwrite prior content — decision forks get appended as new sections showing what changed and why. Preserves the decision history so future-us can see how the plan evolved. |
```

Replace the entire fourth column with:

```markdown
**Hybrid: inline back-refs + master fork log.** When work scope
changes, edit the affected section(s) inline with a one-line back-ref
to the master fork log (e.g. "QA-0a fork — see §9"), AND append a
full narrative entry to §9 Forks. Inline annotations preserve the
linear-read mode (sections describe their actual content); the master
log preserves the chronological "what's changed since plan write?"
overview. Original content is never deleted — inline back-refs add a
line, master log appends entries. Convention adopted 2026-05-07 (see
§9 first entry).
```

- [ ] **Step 1.2: Insert QA-0a entry in §5 Batching Structure**

Locate `### Phase 1 — Critical regression fix + fast wins` in §5,
which currently opens with `#### **QA-0: MT Composite RenderTask
(DSP-12 restore)**  ⚠️ TOP PRIORITY`.

Insert a new batch entry **immediately above** the QA-0 entry:

```markdown
#### **QA-0a: Debug Build Workflow Setup**  (forked in 2026-05-07 — see §9)
- Items: workflow infrastructure (no items from the unified backlog).
- Scope: modify `do_build.bat` to build BOTH Release and Debug
  configs; gate the embedded exe icon for Release-only so Debug
  exe shows the generic Windows .exe icon (taskbar pins
  differentiate); append " [DEBUG]" to the window title in Debug
  builds; cold-start triage of existing `jassert` calls; document
  the new workflow in CLAUDE.md.
- Risk: low. Build infrastructure, no audio code.
- Dependencies: none. Runs first in Phase 1.
- Effort: small-medium (~2 hours). Triage is the variable.
- Why before QA-0: QA-0 ships a `jassertfalse` tripwire on the
  dispatcher's most-recent-wins fallback. Without QA-0a's Debug
  build, that tripwire is compiled out of Release and never fires
  in user workflow. QA-0a makes the tripwire actually useful.
- Plan file: `i-want-you-to-adaptive-dongarra.md`.
```

The unicode emoji in the existing QA-0 line (`⚠️`) is pre-existing
ASCII-violation that we're NOT touching in this batch. Leave it.

- [ ] **Step 1.3: Update §6 Sequencing arrow + add footnote**

Locate the bug-fix arrow in §6 Sequencing:

```markdown
**Bug-fix phases (1-5):**
\`\`\`
QA-0  → QA-A → QA-B → QA-C → QA-D → QA-E → QA-F → QA-Fa → QA-G → QA-H
                                              → QA-I → QA-J → QA-K → QA-L → QA-M → QA-N
\`\`\`
```

Replace with:

```markdown
**Bug-fix phases (1-5):**
\`\`\`
QA-0a* → QA-0  → QA-A → QA-B → QA-C → QA-D → QA-E → QA-F → QA-Fa → QA-G → QA-H
                                                       → QA-I → QA-J → QA-K → QA-L → QA-M → QA-N
\`\`\`

\\* QA-0a inserted 2026-05-07 ahead of QA-0 — Debug build workflow
setup so QA-0's dispatcher tripwire is useful in the user's
shipping-binary workflow. See §9 first entry.
```

(Use the asterisk-then-text footnote style; the asterisk in the
arrow flags the inserted batch visually.)

- [ ] **Step 1.4: Add a note to §7 Verification Approach**

§7's first paragraph reads "Per-batch verification (every batch must
pass before commit):" followed by a numbered list of verification
steps. Insert a one-line note **directly above** the numbered list:

```markdown
**Per-batch verification (every batch must pass before commit):**

> **Note (2026-05-07):** post QA-0a, every "build" step in this list
> produces both Release and Debug exes. The standing rule is to
> verify in the Debug exe FIRST (any `jassert` fires as a precise
> dialog you can screenshot), then re-run the same checks in Release
> as the actual user-facing test. See §9 first entry + CLAUDE.md
> Build System.

1. \`do_build.bat\` clean.
2. App launches, audio plays at default settings.
... (existing list continues unchanged)
```

- [ ] **Step 1.5: Append §9 Forks (master log) section**

Append (do NOT overwrite anything above) a new section at the very
bottom of `../Main Plan.md`, **after** the
existing `## End of plan` heading and BEFORE any trailing content
(the file currently ends with the End of plan line followed by a
short closing paragraph; place §9 after the closing paragraph):

```markdown
---

## 9. Forks

Chronological log of scope/sequencing changes since plan write
(2026-05-07). Entries are append-only. Each entry pairs with inline
back-references in the affected sections (per §0 Rule's hybrid
convention).

### 2026-05-07 — QA-0a inserted before QA-0 (Debug build workflow setup)

**Trigger:** during the QA-0 plan-mode session, the user decided the
dispatcher's most-recent-wins fallback should be tightened to
`jassertfalse`. Investigation surfaced that `jassertfalse` is
compiled out of Release builds and `do_build.bat` only builds
Release, so the tripwire would never fire in user workflow.

**Deeper finding:** the user is a solo developer with no coding
background. Today's diagnostic loop is bottlenecked by describing
behavior in plain English (no precise error messages from Release).
A Debug build alongside Release short-circuits that loop — when a
`jassert` fires, Windows pops up a precise file:line dialog the user
can screenshot. Round-trips collapse from many to one.

**Decision:** insert QA-0a (Debug build workflow setup) BEFORE QA-0.
QA-0a's dispatcher tripwire (in QA-0) becomes genuinely useful from
the moment QA-0 lands.

**QA-0a scope:** modify `do_build.bat` to build both Release and
Debug; gate the embedded exe icon for Release-only (Debug exe shows
generic Windows .exe icon, so taskbar pins differentiate); append
" [DEBUG]" to the window title in Debug builds; cold-start triage of
existing `jassert` calls that fire on a clean default session;
document the new workflow in CLAUDE.md.

**Inline back-refs:**
- §0 Rule updated to document the hybrid annotation convention.
- §5 Phase 1 has new QA-0a entry above QA-0.
- §6 Sequencing arrow updated.
- §7 Verification gained a Debug-first note.

**Plan files:**
- QA-0a: `i-want-you-to-adaptive-dongarra.md`
- QA-0:  re-authored at QA-0a Task 9 as a separate silly-name file
  (filename TBD; main plan §9 entry updated when chosen).

**Verification:** QA-0a closes when the dual-config build produces
both exes, taskbar differentiation works, cold-start triage is done,
and CLAUDE.md reflects the new workflow.
```

- [ ] **Step 1.6: Verify main plan structure**

Open `../Main Plan.md`. Confirm:
- §0 Rule's main-plan row reflects the hybrid convention.
- §5 Phase 1 has QA-0a above QA-0.
- §6 sequencing arrow shows `QA-0a*` first; footnote present.
- §7 has the Debug-first note above the numbered list.
- §9 Forks section appended after End of plan + closing paragraph.
- All other content untouched.

- [ ] **Step 1.7: Commit (main plan hybrid fork annotation)**

```
git status   # confirm only the main plan is modified
git add ../Main Plan.md
git commit -m "QA-0a Step 1: hybrid fork annotation in main plan (§0 rule + §5/6/7 inline + §9 master log)."
```

(If the plan files live outside the BaySickDAW git repo and aren't
tracked there, this commit step is a no-op — confirm with `git status`
in the BaySickDAW directory before attempting. The plan files may
need their own separate git repo or may be left untracked; either way
the file edits land regardless of whether `git commit` succeeds.)

---

### Task 2: Modify do_build.bat to build both configurations

**Files:**
- Modify: `C:\Users\jeffm\Documents\BaySickDAW\do_build.bat`

- [ ] **Step 2.1: Replace do_build.bat content**

Current content (verified at plan-write):

```bat
@echo off
rem Reset PATH to bare minimum so vcvars64.bat can append without hitting the 8191-char limit
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 > nul 2>&1
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\BaySickDAW\build" --target BaySickDAWStandalone --config Release -j4 > "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
```

Replace with:

```bat
@echo off
rem Reset PATH to bare minimum so vcvars64.bat can append without hitting the 8191-char limit
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 > nul 2>&1

rem QA-0a (2026-05-07): build BOTH configs.  Release is the shipping exe and
rem the one used for music production.  Debug ships internal jassert checks
rem -- a failing condition pops a dialog with file:line so regressions are
rem visible to a non-coder solo developer.  Standing workflow: verify Claude
rem fixes in Debug FIRST, then re-run in Release for the actual user test.

echo === Building Release ===                                          > "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\BaySickDAW\build" --target BaySickDAWStandalone --config Release -j4 >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo RELEASE_EXIT_CODE=%ERRORLEVEL%                                   >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"

echo === Building Debug ===                                           >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\jeffm\Documents\BaySickDAW\build" --target BaySickDAWStandalone --config Debug   -j4 >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt" 2>&1
echo DEBUG_EXIT_CODE=%ERRORLEVEL%                                     >> "C:\Users\jeffm\Documents\BaySickDAW\build_log.txt"
```

Key behaviors:
- Both configs share the same build directory; CMake puts artefacts in
  `Release\` and `Debug\` subfolders automatically (multi-config
  generator).
- Release builds first so a Debug compile failure doesn't block the
  shipping exe from being produced.
- Two separate exit-code lines in the log so post-build inspection
  shows which config (or both) failed.
- `>>` (append) used for Debug section so Release output is preserved.

- [ ] **Step 2.2: Run a single build to verify**

Tell Jeff: run `do_build.bat`. Expected output paths after success:
- `build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe`
- `build\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe`

Both files exist; both >0 bytes. Inspect `build_log.txt` and confirm
both `RELEASE_EXIT_CODE=0` and `DEBUG_EXIT_CODE=0`.

If either is non-zero: the bat-script edit landed but a config failed
to compile. Inspect the log for the failure window, share with me;
diagnose and patch before proceeding.

- [ ] **Step 2.3: Commit (build script change)**

```
git add do_build.bat
git commit -m "QA-0a Step 2: do_build.bat builds Release + Debug."
```

---

### Task 3: Gate the embedded exe icon for Release-only

**Files:**
- Modify: `C:\Users\jeffm\Documents\BaySickDAW\CMakeLists.txt:304-308`

- [ ] **Step 3.1: Wrap ICON_BIG in a Release-only conditional**

Current:

```cmake
juce_add_gui_app(BaySickDAWStandalone
    PRODUCT_NAME "BaySickDAW"
    BUNDLE_ID    "com.knowledgebasestudios.baysickdaw"
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Assets/BaySickDAWLogo.png"
)
```

Change to (CMake-conditional icon):

```cmake
# QA-0a (2026-05-07): the embedded exe icon is set ONLY for Release builds.
# Debug exe ships without a custom icon -- Windows shows the generic
# application icon on the taskbar / file explorer / pinned items.  This
# lets the user pin Release + Debug exes side by side and tell them apart
# at a glance (branded BaySickDAW icon = production; Windows default = "I'm
# verifying a fix").  juce_add_gui_app's ICON_BIG arg is single-shot; we
# build the args list conditionally and splat with ${BAYSICK_GUI_APP_ARGS}.
set(BAYSICK_GUI_APP_ARGS
    PRODUCT_NAME "BaySickDAW"
    BUNDLE_ID    "com.knowledgebasestudios.baysickdaw"
)
if(CMAKE_BUILD_TYPE STREQUAL "Release"
   OR CMAKE_CONFIGURATION_TYPES MATCHES "Release")
    list(APPEND BAYSICK_GUI_APP_ARGS
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/BaySickDAWLogo.png")
endif()

juce_add_gui_app(BaySickDAWStandalone ${BAYSICK_GUI_APP_ARGS})
```

**Important caveat:** Visual Studio's CMake generator is *multi-config*
— `CMAKE_BUILD_TYPE` is empty at configure time and only specified per
build invocation via `--config`. The condition above checks BOTH the
single-config var AND the multi-config list as a defensive both-paths
match. Multi-config behavior:

- Configure-time check: `Release` matches multi-config types list;
  ICON_BIG is APPENDED to the args list.
- Per-config build: the resulting target carries the icon resource
  for ALL configs in the multi-config setup.

This is a known JUCE+VS gotcha: `juce_add_gui_app`'s `ICON_BIG` is
applied at target-add time, not per-config. To genuinely strip the
icon from the Debug exe, we need a different approach. **Verify
this caveat at Task 3.3 build-and-inspect time** — if Debug exe
still shows the BaySickDAW icon, fall back to the runtime approach
in Step 3.4 below.

- [ ] **Step 3.2: Run do_build.bat to confirm the CMake change re-configures cleanly**

Tell Jeff: run `do_build.bat`. Expected: configure step re-runs (CMake
detects the CMakeLists change), both configs produce, both exes exist.
If configure errors with a CMake syntax error, the conditional is
malformed.

- [ ] **Step 3.3: Inspect both exes' icons (file explorer)**

Open Windows Explorer. Navigate to:
- `C:\Users\jeffm\Documents\BaySickDAW\build\BaySickDAWStandalone_artefacts\Release\`
- `C:\Users\jeffm\Documents\BaySickDAW\build\BaySickDAWStandalone_artefacts\Debug\`

Compare the two `BaySickDAW.exe` icons:
- Release: BaySickDAW branded icon ✅
- Debug: BaySickDAW branded icon (CMake gotcha hit) OR generic .exe (clean)

If Debug shows generic .exe: proceed to Task 4. ✅
If Debug shows the branded icon: the multi-config CMake gotcha bit us.
Fall through to Step 3.4.

- [ ] **Step 3.4 (FALLBACK if Step 3.3 shows branded icon on Debug)**

If the CMake gotcha caught us, the cleanest fallback is runtime icon
suppression. JUCE doesn't expose a "remove embedded icon at runtime"
API — but we CAN clear the window's titlebar icon and stop returning
an icon image from the application's `getApplicationIcon()` callback
under Debug.

Edit `Source/Standalone/StandaloneApp.cpp`. Locate
`VibesynthStandaloneApp::getApplicationIcon()` (or equivalent). Wrap:

```cpp
const juce::Image VibesynthStandaloneApp::getApplicationIcon()
{
   #if JUCE_DEBUG
    return juce::Image{};   // empty -> Windows default
   #else
    return /* current branded image */;
   #endif
}
```

Then in `VibeSynthWindow` (or the DocumentWindow subclass), pass an
empty Image to setIcon under Debug:

```cpp
   #if JUCE_DEBUG
    setIcon (juce::Image{});
   #else
    setIcon (/* branded image */);
   #endif
```

This handles the running-window icon. The .exe-file-on-disk icon
remains branded, which is acceptable: pinned taskbar items show the
RUNNING window's icon when active. Windows file explorer continues
to show the embedded branded icon — a small caveat.

If this fallback also doesn't differentiate sufficiently, escalate to:
"add a stock placeholder PNG for Debug" and revisit asset sourcing.

- [ ] **Step 3.5: Commit (icon gating)**

```
git add CMakeLists.txt
git add Source/Standalone/StandaloneApp.cpp   # only if Step 3.4 needed
git commit -m "QA-0a Step 3: gate embedded exe icon for Release-only (Debug shows Windows default)."
```

---

### Task 4: Append " [DEBUG]" to window title in Debug builds

**Files:**
- Modify: `C:\Users\jeffm\Documents\BaySickDAW\Source\Standalone\StandaloneApp.cpp` (refreshWindowTitle path)

- [ ] **Step 4.1: Locate the window-title set site**

Grep `Source/Standalone/StandaloneApp.cpp` for `setName` or
`setTitle` or `refreshWindowTitle`. The standalone window's title
is set at startup + refreshed on project open/save.

- [ ] **Step 4.2: Wrap title setter with #ifdef JUCE_DEBUG suffix**

Find the line that sets the window title (e.g.
`setName ("BaySickDAW - " + projectName);` or similar). Modify to
append " [DEBUG]" under Debug:

```cpp
juce::String title = "BaySickDAW";
if (projectName.isNotEmpty())
    title += " - " + projectName;
#if JUCE_DEBUG
title += " [DEBUG]";
#endif
setName (title);
```

(Adjust to match the actual existing title-construction pattern in
the file.)

If `refreshWindowTitle` is the canonical entry point, edit there
instead — single-source-of-truth.

- [ ] **Step 4.3: Build (both configs) and inspect window titles**

Tell Jeff: run `do_build.bat`, then launch BOTH exes:
- `build\...\Release\BaySickDAW.exe` — title bar should read `BaySickDAW` (or `BaySickDAW - <ProjectName>` if a project is open).
- `build\...\Debug\BaySickDAW.exe` — title bar should read `BaySickDAW [DEBUG]` (or `BaySickDAW - <ProjectName> [DEBUG]`).

- [ ] **Step 4.4: Commit (window title differentiation)**

```
git add Source/Standalone/StandaloneApp.cpp
git commit -m "QA-0a Step 4: append [DEBUG] to window title in Debug builds."
```

---

### Task 5: Verify pinning + side-by-side workflow

This task is purely user-driven verification — no code changes.

- [ ] **Step 5.1: Pin both exes to the taskbar**

Tell Jeff:
1. Right-click Release `BaySickDAW.exe` → Pin to taskbar.
2. Right-click Debug `BaySickDAW.exe` → Pin to taskbar.
3. Confirm two distinct taskbar items (different icons OR same-icon
   but different positions if Step 3.4 fallback was used).
4. Hover each — tooltip differentiates by `[DEBUG]` suffix when
   either is running.

- [ ] **Step 5.2: Mental-model check**

Tell Jeff to confirm: "Debug for verifying Claude fixes; Release for
music production." If anything feels off (e.g. Debug runs unbearably
slow on his rig, or pin differentiation is unclear), flag now.

---

### Task 6: Cold-start assertion triage

This is the "first Debug run will probably be noisy" reality check
from the conversation. Time-boxed: focus on ONE cold-start session,
default project, default actions. Larger triage deferred to Phase 6.

- [ ] **Step 6.1: Cold-start the Debug exe with default project**

Tell Jeff: launch the Debug exe (no project loaded; or load whichever
project he typically launches into). Note: any "Assertion Failed!"
dialog that pops up. Format:

```
Assertion failed!
File: <path>
Line: <N>
Expression: <text>
[Abort] [Retry] [Ignore]
```

He should:
- Screenshot the dialog.
- Click [Ignore] to continue past the assert (NOT [Abort] — that
  closes the app).
- Continue using the app normally; note any further dialogs.
- After ~2 minutes of normal use, close the app.

- [ ] **Step 6.2: Catalogue any asserts that fired**

Compile the list:
- File path + line number (one per assert).
- Expression that failed.
- What action (if any) preceded it.

- [ ] **Step 6.3: Triage each assert**

For each, decide:
- **Real bug** → fix inline in QA-0a (small ones) or note for a
  dedicated batch (larger ones).
- **Benign noise (false positive)** → add a one-line comment near
  the assert explaining why it's expected and the runtime behavior is
  fine. Optionally suppress with `// jassert(...)` if it's truly
  unfixable noise.
- **Unclear** → leave for now, document in implemented-work entry,
  revisit in Phase 6 audit.

Triage budget: **30 minutes max**. If 10+ asserts fire and triage
exceeds 30 minutes, document the unfinished list and stop. The Phase
6 audit (QA-Audit) will revisit comprehensively.

- [ ] **Step 6.4: Commit (triage fixes, if any)**

If real bugs were fixed:

```
git add <specific files>
git commit -m "QA-0a Step 6: triage cold-start asserts (<N> fixed, <M> documented)."
```

If only HOLD-FOR comments / no real fixes: same commit, message reads
"...(<M> documented benign)."

If zero asserts fired on cold start: skip the commit; note "clean
cold-start" in implemented-work entry.

---

### Task 7: Document the new workflow in CLAUDE.md

**Files:**
- Modify: `C:\Users\jeffm\Documents\BaySickDAW\CLAUDE.md`

- [ ] **Step 7.1: Append a Debug Build subsection under Build System**

The current Build System section reads:

```markdown
## Build System

- **Build command:** Run `do_build.bat` from `C:\Users\jeffm\Documents\BaySickDAW\`
- **Exe output:** `build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe`
- **Build dir:** `C:\Users\jeffm\Documents\BaySickDAW\build\`
- **Header dependencies:** handled automatically by MSBuild (the generator `do_build.bat` invokes). No manual `.obj` deletion needed after header edits — just re-run `do_build.bat`.
```

Replace with:

```markdown
## Build System

- **Build command:** Run `do_build.bat` from `C:\Users\jeffm\Documents\BaySickDAW\`. Builds BOTH Release and Debug per QA-0a (2026-05-07).
- **Release exe:** `build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe` — the shipping binary, used for music production. Branded BaySickDAW icon.
- **Debug exe:** `build\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe` — the diagnostic binary, used for verifying fixes. Generic Windows .exe icon (intentional — easy taskbar differentiation). Window title shows `[DEBUG]` suffix.
- **Build dir:** `C:\Users\jeffm\Documents\BaySickDAW\build\`
- **Build log:** `build_log.txt` at repo root. Two exit codes — `RELEASE_EXIT_CODE` and `DEBUG_EXIT_CODE`. Either non-zero = that config failed.
- **Header dependencies:** handled automatically by MSBuild. No manual `.obj` deletion after header edits — just re-run `do_build.bat`.
- **Standing rule (verifying Claude fixes):** run the Debug exe FIRST. Any `jassert` that fires shows a Windows dialog with file path + line + condition — screenshot to share. Then re-run in Release as the actual user test. Debug runs slower; audio that glitches in Debug under heavy load may be fine in Release. Always confirm in Release before declaring a real performance regression.
```

- [ ] **Step 7.2: Commit (CLAUDE.md workflow doc)**

```
git add CLAUDE.md
git commit -m "QA-0a Step 7: document Debug build workflow in CLAUDE.md."
```

---

### Task 8: Append QA-0a entry to implemented-work doc

**Files:**
- Modify: `..\Implemented Work Log.md`

- [ ] **Step 8.1: Append entry**

Below the existing 2026-05-07 Triage entry, append:

```
## 2026-05-07 — QA-0a — Debug build workflow setup

### Done
- Annotated main plan with the QA-0a fork using the hybrid convention: §0 Rule updated, §5/§6/§7 inline back-refs, §9 master log appended.
- do_build.bat now builds Release + Debug. Build log writes per-config exit codes.
- CMakeLists.txt gates ICON_BIG for Release-only — Debug exe ships with the generic Windows .exe icon.
- StandaloneApp window title appends " [DEBUG]" under #ifdef JUCE_DEBUG.
- Cold-start assertion triage: <N> asserts fired, <K> fixed inline, <M> documented as benign HOLD-FOR, <U> deferred to Phase 6 audit.
- CLAUDE.md Build System section updated with the new dual-config workflow + standing rule "verify in Debug first, then Release".

### Found along the way
- [list any surprises — e.g. "CMake multi-config gotcha required runtime icon suppression fallback (Step 3.4)" or "<asset>.png missing causes Debug build to fail in <site>" etc.]

### What was done about each finding
- [list resolutions]

### Carry-forward contradictions (if any)
- None expected (this batch doesn't touch architectural primitives).

### Files touched
- Modified: do_build.bat, CMakeLists.txt, Source/Standalone/StandaloneApp.cpp, CLAUDE.md
- Modified (plans): ../Main Plan.md (§0 rule update + §5/6/7 inline back-refs + §9 master log appended), ../Implemented Work Log.md (this entry)

### Commit(s)
- <HASH-FORK>      QA-0a Step 1: annotate main plan with QA-0a fork.
- <HASH-BUILDBAT>  QA-0a Step 2: do_build.bat builds Release + Debug.
- <HASH-ICON>      QA-0a Step 3: gate embedded exe icon for Release-only.
- <HASH-TITLE>     QA-0a Step 4: append [DEBUG] to window title in Debug builds.
- <HASH-TRIAGE>    QA-0a Step 6: triage cold-start asserts (<N> fixed, <M> documented).
- <HASH-CLAUDEMD>  QA-0a Step 7: document Debug build workflow in CLAUDE.md.

### Next action
- QA-0 (re-author plan as a fresh silly-name file at Task 9 below; then execute against the new dual-config build setup — verify in Debug first, then Release).
```

- [ ] **Step 8.2: Commit (implemented-work doc)**

```
git add ../Implemented Work Log.md
git commit -m "QA-0a Step 8: log to implemented-work doc."
```

---

### Task 9: Re-author the QA-0 plan as a separate file

The QA-0 plan was written earlier this session into the same file
this QA-0a plan now occupies. Its content lives in chat history. To
return to QA-0 cleanly after QA-0a closes, the plan content needs
to be on disk in its own file.

- [ ] **Step 9.1: Pick a fresh silly-name for the QA-0 plan file**

Following the convention of other plans in
`C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\Batch Plans\` (`lucky-discovering-tiger.md`,
`../Main Plan.md`). Suggested:
`composite-merging-rivers-twilight.md` — but pick whatever fits.

- [ ] **Step 9.2: Re-create QA-0 plan content from chat transcript**

The chat-history transcript captured the full QA-0 plan content
authored on 2026-05-07. Search for the chapter where the plan was
originally written (look for the file Write tool call with content
starting `# QA-0 Implementation Plan — MT Composite RenderTask
(DSP-12 restore)`). Copy verbatim into the new silly-name file.

Edit-vs-paste note: one section of the QA-0 plan referenced
`jassertfalse`; that is the right behavior NOW that QA-0a has
shipped (since QA-0a built the Debug build that makes the assert
useful). No content change needed in the QA-0 plan beyond restoring
it.

If chat-search fails (e.g. context truncation), re-author from
scratch using the same outline:
1. Strategy 1a (single Composite per audio row, lifecycle owned by
   ensureAudioInsert).
2. Order B execution (clear blockView → engine flow → arrangement-
   clip flow accumulating via renderAudioClipsForRow mtDest).
3. New `Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp`.
4. Delete `AudioInsertTask.h/.cpp` + `ClipPageTask.h/.cpp`.
5. Update register/unregister sites.
6. `jassertfalse` tripwire on dispatcher most-recent-wins fallback.
7. 12-case verification matrix.

- [ ] **Step 9.3: Commit the QA-0 plan file**

```
git add composite-merging-rivers-twilight.md
git commit -m "QA-0a Step 9: re-author QA-0 plan as separate file."
```

(Subject to plan files being tracked in git — see Step 1.3 caveat.)

- [ ] **Step 9.4: Update main plan §9 Forks to reference the new QA-0 plan filename**

Edit `../Main Plan.md`'s §9 Forks first entry,
replacing the placeholder `(filename TBD; main plan §9 entry updated
when chosen)` with the actual filename chosen in Step 9.1.

- [ ] **Step 9.5: Final commit**

```
git add ../Main Plan.md
git commit -m "QA-0a Step 9: link QA-0 plan filename in main plan."
```

---

## Verification (QA-0a acceptance)

QA-0a closes when ALL of the following are true:

1. `do_build.bat` produces both Release + Debug exes; both run.
2. The two exes are visually distinguishable on the taskbar (different
   icons OR identical-icons-with-different-window-titles depending on
   Step 3.3 outcome).
3. Cold-start triage complete: every assert that fired has been
   either fixed, documented as benign, or explicitly deferred.
4. CLAUDE.md Build System section reflects the new workflow.
5. Main plan reflects the hybrid fork convention: §0 Rule updated, §5/§6/§7 inline back-refs present, §9 Forks master log appended.
6. Implemented-work entry appended.
7. QA-0 plan re-authored as its own silly-name file on disk.

---

## Anti-pattern notes (carried forward)

- **No build runs from Claude.** Jeff runs `do_build.bat`. Each "build"
  step = "tell Jeff to run, await report".
- **No skipping Debug build verification.** Per QA-0a's standing rule,
  EVERY future "verify the fix" step in any batch's plan = "run
  Debug exe FIRST, then Release."
- **No `git add -A`.** Stage specific files only at every commit.
- **No `--no-verify` or `--amend`.** New commits at every checkpoint.
- **ASCII-only in any user-facing string.** The window title `[DEBUG]`
  suffix is pure ASCII. CLAUDE.md text is pure ASCII.
- **Don't speculate about asserts** in the cold-start triage — read
  the code at the file:line, decide based on what's there. If
  unclear, document and defer.

---

## Carry-Over

(Empty — created at plan write time. Populated at first stopping
point per main plan §0 Rule 2.)
