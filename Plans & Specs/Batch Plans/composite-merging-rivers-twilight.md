# QA-0 Implementation Plan - MT Composite RenderTask (DSP-12 restore)

> **For agentic workers:** Executed inline. Steps use checkbox `- [ ]` syntax.
> Read all three companion docs at
> `C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\*.md` BEFORE
> touching code (Rule 1 of the three-doc system).
>
> **Predecessor:** QA-0a closed 2026-05-07 (commits `b34c54d` / `a472a44` /
> `bd67fdf`). The Debug build is now usable as a diagnostic tool; the
> `jassertfalse` tripwire this batch ships will actually fire when triggered.

**Goal:** Restore WAV/MP3 Builder-grid playback under MT by replacing the
two competing render tasks at `audioInsert(N)` channel ids
(`AudioInsertTask` arrangement-timeline path + `ClipPageTask`
sampler-MIDI path) with a single `CompositeAudioInsertTask` that owns
BOTH flows and sums them internally before insert DSP.

**Architecture:** New task type registered ONCE per audio row at
`audioInsert(row)`. Order B execution: clear blockView once -> clip-engine
flow (if engine present) -> arrangement-clip flow (if song mode + clips
on row) -> both contributions accumulate in the row's arena slot. Matches
serial-mode "both contributions sum into the same routing accumulator"
parity.

