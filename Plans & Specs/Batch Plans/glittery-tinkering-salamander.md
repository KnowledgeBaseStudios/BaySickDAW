# QA-Md: MT Engine Debug-Build Investigation

> **For execution:** use `superpowers:executing-plans` inline per the QA-Md prompt. Steps use `- [ ]` checkbox syntax. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

## Context

Per QA-0a finding #9 (logged 2026-05-07 in [Implemented Work Log.md](../Implemented Work Log.md), promoted from Phase 5 → Phase 1 at QA-0 close per [Main Plan.md](../Main Plan.md) §9 third Forks entry):

**Symptom.** Under Debug builds, the multi-threaded render path is a no-op. The DSP meter reads identically with MT toggled on vs off; settings.xml persistence works; the toggle UI works; but the dispatcher isn't actually distributing work to threads. Audio plays cleanly (Jeff confirmed 2026-05-08) — workers just aren't doing anything productive. Release MT works correctly (verified during Batch 10 ship + QA-0's 12-case matrix).

**Why this batch matters.** QA-0a stood up the dual-config Debug build to give Jeff a proper diagnostic loop (jassert dialogs over plain-English bug reports). Every downstream batch that touches audio code (most of them) needs MT to actually engage under Debug to surface MT-specific bugs precisely. Without QA-Md, those batches fly blind on MT in Debug.

**Static-analysis findings (pre-plan investigation, 2026-05-08):**

- No `_DEBUG`/`NDEBUG`/`JUCE_DEBUG` ifdefs anywhere in [`Source/Engine/`](../../Source/Engine).
- `VibeThreadPool::VibeThreadPool` at [`VibeThreadPool.cpp:22`](../../Source/Engine/VibeThreadPool.cpp) spawns workers unconditionally in ctor; no Debug-conditional sizing.
- `computeRenderWorkerCount` at [`PluginProcessor.cpp:114`](../../Source/PluginProcessor.cpp) returns `min(hw_concurrency-1, 8)` — same code in both configs.
- The MT branch at [`PluginProcessor.cpp:1834`](../../Source/PluginProcessor.cpp) is a runtime atomic load — both branches present in the binary.
- `MasterTask::run` at [`MasterTask.cpp:16`](../../Source/Engine/Tasks/MasterTask.cpp) always fires `mDoneFlag->store(true, release)` even on early-out paths.
- `kWatchdogTimeoutMillis = 100.0` ms ([`RenderEngineFlags.h:64`](../../Source/Engine/RenderEngineFlags.h)) — fixed across configs.

So the puzzle is purely runtime. Most likely cause given "audio plays cleanly + identical DSP meter": **the audio thread is pumping every task itself via `runUntilOrTimeout`** while workers sit idle (asleep, or losing the race for queue items). DSP meter measures audio-thread wall-clock — same as serial when main thread does all the work.

## Goal

Restore meaningful MT parallelism under Debug builds by (1) instrumenting the dispatcher + worker pool to observe per-thread task distribution, (2) running the instrumented build to identify the root cause, (3) fixing it, then (4) deciding whether to keep instrumentation as a permanent diagnostic.

## Architecture

**Diagnostic-first, four phases:**

1. **Phase 1 — Instrument.** Add atomic counters to `VibeThreadPool` + `RenderGraphDispatcher`. Add a "Run MT Diagnostic" item to the existing Mixer hamburger menu (id 203, sequential after the existing 201/202). Click handler resets counters, sets capture flag on, sleeps 2 s on message thread, sets capture flag off, snapshots counters, pops `juce::AlertWindow` with formatted summary. All counter increments are gated on `gCaptureOn.load(memory_order_relaxed)` so the diagnostic adds zero hot-path cost when off. Both Debug and Release builds get the instrumentation (Release is the known-good baseline).
2. **Phase 2 — Diagnose.** Run the instrumented build twice while a known session plays (Release first as baseline, then Debug). Capture screenshots. Compare counters. Identify root-cause branch:
    - **Branch A** — Workers spawn but stay asleep (signal protocol broken under Debug timing).
    - **Branch B** — Watchdog firing every block (Debug ~5x slower → 100 ms deadline too tight).
    - **Branch C** — Audio thread monopolizes the queue (workers wake but lose tryPop race).
    - **Branch D** — Other cause; surface to Jeff and re-plan.
3. **Phase 3 — Fix.** Branch A/B/C plans pre-spec'd below; Branch D re-plans via plan-mode + AskUserQuestion.
4. **Phase 4 — Cleanup + close.** Decide instrumentation fate (keep / remove / compile-flag-gate). `/draft-doc batch-close`, `/review-batch QA-Md`, commit, append [Implemented Work Log.md](../Implemented Work Log.md) entry.

**Tech Stack.** C++17, JUCE 7, std::atomic counters (relaxed ordering — diagnostic only), `juce::AlertWindow::showAsync`, `juce::Thread::sleep`, do_build.bat dual-config build, screenshot capture for chat round-trip.

## File Map

**Modified:**
- [`Source/Engine/RenderEngineFlags.h`](../../Source/Engine/RenderEngineFlags.h) — add `MtDiagnostic` namespace with capture-on flag + counter atomics + `reset()` + `Snapshot` struct + `snapshot()` helper.
- [`Source/Engine/VibeThreadPool.cpp`](../../Source/Engine/VibeThreadPool.cpp) — increment counters in `runOneTask` (`gChildSubmits++` when submitting a freed child), `workerLoop` (`gWorkerSpinFinds++` / `gWorkerSleepFinds++` / `gWorkerIdleSleeps++` / `gWorkerWakes++` / `gWorkerTasks++`), `runUntilOrTimeout` (`gMainThreadTasks++` per dispatch + `gWatchdogFires++` on timeout exit).
- [`Source/Engine/RenderGraphDispatcher.cpp`](../../Source/Engine/RenderGraphDispatcher.cpp) — increment counters in `dispatchBlock` (`gBlockCount++` at entry, `gLeavesSubmitted++` per leaf).
- [`Source/Standalone/StandaloneEditor.cpp`](../../Source/Standalone/StandaloneEditor.cpp) — add `Mixer hamburger → "Run MT Diagnostic"` menu item (id 203) and click handler around line 4450-4514.

**Read-only references:**
- [`Source/Engine/Tasks/MasterTask.cpp`](../../Source/Engine/Tasks/MasterTask.cpp) — confirm done-flag fires on all paths.
- [`Source/PluginProcessor.cpp:1821-1895`](../../Source/PluginProcessor.cpp) — the MT branch in processBlock.

**Plan + close-out (read + append-only):**
- [`Plans & Specs/Main Plan.md`](../Main Plan.md) — update QA-Md `**Plan file:**` line at Phase 0; append §9 Forks entry at close if scope changed.
- [`Plans & Specs/Implemented Work Log.md`](../Implemented Work Log.md) — append batch-close entry.

---

## Phase 0 — Open the batch

### Task 0: Update Main Plan §5 QA-Md plan-file pointer

**Files:**
- Modify: [`Plans & Specs/Main Plan.md:664`](../Main Plan.md)

- [ ] **Step 0.1: Update `**Plan file:**` line**

Edit Main Plan.md line 664 from:

```
**Plan file:** TBD (silly-name file when batch starts)
```

to:

```
**Plan file:** `Plans & Specs/Batch Plans/glittery-tinkering-salamander.md`
```

- [ ] **Step 0.2: Stage + commit (chore commit, no source change)**

```
git add "Plans & Specs/Batch Plans/glittery-tinkering-salamander.md" "Plans & Specs/Main Plan.md"
git commit -m "QA-Md: open batch with plan file reference."
```

---

## Phase 1 — Instrumentation

### Task 1: Add `MtDiagnostic` counter namespace

**Files:**
- Modify: [`Source/Engine/RenderEngineFlags.h`](../../Source/Engine/RenderEngineFlags.h)

- [ ] **Step 1.1: Add MtDiagnostic namespace block**

Append to `Source/Engine/RenderEngineFlags.h` (inside the `namespace RenderEngine { ... }` block, after the existing `kMaxStripChannels` constant):

```cpp
    // 2026-05-08 (QA-Md): diagnostic counters for the MT-no-op-in-Debug
    // investigation.  All counters are gated on gCaptureOn so the hot path
    // pays only one relaxed-load per increment site when capture is off.
    // Use std::atomic with relaxed ordering -- these are diagnostic only,
    // no algorithmic dependency.
    //
    // Lifecycle: StandaloneEditor's "Run MT Diagnostic" menu item
    // calls reset() -> sets gCaptureOn=true -> sleeps 2 s -> sets
    // gCaptureOn=false -> snapshot() -> displays AlertWindow.
    namespace MtDiagnostic
    {
        inline std::atomic<bool>      gCaptureOn        { false };

        inline std::atomic<long long> gBlockCount       { 0 };  // dispatchBlock invocations
        inline std::atomic<long long> gLeavesSubmitted  { 0 };  // pool.submit calls from dispatcher leaf seeding
        inline std::atomic<long long> gChildSubmits     { 0 };  // pool.submit calls from runOneTask child cascade
        inline std::atomic<long long> gWatchdogFires    { 0 };  // runUntilOrTimeout returned false (block timed out)

        inline std::atomic<long long> gMainThreadTasks  { 0 };  // tasks runOneTask'd from runUntilOrTimeout
        inline std::atomic<long long> gWorkerTasks      { 0 };  // tasks runOneTask'd from workerLoop (any worker)
        inline std::atomic<long long> gWorkerSpinFinds  { 0 };  // workerLoop spin-phase tryPop hits
        inline std::atomic<long long> gWorkerSleepFinds { 0 };  // workerLoop sleep-phase tryPop hits (race resolution)
        inline std::atomic<long long> gWorkerIdleSleeps { 0 };  // workerLoop entered waker.wait
        inline std::atomic<long long> gWorkerWakes      { 0 };  // workerLoop returned from waker.wait

        struct Snapshot
        {
            long long blockCount;
            long long leavesSubmitted;
            long long childSubmits;
            long long watchdogFires;
            long long mainThreadTasks;
            long long workerTasks;
            long long workerSpinFinds;
            long long workerSleepFinds;
            long long workerIdleSleeps;
            long long workerWakes;
        };

        inline void reset()
        {
            gBlockCount      .store (0);
            gLeavesSubmitted .store (0);
            gChildSubmits    .store (0);
            gWatchdogFires   .store (0);
            gMainThreadTasks .store (0);
            gWorkerTasks     .store (0);
            gWorkerSpinFinds .store (0);
            gWorkerSleepFinds.store (0);
            gWorkerIdleSleeps.store (0);
            gWorkerWakes     .store (0);
        }

        inline Snapshot snapshot()
        {
            return {
                gBlockCount      .load(),
                gLeavesSubmitted .load(),
                gChildSubmits    .load(),
                gWatchdogFires   .load(),
                gMainThreadTasks .load(),
                gWorkerTasks     .load(),
                gWorkerSpinFinds .load(),
                gWorkerSleepFinds.load(),
                gWorkerIdleSleeps.load(),
                gWorkerWakes     .load()
            };
        }
    }
```

- [ ] **Step 1.2: Verify compiles — Jeff runs do_build.bat**

Both Release and Debug should build clean (header-only change; no link impact).

Expected: build succeeds; both `BaySickDAW.exe` (Release) and `BaySickDAW.exe` (Debug `[DEBUG]` window title) produced.

- [ ] **Step 1.3: Commit Task 1**

```
git add Source/Engine/RenderEngineFlags.h
git commit -m "QA-Md Step 1: add MtDiagnostic counter namespace."
```

### Task 2: Wire counters into VibeThreadPool

**Files:**
- Modify: [`Source/Engine/VibeThreadPool.cpp`](../../Source/Engine/VibeThreadPool.cpp)

- [ ] **Step 2.1: Add `gChildSubmits` increment to runOneTask child cascade**

In `runOneTask` (current code lines 81-97), modify the child loop:

Current:

```cpp
for (auto* child : task->mChildren)
{
    const int prev = child->mDeps.fetch_sub (1, std::memory_order_acq_rel);
    if (prev == 1)
        submit (child);
}
```

Replace with:

```cpp
for (auto* child : task->mChildren)
{
    const int prev = child->mDeps.fetch_sub (1, std::memory_order_acq_rel);
    if (prev == 1)
    {
        if (RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed))
            RenderEngine::MtDiagnostic::gChildSubmits.fetch_add (1, std::memory_order_relaxed);
        submit (child);
    }
}
```

- [ ] **Step 2.2: Add `gMainThreadTasks` + `gWatchdogFires` to runUntilOrTimeout**

In `runUntilOrTimeout` (current code lines 115-132), add per-task and on-timeout increments:

Current:

```cpp
bool VibeThreadPool::runUntilOrTimeout (std::atomic<bool>& done,
                                          double              deadlineMillisHiRes) noexcept
{
    while (! done.load (std::memory_order_acquire))
    {
        if (auto* task = tryPop())
        {
            runOneTask (task);
        }
        else
        {
            if (juce::Time::getMillisecondCounterHiRes() >= deadlineMillisHiRes)
                return false;
            std::this_thread::yield();
        }
    }
    return true;
}
```

Replace with:

```cpp
bool VibeThreadPool::runUntilOrTimeout (std::atomic<bool>& done,
                                          double              deadlineMillisHiRes) noexcept
{
    const bool capture = RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed);
    while (! done.load (std::memory_order_acquire))
    {
        if (auto* task = tryPop())
        {
            if (capture)
                RenderEngine::MtDiagnostic::gMainThreadTasks.fetch_add (1, std::memory_order_relaxed);
            runOneTask (task);
        }
        else
        {
            if (juce::Time::getMillisecondCounterHiRes() >= deadlineMillisHiRes)
            {
                if (capture)
                    RenderEngine::MtDiagnostic::gWatchdogFires.fetch_add (1, std::memory_order_relaxed);
                return false;
            }
            std::this_thread::yield();
        }
    }
    return true;
}
```

(Note: `capture` cached at fn entry — the 2-second window is many blocks long, capture state won't flip mid-call.)

- [ ] **Step 2.3: Add worker counters to workerLoop**

In `workerLoop` (current code lines 141-181), add increments to spin/sleep/wake/idle paths.

Current:

```cpp
void VibeThreadPool::workerLoop (int workerIndex) noexcept
{
    auto& waker = *mImpl->wakers[(size_t) workerIndex];

    while (! mShutdown.load (std::memory_order_acquire))
    {
        bool gotTask = false;
        for (int i = 0; i < RenderEngine::kWorkerSpinIterations; ++i)
        {
            if (auto* task = tryPop())
            {
                runOneTask (task);
                gotTask = true;
                break;
            }
        }
        if (gotTask)
            continue;

        mImpl->activeWaiters.fetch_add (1, std::memory_order_release);

        if (auto* task = tryPop())
        {
            mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
            runOneTask (task);
            continue;
        }

        waker.wait (-1);

        mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
    }
}
```

Replace with:

```cpp
void VibeThreadPool::workerLoop (int workerIndex) noexcept
{
    auto& waker = *mImpl->wakers[(size_t) workerIndex];

    while (! mShutdown.load (std::memory_order_acquire))
    {
        const bool capture = RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed);

        bool gotTask = false;
        for (int i = 0; i < RenderEngine::kWorkerSpinIterations; ++i)
        {
            if (auto* task = tryPop())
            {
                if (capture)
                {
                    RenderEngine::MtDiagnostic::gWorkerSpinFinds.fetch_add (1, std::memory_order_relaxed);
                    RenderEngine::MtDiagnostic::gWorkerTasks    .fetch_add (1, std::memory_order_relaxed);
                }
                runOneTask (task);
                gotTask = true;
                break;
            }
        }
        if (gotTask)
            continue;

        mImpl->activeWaiters.fetch_add (1, std::memory_order_release);

        if (auto* task = tryPop())
        {
            mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
            if (capture)
            {
                RenderEngine::MtDiagnostic::gWorkerSleepFinds.fetch_add (1, std::memory_order_relaxed);
                RenderEngine::MtDiagnostic::gWorkerTasks     .fetch_add (1, std::memory_order_relaxed);
            }
            runOneTask (task);
            continue;
        }

        if (capture)
            RenderEngine::MtDiagnostic::gWorkerIdleSleeps.fetch_add (1, std::memory_order_relaxed);

        waker.wait (-1);

        if (capture)
            RenderEngine::MtDiagnostic::gWorkerWakes.fetch_add (1, std::memory_order_relaxed);

        mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
    }
}
```

- [ ] **Step 2.4: Verify compiles — Jeff runs do_build.bat**

Both configs must build clean.

- [ ] **Step 2.5: Commit Task 2**

```
git add Source/Engine/VibeThreadPool.cpp
git commit -m "QA-Md Step 2: instrument VibeThreadPool counters."
```

### Task 3: Wire counters into RenderGraphDispatcher

**Files:**
- Modify: [`Source/Engine/RenderGraphDispatcher.cpp`](../../Source/Engine/RenderGraphDispatcher.cpp)

- [ ] **Step 3.1: Add `gBlockCount` + `gLeavesSubmitted` to dispatchBlock**

In `dispatchBlock` (current code starting line 207), add increments after the early-return guard and inside the leaf-seed loop.

Insert one increment after the `if (n <= 0 || mTasks.empty()) { ... return; }` guard (top of `dispatchBlock` proper, just before the dep-counter reset loop):

```cpp
    if (RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed))
        RenderEngine::MtDiagnostic::gBlockCount.fetch_add (1, std::memory_order_relaxed);
```

Modify the leaf-seed loop (currently lines 233-235) — add the per-leaf increment:

Current:

```cpp
    // ── Seed leaves ─────────────────────────────────────────────────────────
    for (auto* t : mTasks)
        if (t != nullptr && t->mInitialDeps == 0)
            mPool.submit (t);
```

Replace with:

```cpp
    // ── Seed leaves ─────────────────────────────────────────────────────────
    {
        const bool capture = RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed);
        for (auto* t : mTasks)
            if (t != nullptr && t->mInitialDeps == 0)
            {
                if (capture)
                    RenderEngine::MtDiagnostic::gLeavesSubmitted.fetch_add (1, std::memory_order_relaxed);
                mPool.submit (t);
            }
    }
```

- [ ] **Step 3.2: Verify compiles — Jeff runs do_build.bat**

Both configs must build clean.

- [ ] **Step 3.3: Commit Task 3**

```
git add Source/Engine/RenderGraphDispatcher.cpp
git commit -m "QA-Md Step 3: instrument RenderGraphDispatcher block + leaf counters."
```

### Task 4: Add "Run MT Diagnostic" menu item

**Files:**
- Modify: [`Source/Standalone/StandaloneEditor.cpp:4450-4514`](../../Source/Standalone/StandaloneEditor.cpp)

- [ ] **Step 4.1: Add menu item id 203 to Mixer hamburger PopupMenu build**

In the Mixer hamburger menu builder, after the existing `m.addItem (202, "Multi-core Rendering", ...)` line (around 4451-4454), add:

```cpp
                    // 2026-05-08 (QA-Md): diagnostic capture for the
                    // MT-no-op-in-Debug investigation.  Click triggers
                    // a 2-second counter capture window and pops an
                    // AlertWindow with the per-thread task distribution.
                    m.addItem (203,
                                "Run MT Diagnostic (2s capture)",
                                true,                  // enabled
                                false);                // not checkable
```

- [ ] **Step 4.2: Add `r == 203` handler**

In the same `showMenuAsync` lambda, after the `if (r == 202) { ... return; }` block (around line 4498), add:

```cpp
                            if (r == 203)
                            {
                                // QA-Md: 2-second diagnostic capture.
                                // Reset counters -> set capture flag ->
                                // sleep 2 s on message thread -> snapshot
                                // -> AlertWindow.  Audio must be playing
                                // for non-zero counts; user prompted to
                                // confirm playback first.
                                const bool ok = juce::AlertWindow::showOkCancelBox (
                                    juce::MessageBoxIconType::InfoIcon,
                                    "MT Diagnostic",
                                    "Start playback now, then click OK.\n"
                                    "Capture runs for 2 seconds.\n"
                                    "(Cancel to abort.)",
                                    "OK", "Cancel", nullptr, nullptr);
                                if (! ok)
                                    return;

                                RenderEngine::MtDiagnostic::reset();
                                RenderEngine::MtDiagnostic::gCaptureOn.store (true, std::memory_order_release);

                                juce::Thread::sleep (2000);

                                RenderEngine::MtDiagnostic::gCaptureOn.store (false, std::memory_order_release);
                                const auto snap = RenderEngine::MtDiagnostic::snapshot();

                                const bool      mtMode       = RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire);
                                const long long totalSubmits = snap.leavesSubmitted + snap.childSubmits;
                                const long long totalRun     = snap.mainThreadTasks + snap.workerTasks;
                                const double    mainPct      = totalRun > 0
                                    ? 100.0 * (double) snap.mainThreadTasks / (double) totalRun
                                    : 0.0;
                                const double    workerPct    = totalRun > 0
                                    ? 100.0 * (double) snap.workerTasks / (double) totalRun
                                    : 0.0;

                                juce::String body;
                                body
                                  << "Build: "
                                #if JUCE_DEBUG
                                  << "Debug"
                                #else
                                  << "Release"
                                #endif
                                  << "    MT mode: " << (mtMode ? "ON" : "OFF") << "\n"
                                  << "Capture window: 2 s\n\n"
                                  << "Blocks processed:    " << snap.blockCount       << "\n"
                                  << "Leaves submitted:    " << snap.leavesSubmitted  << "\n"
                                  << "Child submits:       " << snap.childSubmits     << "\n"
                                  << "Total submits:       " << totalSubmits          << "\n"
                                  << "Watchdog fires:      " << snap.watchdogFires    << "\n\n"
                                  << "Main-thread tasks:   " << snap.mainThreadTasks
                                      << "  (" << juce::String (mainPct,   1) << "%)\n"
                                  << "Worker tasks (all):  " << snap.workerTasks
                                      << "  (" << juce::String (workerPct, 1) << "%)\n"
                                  << "Total tasks run:     " << totalRun << "\n\n"
                                  << "Worker spin finds:   " << snap.workerSpinFinds  << "\n"
                                  << "Worker sleep finds:  " << snap.workerSleepFinds << "\n"
                                  << "Worker idle sleeps:  " << snap.workerIdleSleeps << "\n"
                                  << "Worker wakes:        " << snap.workerWakes      << "\n";

                                juce::AlertWindow::showAsync (
                                    juce::MessageBoxOptions()
                                        .withIconType (juce::MessageBoxIconType::InfoIcon)
                                        .withTitle ("MT Diagnostic Result")
                                        .withMessage (body)
                                        .withButton ("OK"),
                                    nullptr);
                                return;
                            }
```

- [ ] **Step 4.3: Verify `RenderEngineFlags.h` is included in StandaloneEditor.cpp**

Quick check via Read or Grep:

```
grep -n "RenderEngineFlags" Source/Standalone/StandaloneEditor.cpp
```

If not found, add `#include "../Engine/RenderEngineFlags.h"` near the other engine includes at the top of the file (path is relative to `Source/Standalone/`).

- [ ] **Step 4.4: Verify compiles — Jeff runs do_build.bat**

Both configs must build clean. Open Mixer page; hamburger menu now shows "Run MT Diagnostic (2s capture)".

- [ ] **Step 4.5: Commit Task 4**

```
git add Source/Standalone/StandaloneEditor.cpp
git commit -m "QA-Md Step 4: add 'Run MT Diagnostic' menu item to Mixer hamburger."
```

### Task 5: Smoke-test instrumentation in Release

**Files:** none (runtime verification)

- [ ] **Step 5.1: Jeff opens Release exe**

`build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe`. Load any session that produces audible output (a Layers tab + a few notes is sufficient). Confirm MT is ON via Mixer hamburger ("Multi-core Rendering" checked).

- [ ] **Step 5.2: Click Mixer hamburger → "Run MT Diagnostic (2s capture)"**

First AlertWindow prompts to start playback. Start playback in the Builder, then click OK. Wait 2 s. Result AlertWindow appears.

- [ ] **Step 5.3: Verify Release counter pattern**

Expected pattern (Release + MT ON, audio playing):
- `Blocks processed`: > 0 (typically 80-200 blocks at 44.1 kHz / 1024-sample blocks over 2 s).
- `Leaves submitted`: > 0 (one per leaf task per block).
- `Total submits`: > 0.
- `Watchdog fires`: 0 (or very low; healthy MT shouldn't time out).
- `Main-thread tasks`: > 0 BUT < total (main thread participates as worker; should be a fraction).
- `Worker tasks (all)`: > 0 (workers genuinely doing work).
- `Worker spin finds` and/or `Worker sleep finds`: > 0.
- `Worker wakes`: roughly equals `Worker idle sleeps` (every sleep eventually wakes).

Jeff: screenshot the AlertWindow text. Share in chat.

- [ ] **Step 5.4: If Release pattern doesn't match expected, abort and re-plan**

If Release shows `Worker tasks = 0` too, the bug is upstream of Debug-vs-Release and the diagnostic baseline is wrong. Surface to Jeff.

If Release pattern matches expected, proceed to Phase 2 with confidence in the instrument.

- [ ] **Step 5.5: Commit any minor instrumentation fixes if needed**

Skip this step if no fixes were needed.

---

## Phase 2 — Diagnose

### Task 6: Capture Debug counter pattern

**Files:** none (runtime data capture)

- [ ] **Step 6.1: Close Release, open Debug exe**

ASIO is exclusive — both can't run simultaneously. Open `build\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe`. Window title shows `[DEBUG]` suffix.

- [ ] **Step 6.2: Load same session as Step 5.1**

Same project = comparable counter values. Both configs share `Documents\BaySickDAW` settings + project files.

- [ ] **Step 6.3: Verify MT is ON (Mixer hamburger → Multi-core Rendering checked)**

If unchecked, click to enable. Setting persists to settings.xml.

- [ ] **Step 6.4: Run diagnostic, screenshot result**

Same procedure as Step 5.2. Share the screenshot in chat.

- [ ] **Step 6.5: Capture a SECOND Debug capture with MT OFF for control**

Toggle Mixer hamburger → Multi-core Rendering OFF. Run diagnostic again.

Expected: serial path skips the MT branch entirely → `gBlockCount = 0` (and all other counters 0). Confirms branch selection is working under Debug.

If Debug+MT-OFF shows non-zero `gBlockCount`, the branch decision itself is wrong (MT branch taken when atomic is false) — that becomes a major finding requiring re-plan.

### Task 7: Compare Release vs Debug + identify failure branch

**Files:** none (analysis)

- [ ] **Step 7.1: Side-by-side counter comparison**

Use the screenshots from Tasks 5 and 6. Compute:
- `R_workerPct = Release worker tasks / Release total tasks`
- `D_workerPct = Debug   worker tasks / Debug   total tasks`
- `R_watchdog  = Release watchdog fires / Release blocks`
- `D_watchdog  = Debug   watchdog fires / Debug   blocks`
- `R_wakeRatio = Release worker wakes / Release worker idle sleeps` (should be ≈ 1.0)
- `D_wakeRatio = Debug   worker wakes / Debug   worker idle sleeps`

- [ ] **Step 7.2: Apply branch decision tree**

| Pattern | Likely cause | Branch |
|---------|--------------|--------|
| `D_workerPct < 5%` AND `D_watchdog ≈ 0` AND `D_wakeRatio < 0.5` | Workers asleep, never waking — wake protocol issue | A |
| `D_watchdog > 50%` of blocks | Watchdog firing too often — 100 ms too tight | B |
| `D_workerPct < 5%` AND `D_watchdog ≈ 0` AND `D_wakeRatio ≈ 1.0` AND `Worker spin finds ≈ 0` | Workers spinning, finding nothing, going to sleep, but main thread monopolizes queue | C |
| Anything else | Unanticipated cause | D |

- [ ] **Step 7.3: Document the decision in the Carry-Over block**

Append at bottom of this plan file's Carry-Over section:

```
## Carry-Over (Phase 2 close)

- Diagnosis branch chosen: A / B / C / D
- Reasoning: <one paragraph: which counters justified the choice>
- Release baseline: blocks=N, workerPct=X%, mainPct=Y%, watchdog=Z, wakeRatio=W
- Debug captured: blocks=N, workerPct=X%, mainPct=Y%, watchdog=Z, wakeRatio=W
- Next action: execute Phase 3 Branch <X>
```

- [ ] **Step 7.4: Stop here, surface to Jeff for branch confirmation**

Show Jeff the comparison + chosen branch + reasoning. Jeff confirms or redirects. Do NOT proceed to Phase 3 without Jeff's confirmation.

If branch D, plan-mode + AskUserQuestion to re-plan Phase 3.

---

## Phase 3 — Fix (execute the branch confirmed in Task 7)

### Branch A — Workers Asleep / Wake Protocol Broken

**Hypothesis.** Workers reach `waker.wait(-1)` and stay there. `submit()`'s `if (activeWaiters > 0) signal()` race window may have a Debug-specific failure: the spin loop completes very fast in Release (~50 µs) but slow in Debug (~250 µs+). During Debug's slower spin, leaf tasks may complete before workers ever register as waiters. By the time submit runs, `activeWaiters == 0`, no signal sent, workers eventually finish spin and find queue empty, register as waiters, sleep — and never wake because no further submits happen until the next block.

**Fix.** Signal-on-submit unconditionally (don't gate on `activeWaiters > 0`). `juce::WaitableEvent::signal` is sticky; the next `wait()` returns immediately if no waiter is currently asleep. Trivial overhead.

- [ ] **Step A.1: Modify VibeThreadPool::submit to signal unconditionally**

Edit [`VibeThreadPool.cpp:50-71`](../../Source/Engine/VibeThreadPool.cpp).

Current:

```cpp
void VibeThreadPool::submit (RenderTask* task) noexcept
{
    if (task == nullptr)
        return;
    mImpl->queue.enqueue (task);
    if (mImpl->activeWaiters.load (std::memory_order_acquire) > 0)
        for (auto& waker : mImpl->wakers)
            waker->signal();
}
```

Replace with:

```cpp
void VibeThreadPool::submit (RenderTask* task) noexcept
{
    if (task == nullptr)
        return;
    mImpl->queue.enqueue (task);
    // 2026-05-08 (QA-Md Branch A): Signal one waker regardless of
    // activeWaiters count.  In Debug the spin loop is slow enough that
    // workers may register as waiters AFTER submit's check, missing the
    // signal and sleeping indefinitely.  juce::WaitableEvent::signal is
    // sticky, so signaling when no waiter is asleep just means the next
    // wait() returns immediately -- cheap and correct.  Signal one
    // (not all) to avoid stampede on multi-leaf submit cascades.
    mImpl->wakers[0]->signal();
}
```

(Picks `wakers[0]` arbitrarily — any of the N wakers works since we only need to wake A worker, and any worker can pop ANY task. Round-robin via atomic counter is a future optimization; not needed here.)

- [ ] **Step A.2: Build + verify diagnostic in Debug**

Jeff runs do_build.bat. Re-run diagnostic in Debug (Task 6 procedure).

Expected new pattern:
- `Worker tasks (all)`: > 50% of total.
- `Worker wakes ≈ Worker idle sleeps`: ratio close to 1.0.
- `Watchdog fires`: still 0.

If pattern matches, fix is verified.

- [ ] **Step A.3: Verify Release pattern unchanged (no regression)**

Jeff runs Release diagnostic. Counter pattern should match Task 5.3 baseline (workers may pick up MORE tasks if signal-always was a latent issue in Release too — that's a win, not a regression).

- [ ] **Step A.4: Commit Branch A fix**

```
git add Source/Engine/VibeThreadPool.cpp
git commit -m "QA-Md Branch A: signal one waker on every submit (Debug wake protocol fix)."
```

- [ ] **Step A.5: Skip to Phase 4**

### Branch B — Watchdog Too Aggressive Under Debug

**Hypothesis.** `kWatchdogTimeoutMillis = 100.0` is too tight for Debug's ~5x slowdown. Most blocks time out, dispatcher clears output, audio plays from arena's prior contents (stale-but-coherent data) → no audible silence but DSP meter shows main-thread-pegged-at-100ms-per-block.

**Fix.** Scale watchdog timeout by config. Compile-time `#if JUCE_DEBUG` ifdef widens the deadline under Debug only.

- [ ] **Step B.1: Modify kWatchdogTimeoutMillis to be config-aware**

Edit [`RenderEngineFlags.h:64`](../../Source/Engine/RenderEngineFlags.h).

Current:

```cpp
inline constexpr double kWatchdogTimeoutMillis = 100.0;
```

Replace with:

```cpp
// 2026-05-08 (QA-Md Branch B): Debug runs ~5x slower than Release; the
// 100 ms deadline is too aggressive under Debug and triggers spurious
// watchdog fires that silence the dispatcher.  Widen to 500 ms under
// Debug only (still well under any healthy block under Release-tier
// load; just won't false-positive when Debug's per-task cost is inflated).
#if JUCE_DEBUG
inline constexpr double kWatchdogTimeoutMillis = 500.0;
#else
inline constexpr double kWatchdogTimeoutMillis = 100.0;
#endif
```

- [ ] **Step B.2: Build + re-diagnose in Debug**

do_build.bat → Debug diagnostic.

Expected: `Watchdog fires` drops to ~0; `Worker tasks` rises substantially.

- [ ] **Step B.3: Verify Release pattern unchanged**

Release diagnostic identical to Task 5.3 baseline (same code in Release, ifdef'd out).

- [ ] **Step B.4: Commit Branch B fix**

```
git add Source/Engine/RenderEngineFlags.h
git commit -m "QA-Md Branch B: widen watchdog timeout to 500 ms under Debug."
```

- [ ] **Step B.5: Skip to Phase 4**

### Branch C — Audio Thread Starves Workers

**Hypothesis.** Workers DO wake up and DO check the queue, but the audio thread (also running `runUntilOrTimeout`) is consistently faster at popping. Net result: workers wake → tryPop returns nothing → workers sleep again. Main thread eats everything.

**Fix.** Less obvious. Branch C is the least likely failure mode given the architecture (workers should pick up tasks faster than the slower audio thread under Debug). If diagnosis lands here, multiple fix shapes are plausible.

- [ ] **Step C.1: Surface to Jeff for fix design**

AskUserQuestion choices:
- (a) Yield-before-pop in `runUntilOrTimeout` — gives workers first crack each iteration.
- (b) Don't change pump strategy; investigate deeper (queue contention? cache thrash? Debug-specific moodycamel scheduling?).
- (c) Skip main-thread participation entirely under Debug (`#if JUCE_DEBUG` gate around `runUntilOrTimeout`'s pop branch — main thread only waits, workers do all the work).
- (d) Something else.

Do NOT implement without Jeff's confirmation.

### Branch D — Other Cause

Plan-mode + AskUserQuestion to re-plan Phase 3 from diagnostic findings.

---

## Phase 4 — Cleanup + Close

### Task 8: Decide instrumentation fate

**Files:** none (decision)

- [ ] **Step 8.1: Surface options to Jeff**

The instrumentation is ~150 lines (counters + menu item). Three options:
- **(a) Keep as permanent diagnostic.** Useful for future MT-related batches. Cost: 11 atomic counters always-allocated, ~0 hot-path cost when capture off (one relaxed-load per increment site, JIT'd to plain mov on x86).
- **(b) Remove entirely.** Clean closeout. Risk: re-add cost if MT issues recur.
- **(c) Compile-flag-gate.** Wrap counters + menu item in `#if BAYSICKDAW_MT_DIAGNOSTIC` — off by default in Release, on in Debug.

Recommend (a) — cost is genuinely negligible and the instrumentation has value beyond this batch. Jeff picks.

### Task 9: Apply Jeff's instrumentation decision

- [ ] **Step 9.1: Execute the chosen path**

If (b): reverse Tasks 1-4 (single revert commit).
If (c): wrap with `#if BAYSICKDAW_MT_DIAGNOSTIC` / `#endif`. Add `add_compile_definitions(BAYSICKDAW_MT_DIAGNOSTIC)` to the Debug-config branch in CMakeLists.txt.
If (a): no code change.

- [ ] **Step 9.2: Build + verify (if any code change)**

do_build.bat → both configs clean.

- [ ] **Step 9.3: Commit (if any change)**

### Task 10: Run /draft-doc batch-close + /review-batch + commit

Per CLAUDE.md Main Plan §0 Agent Orchestration Rules "Batch close (mandatory sequence)".

- [ ] **Step 10.1: `/draft-doc batch-close`**

Dispatch the doc-drafter agent in `batch-close` mode with QA-Md context:
- Batch: QA-Md
- Bucket: Cross-cutting Infrastructure
- Done: list every Task completed
- Found along the way: branch chosen + counter readings
- What was done about each finding: the fix + commit hashes
- Carry-forward contradictions: confirm carry-forward §1 "MT engine is production, default ON" is now true for BOTH Release AND Debug.
- Files touched: list
- Commits: list

Returns proposed text in code block.

- [ ] **Step 10.2: Apply doc draft**

Edit [Implemented Work Log.md](../Implemented Work Log.md) — append new `### YYYY-MM-DD HH:MM PT — QA-Md — <summary>` entry from doc-drafter output.

- [ ] **Step 10.3: `/review-batch QA-Md`**

Dispatch batch-code-reviewer with QA-Md plan + diff. Address any BLOCKER findings before proceeding. NEEDS-FIX items: address inline. NIT items: defer or address.

- [ ] **Step 10.4: Update Main Plan §5 QA-Md entry**

Annotate the entry if Phase 2 surfaced findings that need routing per Rule 3. If diagnosis-fix was clean, no annotation needed beyond marking the batch closed.

- [ ] **Step 10.5: Append §9 Forks entry if scope changed**

If diagnosis surfaced anything beyond expected scope (e.g., a separate finding routed to a future batch), append a §9 Forks entry per Rule 3.

- [ ] **Step 10.6: Final commit**

```
git add "Plans & Specs/Implemented Work Log.md" "Plans & Specs/Main Plan.md"
git commit -m "QA-Md close: <one-line summary>"
```

---

## Verification Summary

**Per-batch verification (per Main Plan §7):**
- ✅ do_build.bat passes both configs after every Task with code changes.
- ✅ Diagnostic AlertWindow appears in both configs with sensible counters.
- ✅ Debug + MT diagnostic shows worker activity > 50% post-fix.
- ✅ Release + MT counter pattern unchanged (no regression).
- ✅ Audio plays cleanly under Debug + MT (already does, must not regress).
- ✅ MT toggle hot-swap still works under both configs (settings.xml persistence + UI checkmark sync).

**End-to-end test:**
1. Open Debug exe.
2. Load a heavy session.
3. Mixer hamburger → Run MT Diagnostic → counter shows balanced worker/main split.
4. Play audio for 30 s. No clicks, no dropouts.
5. Toggle MT off + on. No audio glitch.
6. Quit + relaunch. MT setting restored from settings.xml. Diagnostic still works.

---

## Carry-Over

(Append per CLAUDE.md §0 Rule 2 at every stopping point.)

```
## Carry-Over (YYYY-MM-DD HH:MM PT)

- Completed: <task IDs>
- In-flight: <task ID + state>
- Assumptions changed: <any>
- Resume action: <literal next thing>
- Implemented-work entry needed: <one-line>
```