**Tech stack:** JUCE 7 C++ (no headless tests - verification = in-app
12-case matrix, run in BOTH Debug and Release per QA-0a's standing rule).
MSVC /MD.

---

## Context

### What broke (DSP-12)

2026-05-07 triage discovered: WAV/MP3 drops on the Builder grid produce
silent playback under MT. Toggling MT off restores correct playback.
Cause: `onAudioClipAdded`'s cascade (`StandaloneEditor.cpp:1914-1949`)
calls `ensureAudioInsert(row)` -> registers `AudioInsertTask` at
`audioInsert(row)`, then `spawnClipsTabIfMissing(row, file)` ->
`registerClipEngine(row, eng)` -> registers `ClipPageTask` at the SAME
channel id. The dispatcher's `mTasksByChannel[chId] != nullptr` check
silently unregisters the previous task (most-recent-wins,
`RenderGraphDispatcher.cpp:55-56`). The Clips-tab page has no MIDI notes
triggering its sampler so the row outputs silence; the arrangement-clip
path is gone.

### Why composite (carry-forward §4 settled)

Single new task type owns both flows and sums them internally before
insert DSP. Restores parity with serial mode where both
`PluginProcessor.cpp:2082-2118` (clip-engine block) and the audio-clip
players Pass 2 (`PluginProcessor.cpp:2438-2445`) call
`routeInsertOutput(audioInsert(row), ...)` separately and the routing
graph's accumulator at `audioInsert(row)` sums both naturally.

### Why Order B inside the composite

`renderAudioClipsForRow` already supports an `mtDest` mode that ADDS each
per-clip post-rack contribution to the destination. Engine flow currently
writes engine output directly into blockView via
`engine->processBlock(blockView)` + in-place `processInsert(blockView)`.
Running engine flow FIRST populates blockView with post-rack engine
output; running arrangement-clip flow with `mtDest=&blockView` ADDS
per-clip contributions on top. No extra scratch buffer needed for engine
output. Order A would require an engine-scratch buffer; Order C would
require restructuring per-clip processInsert into per-row processInsert
(QA-J / DSP-06; explicitly deferred).

### What this batch is NOT fixing

- DSP-06 multi-clip stacking. Stays as-is (per-clip processInsert called
  N times per row); QA-J's job.
- Clip-engine peak draining behavior change. Both flows continue to
  CAS-max into `mAudioRowPeakDb*Run` mirrors as today.

### Decisions already made

| Topic | Decision |
|-------|----------|
| Class shape | **New `CompositeAudioInsertTask` file** (per QA-0a-era spec). Delete old `AudioInsertTask.h/.cpp` + `ClipPageTask.h/.cpp`. `mClipRenderTasks[]` array deleted. |
| Lifecycle | **Strategy 1a.** One Composite per audio row, created by `ensureAudioInsert`. `registerClipEngine` sets a pointer on the existing instance (no separate task registered). `unregisterClipEngine` clears the pointer (instance survives). |
| Execution order inside `run()` | **Order B.** Clear blockView -> engine flow (if engine) -> arrangement-clip flow accumulating via `renderAudioClipsForRow` mtDest. |
| Dispatcher fallback | **`jassertfalse` tripwire** on the most-recent-wins branch (Debug-active per QA-0a; Release stays silent). |

---

## File structure

| Action | File | Responsibility |
|--------|------|---------------|
| Create | `Source/Engine/Tasks/CompositeAudioInsertTask.h` | Composite RenderTask declaration. |
| Create | `Source/Engine/Tasks/CompositeAudioInsertTask.cpp` | Composite RenderTask implementation: clip-engine flow + arrangement-clip flow in one `run()`. |
| Delete | `Source/Engine/Tasks/AudioInsertTask.h` | Old per-row clip task (responsibilities absorbed). |
| Delete | `Source/Engine/Tasks/AudioInsertTask.cpp` | Old per-row clip task (responsibilities absorbed). |
| Delete | `Source/Engine/Tasks/ClipPageTask.h` | Old clip-engine task (responsibilities absorbed). |
| Delete | `Source/Engine/Tasks/ClipPageTask.cpp` | Old clip-engine task (responsibilities absorbed). |
| Modify | `CMakeLists.txt` | Replace two old source entries with the new one (lines 140-141). |
| Modify | `Source/PluginProcessor.h` | Include swap; friend class swap; `mAudioRenderTasks` array type change; delete `mClipRenderTasks` array (lines 30-31, 51-52, 1090-1091). |
| Modify | `Source/PluginProcessor.cpp` | `ensureAudioInsert` constructs Composite; `registerClipEngine` sets composite's clip-engine pointer + defensively ensures the row exists; `unregisterClipEngine` clears the pointer (no task registration churn); serial Pass 2 still calls `mAudioRenderTasks[row]->getClipScratch(...)` - same API, new type. Lines 2440-2444, 4209-4257, 4571-4577. |
| Modify | `Source/Engine/RenderGraphDispatcher.cpp` | Tighten most-recent-wins fallback to `jassertfalse` + comment (lines 55-56). |

---

## Tasks

### Task 1: Skeleton + CMake wire-up

**Files:**
- Create: `Source/Engine/Tasks/CompositeAudioInsertTask.h`
- Create: `Source/Engine/Tasks/CompositeAudioInsertTask.cpp`
- Modify: `CMakeLists.txt:140-141`

- [ ] **Step 1.1: Create the header skeleton**

Write `Source/Engine/Tasks/CompositeAudioInsertTask.h`:

```cpp
#pragma once

#include <JuceHeader.h>

#include "../RenderTask.h"
#include "../BlockContext.h"

class VibeGraph;
class ISidechainEngine;
class VibeSynthProcessor;

// CompositeAudioInsertTask
// ------------------------
// 2026-05-07 (QA-0): single render task per audio row that owns BOTH
// previously-conflicting flows at channelId = audioInsert(row):
//
//   1. Clip-engine flow (was ClipPageTask): a sampler-style
//      juce::AudioProcessor that the user assigned to a Clips ribbon tab,
//      driven by piano-roll MIDI from BlockContext::clipPageMidi[row].
//      processBlock writes engine audio into the row's arena slot;
//      processInsert applies polarity / preEq / width / rack / postEq /
//      fader x mute x solo / PDC / peak in-place.
//
//   2. Arrangement-clip flow (was AudioInsertTask): per-row decode of
//      every NON-FilePlay AudioClipPlayer through the shared helper
//      VibeSynthProcessor::renderAudioClipsForRow (mtDest = blockView).
//      Each clip's post-rack output is ADDED to blockView (additive).
//
// Order B inside run(): clear blockView once -> clip-engine flow
// (if engine set) -> arrangement-clip flow (if song mode + has clips).
// Both contributions accumulate in mOutputBuffer.
//
// Lifecycle (Strategy 1a):
//   - One instance per audio row, created by
//     VibeSynthProcessor::ensureAudioInsert and registered with the
//     dispatcher under audioInsert(row).
//   - registerClipEngine(pageIdx, eng) sets mClipEngine on the existing
//     instance (no separate task registered, no most-recent-wins).
//   - unregisterClipEngine clears mClipEngine (instance survives).
class CompositeAudioInsertTask : public RenderTask
{
public:
    CompositeAudioInsertTask (int                 row,
                              int                 channelIdIn,
                              VibeGraph&          graph,
                              VibeSynthProcessor& processor);

    void run() override;

    void setClipEngine (juce::AudioProcessor* engine);

    juce::AudioBuffer<float>& getClipScratch (int numChannels, int numSamples);

private:
    int                                mIndex     = 0;
    VibeGraph*                         mGraph     = nullptr;
    VibeSynthProcessor*                mProcessor = nullptr;
    std::atomic<juce::AudioProcessor*> mClipEngine { nullptr };
    std::atomic<ISidechainEngine*>     mScEngine   { nullptr };
    juce::AudioBuffer<float>           mClipScratch;
};
```

- [ ] **Step 1.2: Create the implementation skeleton (empty run, factor in later)**

Write `Source/Engine/Tasks/CompositeAudioInsertTask.cpp`:

```cpp
#include "CompositeAudioInsertTask.h"
#include "../../PluginProcessor.h"
#include "../../PatternManager.h"
#include "../../VibeGraph.h"
#include "../../DSP/EngineSidechainHelper.h"
#include "../SidechainPullHelper.h"

#include <atomic>
#include <limits>

CompositeAudioInsertTask::CompositeAudioInsertTask (int                 row,
                                                    int                 channelIdIn,
                                                    VibeGraph&          graph,
                                                    VibeSynthProcessor& processor)
    : mIndex (row),
      mGraph (&graph),
      mProcessor (&processor)
{
    this->channelId = channelIdIn;
}

void CompositeAudioInsertTask::setClipEngine (juce::AudioProcessor* engine)
{
    mClipEngine.store (engine, std::memory_order_release);
    mScEngine .store (dynamic_cast<ISidechainEngine*> (engine),
                      std::memory_order_release);
}

juce::AudioBuffer<float>& CompositeAudioInsertTask::getClipScratch (int numChannels,
                                                                    int numSamples)
{
    if (mClipScratch.getNumChannels() != numChannels
        || mClipScratch.getNumSamples() < numSamples)
    {
        mClipScratch.setSize (numChannels, numSamples,
                              /*keepExistingContent*/ false,
                              /*clearExtraSpace*/    false,
                              /*avoidReallocating*/  true);
    }
    return mClipScratch;
}

void CompositeAudioInsertTask::run()
{
    // Filled in Task 2.
}
```

- [ ] **Step 1.3: Wire into CMakeLists.txt**

Edit `CMakeLists.txt:139-141` (replace ClipPageTask + AudioInsertTask
entries with the new file). Note the Task 4 deletions of the old files
won't break the build because we're replacing the CMake refs at the same
time.

- [ ] **Step 1.4: Build (compile-only sanity)**

Run `do_build.bat`. Expected: clean build with both Release + Debug
exit codes 0. Old files still on disk but no longer referenced by CMake;
they compile + relink against unchanged old PluginProcessor.h / .cpp.

- [ ] **Step 1.5: Commit**

```
git add Source/Engine/Tasks/CompositeAudioInsertTask.h \
        Source/Engine/Tasks/CompositeAudioInsertTask.cpp \
        CMakeLists.txt
git commit -m "QA-0 Step 1: scaffold CompositeAudioInsertTask (skeleton)."
```

---

### Task 2: Fill in `CompositeAudioInsertTask::run()`

**Files:**
- Modify: `Source/Engine/Tasks/CompositeAudioInsertTask.cpp` (run body)

- [ ] **Step 2.1: Implement run() - Order B, clip-engine flow first**

Replace the empty `run()` body with the full Order B implementation
(SC pull once, engine flow if engine present + drain peaks, then
arrangement-clip flow gated on song mode + playing + PatternManager).
See QA-0a plan archive (in this session's chat) for the verbatim body
or write from the spec above.

- [ ] **Step 2.2: Build**

Tell Jeff: run `do_build.bat`. Expected clean. If `mProcessor->mAudioRowPeakDbLRun` access fails to compile, friend declaration in Task 3.1 hasn't landed yet - add `friend class CompositeAudioInsertTask;` near the existing `friend class AudioInsertTask;` to get past this step.

- [ ] **Step 2.3: Commit**

```
git add Source/Engine/Tasks/CompositeAudioInsertTask.cpp
git commit -m "QA-0 Step 2: implement Composite::run() (Order B both flows)."
```

---

### Task 3: Wire the new task at registration sites

**Files:**
- Modify: `Source/PluginProcessor.h` (lines 30-31, 51-52, 1090-1091)
- Modify: `Source/PluginProcessor.cpp` (lines 2440-2444, 4209-4257, 4571-4577)

- [ ] **Step 3.1: Swap includes + friend decls in PluginProcessor.h**

Replace the two old includes + two old friend decls with single
`CompositeAudioInsertTask` versions.

- [ ] **Step 3.2: Swap array storage in PluginProcessor.h**

`mAudioRenderTasks` becomes `std::array<std::unique_ptr<CompositeAudioInsertTask>, kMaxAudioRows>`.
Delete `mClipRenderTasks`.

- [ ] **Step 3.3: Update `ensureAudioInsert` to construct Composite**

At `Source/PluginProcessor.cpp:4571-4577`, replace `AudioInsertTask`
construction with `CompositeAudioInsertTask` (signature change: now also
takes `mVibeGraph` reference).

- [ ] **Step 3.4: Update `registerClipEngine`**

Replace the registration logic with: defensively call
`ensureAudioInsert(pageIdx, "Audio " + ...)` to make sure the per-row
Composite exists, then `mAudioRenderTasks[pageIdx]->setClipEngine(eng)`.
No separate task registration, no most-recent-wins triggered.

- [ ] **Step 3.5: Update `unregisterClipEngine`**

Replace with: clear the Composite's clip-engine pointer
(`task->setClipEngine(nullptr)`); the per-row Composite stays alive.

- [ ] **Step 3.6: Verify serial Pass 2 compiles**

`Source/PluginProcessor.cpp:2438-2445` calls
`task->getClipScratch(numOut, numSamples)` on the per-row task. Same
API on Composite, just different type. Should compile cleanly.

- [ ] **Step 3.7: Build (full compile + link)**

`do_build.bat`. If link errors mention old types, Task 4 hasn't run
yet and that's fine.

- [ ] **Step 3.8: Quick smoke test - DEBUG FIRST per QA-0a standing rule**

Run the Debug exe. Drop a WAV on the Builder grid. Press play. Audio
should play under MT (the bug fix is functionally complete at this
point - Task 4 is just dead-code cleanup).

- [ ] **Step 3.9: Commit**

```
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "QA-0 Step 3: wire CompositeAudioInsertTask at registration sites; drop separate ClipPageTask registration."
```

---

### Task 4: Delete the old AudioInsertTask + ClipPageTask source files

- [ ] **Step 4.1: Sanity check - no other references**

```
git grep -nE 'AudioInsertTask|ClipPageTask' -- Source/
```

Expected: only stale comments. Update or leave per project convention.

- [ ] **Step 4.2: Delete the four source files**

```
git rm Source/Engine/Tasks/AudioInsertTask.h
git rm Source/Engine/Tasks/AudioInsertTask.cpp
git rm Source/Engine/Tasks/ClipPageTask.h
git rm Source/Engine/Tasks/ClipPageTask.cpp
```

- [ ] **Step 4.3: Build**

`do_build.bat`. Expected: clean. CMakeLists already points at the new
file; deletions just trim the disk.

- [ ] **Step 4.4: Commit**

```
git add -u
git commit -m "QA-0 Step 4: remove orphaned AudioInsertTask + ClipPageTask sources."
```

---

### Task 5: Dispatcher tripwire on most-recent-wins fallback

**Files:**
- Modify: `Source/Engine/RenderGraphDispatcher.cpp:54-56`

- [ ] **Step 5.1: Add jassertfalse + clarifying comment**

Wrap the fallback in `jassertfalse` so future regressions surface
loudly in Debug. Release behavior unchanged (jassertfalse compiles out).

```cpp
    // If something was already registered at this id, replace it. The caller
    // is responsible for the lifetime of the previous task.
    //
    // QA-0 (2026-05-07): for normal paths this branch is now unreachable -
    // the per-row Composite at audioInsert(N) is registered exactly once
    // by ensureAudioInsert; registerClipEngine sets a pointer on the
    // existing Composite rather than registering a separate task.  jassert
    // surfaces any future regression where a new task type re-introduces
    // a multi-source channel without going through a composite shape;
    // the silent fallback below stays as a safety net for release builds.
    if (mTasksByChannel[(size_t) chId] != nullptr)
    {
        jassertfalse;
        unregisterTask (chId);
    }
```

- [ ] **Step 5.2: Build (recompile dispatcher unit only)**

`do_build.bat`. Expected: clean.

- [ ] **Step 5.3: Commit**

```
git add Source/Engine/RenderGraphDispatcher.cpp
git commit -m "QA-0 Step 5: jassertfalse tripwire on dispatcher most-recent-wins fallback."
```

---

### Task 6: 12-case verification matrix (Debug FIRST, then Release)

**Per QA-0a's standing rule, run every case in Debug exe FIRST, then
re-run in Release exe.**

**Cases:**

| # | Drop target | File | MT | Expected |
|---|-------------|------|----|----------|
| 1 | Builder grid | WAV | ON  | Plays |
| 2 | Builder grid | WAV | OFF | Plays |
| 3 | Builder grid | MP3 | ON  | Plays |
| 4 | Builder grid | MP3 | OFF | Plays |
| 5 | Clips tab    | WAV | ON  | Plays via piano-roll |
| 6 | Clips tab    | WAV | OFF | Plays via piano-roll |
| 7 | Clips tab    | MP3 | ON  | Plays via piano-roll |
| 8 | Clips tab    | MP3 | OFF | Plays via piano-roll |
| 9 | Both: Builder block + piano-roll trigger on same row | WAV | ON | Both audible (summed) |
| 10 | Both: Builder block + piano-roll trigger on same row | WAV | OFF | Both audible (summed) |
| 11 | Both: Builder block + piano-roll trigger on same row | MP3 | ON | Both audible (summed) |
| 12 | Both: Builder block + piano-roll trigger on same row | MP3 | OFF | Both audible (summed) |

**MT-on cases (1, 3, 5, 7, 9, 11):** in Debug only the toggle persists
(per QA-0a finding #9, MT is no-op in Debug); for actual MT validation
re-run in Release. Effective Debug coverage = MT-OFF cases (2/4/6/8/10/12).

- [ ] **Step 6.1: Cold-start each case in Debug exe**

For each case, launch fresh, toggle MT to the case's setting via Mixer
hamburger, drop the file, press play (or trigger piano-roll for
Clips-tab cases), listen.

- [ ] **Step 6.2: Re-run cases 1, 3, 5, 7, 9, 11 in Release**

These are the MT-on cases that need real MT runtime to validate. Release
MT engine engages correctly; the meter shows the difference.

- [ ] **Step 6.3: Per-batch verification ladder (main plan §7)**

After 12 cases pass:
1. `do_build.bat` clean (both exit codes 0).
2. App launches in both Debug and Release.
3. Open existing big project (5 Guitars + 5 Bass + 1 Rusty) - no crash, audio plays.
4. Save -> close -> reopen -> load - round-trip clean.
5. MT toggle round-trip (in Release): hamburger -> off -> identical serial-path audio -> on -> identical MT-path audio. Settings.xml persistence verified across restart.

- [ ] **Step 6.4: Regression sweep on neighboring DSP / strip surfaces**

- Drop a Vox file on a Vox strip - recording / playback unaffected.
- Drum pattern still plays correctly.
- Sidechain compressor on a clip still receives SC input.

If any regress, the SC pull or peak drain path got broken - revert to
Step 3.9's commit and re-audit.

---

### Task 7: Final commit + log

- [ ] **Step 7.1: Verify clean working tree**

```
git status
```

Expected: only `.claude/settings.local.json` + planning files.

- [ ] **Step 7.2: Append entry to implemented-work doc**

Edit `..\Implemented Work Log.md`,
append after the QA-0a entry. Include final commit hashes from
`git log --oneline -5`.

- [ ] **Step 7.3: Final commit (the implemented-work entry)**

The plan files live outside the BaySickDAW git repo (no commit there);
the file edit lands regardless.

---

## Verification (composite acceptance)

QA-0 closes when ALL of the following are true:

1. 12-case matrix passes in BOTH Debug and Release per Step 6.1+6.2.
2. Per-batch verification ladder passes (Step 6.3).
3. Regression sweep on neighboring items passes (Step 6.4).
4. `git status` clean modulo planning files.
5. Implemented-work entry appended.

---

## Anti-pattern notes (carried forward from QA-0a)

- **Verify in Debug first, then Release** at every step that says "build".
- **No build runs from Claude.** Jeff runs `do_build.bat`.
- **Stage specific files only** at every commit (no `git add -A`).
- **No `--no-verify` or `--amend`.** New commits at every checkpoint.
- **ASCII-only in any user-facing string.** No em-dashes; QA-0a swept the project clean.

---

## Carry-Over

(Empty - created at plan write time. Populated at first stopping
point per main plan §0 Rule 2.)
