# BaySickDAW Security Audit - Tier 1 (our source only)

**Date:** 2026-08-10
**Batch:** QA-Cleanup (`spry-tidying-pika`), Task 7
**Agent:** `security-auditor` (built in this batch, Task 6) via `/audit-security`
**CL-289 part:** Part 2 (file-parser audit), OUR parsers under `Source/` only.

> **Tiering correction 2026-08-10.** This report originally called itself
> "Tier 1 = our source" with vendored code as "Tier 2". That split was invented
> here; CL-289 tiers by RELEASE PHASE and its Tier 1 has FOUR parts - CVE scan,
> file-parser audit, DLL safety, save-file format audit - with vendored parsers
> and XXE both inside Tier 1. This report is one part of one of those four.

**The complete CL-289 Tier 1 set:**
- `security-audit-2026-08-10.md` (this file) - Part 2, our parsers
- `security-audit-2026-08-10-part2-vendored-parsers.md` - Part 2 (vendored) + Part 4 (XXE / billion-laughs)
- `security-audit-2026-08-10-part3-cve-and-dll.md` - Part 1 (CVE scan) + Part 3 (DLL safety)

Tier 2 (QA-Updater network audit) is not runnable until that batch exists.
Tier 3 (cloud) is post-V1.
**Status:** ALL TEN FIXED, same batch. Jeff 2026-08-10: *"Track the 3 lows to
confirm what is actually going on there and if there is something to fix than
fix it and fix the 7 others."* The three LOWs were re-read in the parent session
first and all three were real. Build green after the fixes (six exit codes 0,
four link lines, zero errors, zero warnings).

**Two are PARTIAL by nature and are recorded as such, not as closed:**

- **MEDIUM-6** - the downgrade half is closed (the installer now resolves the
  redirect chain itself and refuses any non-HTTPS hop). The INTEGRITY half is
  not: validation is still the exact-byte-length check, which a padded file
  passes. The complete fix is a published SHA-256 per asset verified before
  extraction, which needs hashes published alongside the GitHub release. An
  empty hash field would be a check that checks nothing, so none was added.
- **MEDIUM-7** - a wedged helper still costs 15 s ONCE; the latch stops it being
  paid per action. The underlying conflation (one constant serving as both the
  startup handshake timeout and the per-write timeout) is untouched.

**HIGH-1 carries a deliberate behavior change:** a project referencing a plugin
that is not in the user's added list no longer loads that plugin. It is refused
and named in the missing-file dialog; the project still opens. There is no
version of this fix that preserves the old "load whatever path the file names"
behavior, which is the whole finding.

**The framing in the first draft of this report was wrong** and Jeff called it:
it described HIGH-1 as "deliberate". What was deliberate is rebuilding from the
blob instead of the added list - that choice was made and written down. Loading
an unvalidated filesystem path was never a decision; it was a consequence nobody
looked at. Calling it deliberate made it read as defended.

---

## Parent-session verification

Per `feedback_verify_subagent_finding_premise`, every finding below that could be
checked from source was re-read in the parent session before being relayed. An
agent finding is a lead, not a fact.

| # | Finding | Premise verified? | How |
|---|---|---|---|
| 1 | Plugin path loaded from project XML | **CONFIRMED** | `HostedPlugin.cpp:512` reads `<PLUGIN>` from the blob; `HostedPluginEffect.cpp:189-193` carries the comment "Rebuild from the description in the blob rather than the added list"; `setPlugin` -> `createPluginInstance (mDesc, ...)` at `:163`. The scanned/added list is genuinely not consulted. |
| 2 | `mPendingState` data race | **CONFIRMED, stronger than reported** | `SandboxedPluginClient.h:238` declares it with no lock. The comment two lines BELOW it - "Written on the reader thread, read from the message thread -- a bare String here was a data race" - protects `mLastError`. The identical hazard was recognised and fixed for the neighbour and missed here. Writer `:538` (`replaceAll`, IPC thread), reader `:382` (`dest = mPendingState`, message thread). |
| 3 | SFZ `sw_down` / `sw_up` out-of-bounds | **CONFIRMED** | `mSwDownHeld` is `std::array<bool, 128>` (`BaySickPlayerDSP.h:134`). Indexed at `:624-625` guarded only by `!= -1`. `sfzNote` (`:456`) returns a bare `getIntValue()`; `noteNameToMidi` (`:496`) computes `12 * (octave + 1) + semitone` with an unbounded octave. The siblings at `:395-396` and `:416` DO clamp - so this is an inconsistency, not a policy. |
| 4 | SFZ `seq_length` divide-by-zero | **CONFIRMED** | `:638-639` sets `hasRR` if ANY candidate has `roundRobinTotal > 0`; `:650` then takes `rrTotal` from `candidates[0]`, which may be 0; `:651` computes `counter % rrTotal`. |
| 5 | Buffer sized from the file's own header | **CONFIRMED** | `BaySickPlayerDSP.cpp:578-581` derives the 60-second cap from `reader->sampleRate`, which comes from the file, then truncates with `(int)`. `SlideSampleCache.cpp:32` has no cap at all. |
| 6 | Download integrity / redirect downgrade | **PARTIALLY** - our half confirmed | `CoreLibraryInstaller.h:62` pins HTTPS, `:648` allows 10 redirects, `:775` validates by exact byte count only. The claim that JUCE follows an HTTPS->HTTP redirect is a vendored-code claim and was NOT re-verified in the parent session. |
| 7 | 15 s message-thread stall on a wedged helper | **PARTIALLY** - our half confirmed | `SandboxedPluginClient.cpp:56` sets the 15 s timeout. The audio thread is genuinely insulated (`:236-241` documents it). The claim about JUCE's ping-thread recovery at ~16 s was NOT re-verified. |
| 8-10 | LOW findings | Not re-verified | Low severity; recorded as leads. |

**Not verifiable from source alone:** nothing here was demonstrated against a
running binary. No crafted WAV, SFZ or project was fed to the app. Findings 3,
4 and 5 are cheap to prove empirically if we want them proven before fixing.

---

## Findings

### HIGH-1 - A project file can name any `.vst3` on disk or on a network share, and it loads without asking

**Where:** `Hosting/HostedPlugin.cpp:512` -> `Hosting/HostedPluginEffect.cpp:192`
-> `HostedPlugin.cpp:163`.

Restoring a plugin does not look it up in the list the user scanned and added.
It reads a `<PLUGIN>` element out of the saved blob and rebuilds from whatever
that element says. This is deliberate - the comments at `HostedPlugin.cpp:22-25`
and `EngineRig.cpp:454-456` both state the reason: a project referencing a
plugin the user has since un-added must still load. The consequence was not
intended: `fileOrIdentifier` is a full filesystem path and nothing checks it.

**Exploit path:** a shared project folder, template, or forum download has one
effect slot whose plugin description points at the attacker's `.vst3`, possibly
on a UNC share. The project opens, nothing is displayed, no dialog names the
file, and their code is running with the user's privileges.

**Note on "sandboxed":** `SandboxedPluginClient` is about ARCHITECTURE (x86 vs
x64), not security. The helper launches via plain `CreateProcess` - no job
object, no restricted token, no integrity drop - so the bridged route is no
safer than in-process, and a 64-bit Plugins tab is pinned unbridged anyway
(`EngineRig.cpp:479-480`).

**Fix direction:** on restore, require the description to match the scanned/added
list; when it does not, prompt with the path spelled out rather than loading
silently. Keeps the stated intent (un-added plugins still load) while removing
the silent-arbitrary-DLL case.

### HIGH-2 - The bridged plugin's saved-state reply is copied without a lock

**Where:** `Hosting/SandboxedPluginClient.h:238`, writer `.cpp:538`, reader
`.cpp:381-382`.

Every other cross-thread member in this class was given its own
`CriticalSection` - the error string, parameter list, program name, touched
parameters. `mPendingState` is the one that was missed, and it is the only one
carrying a large helper-controlled buffer.

**Exploit path:** the plugin answers the host's GetState with a large blob, then
immediately sends a second tiny one, so `replaceAll` reallocates while the
message thread is copying. The helper controls the timing, so this is reachable
on demand. The user sees a crash on Save; when it does not crash, unrelated
process memory can end up base64-encoded into the project file
(`HostedPlugin.cpp:507`) and the plugin's saved state is silently wrong.

### MEDIUM-3 - SFZ keyswitch value indexes a 128-slot array unchecked, on the audio thread

**Where:** `BaySickPlayer/BaySickPlayerDSP.cpp:624-625`.

`sw_down=9999` reads ~9,999 bytes past a 128-byte array; `sw_down=-5000` reads
far below it. The same values are correctly clamped at `:395-396`, `:416`, and
all four public keyswitch entry points (`:668`, `:674`, `:681`, `:697`). Only
these two lines were missed.

**Exploit path:** a sample pack, or a project referencing one, with one bad
`sw_down=` line. Instant crash on the first note, or - for a moderate value
landing in nearby heap - an instrument that picks the wrong articulation
depending on unrelated memory.

### MEDIUM-4 - SFZ `seq_length` of zero divides by zero on the audio thread

**Where:** `BaySickPlayer/BaySickPlayerDSP.cpp:639, 650-651`.

The comment at `:618-622` shows this crash was hit before (2026-05-27) and was
addressed by making keyswitch filtering isolate the candidate pools. That works
when the SFZ uses keyswitches consistently; it is not a guard. Two regions on
the same key/velocity/group, the first with no `seq_length` and the second with
`seq_length=4`, reproduces it.

**Exploit path:** app dies instantly on that note. Integer divide-by-zero, not
memory corruption.

### MEDIUM-5 - Audio buffers sized from the file's own header, with a cap the header defeats

**Where:** `BaySickPlayer/BaySickPlayerDSP.cpp:578-581`;
`SlideSampler/SlideSampleCache.cpp:32` (no cap at all).

The 60-second ceiling is computed from the sample rate the file declares. A file
can declare an enormous sample rate and data-chunk length while being ~60 bytes
on disk; the cap evaluates to something astronomical and the `(int)` cast
truncates.

**Exploit path:** app freezes and dies trying to reserve gigabytes from a tiny
file. Ranked as a crash rather than corruption: the wrapped sizes land in the
enormous range and the allocator throws.

**The contrast worth copying:** `PluginProcessor.cpp:5703-5718` does this job
correctly - byte total in 64-bit, real threshold check, overflow caught with a
`<= 0` test.

### MEDIUM-6 - Core Library download has no integrity check

**Where:** `CoreLibraryInstaller.h:62` (HTTPS pinned), `:648` (10 redirects),
`:775` (validates by exact byte count only).

Byte-count matching is a corruption check, not a security one - padding a file
to an exact length is trivial. Ranked MEDIUM because the extracted payload is
not executed by the app; this is planting files where the user browses, not code
execution. Zip-slip is genuinely blocked (see Clean, below).

**Fix direction:** a published SHA-256 per asset, checked before extraction.
That closes it regardless of what the transport does.

### MEDIUM-7 - A hung bridged plugin freezes the UI for 15 s per action

**Where:** `SandboxedPluginClient.cpp:56`, `:111`.

The audio thread was deliberately and correctly kept off this pipe (`:236-241`
documents the hazard). The message thread was not: `setState`, `setParameter`,
`prepare`, `openEditor`, `closeEditor` and the destructor all go through it.
Deleting a wedged plugin pays it twice (`:82`, `:84`). Audio keeps playing.

### LOW-8 - `library:` / `mysamples:` prefixes do not confine anything

`SampleLibrary.cpp:248-278`. `getChildFile` resolves `..` and passes absolutes
through, so `library:../../../../Windows/win.ini` escapes the root.

**Recorded as informational, not a boundary break** - the boundary does not
exist anywhere else either (`PluginProcessor.cpp:7214` and `SampleLibrary.cpp:297`
pass absolute stored paths through BY DESIGN). A project can already reference
any file on the machine. What is reachable today is reading an arbitrary file
and failing to decode it as audio. Worth recording so that if a WRITE ever uses
a resolved reference, this is already understood to be unconstrained.

### LOW-9 - SFZ `#include` has no cycle detection

Depth-capped (6 in `SlideRegionMap.cpp:70`, 4 in `BaySickGuitarsProcessor.cpp:553`)
but not breadth-capped, and no self-include detection. 100 includes at depth 6
is 100^6 reads. Sample-pack-shaped denial of service.

### LOW-10 - Helper's reported editor size is unbounded and sticks as a window minimum

`HostedPlugin.cpp:707-718` validates only `w <= 0 || h <= 0`. Feeds
`setResizeFloor`, which persists (`WorkspaceWindow.cpp:274-280`) and overrides
the workspace clamp (`:342-343`). Cosmetic, one window, recoverable.

### Vendored, noted not audited

`juce_InterprocessConnection.cpp:361-365` allocates a message buffer sized purely
from the length the helper wrote into the header, with no ceiling. Our bridge
layer is clean - it never trusts the declared length - but JUCE has already
allocated by the time our code runs. Mentioned because the bridge is the only
thing reaching it with untrusted input.

---

## Checked and clean

This section is what makes the next audit cheaper.

- **Project XML indices are consistently bounds-checked.** Every page factory
  rejects an out-of-range or already-used `pageIndex` and returns null; the
  restore loop `continue`s on null (`StandaloneEditor.cpp:2519, 2530, 2541, 2759`).
  Mixer strips use `std::map<int,...>` plus explicit bounds
  (`MixerPage.cpp:2140, 2523, 2780`); name/order restore only touches entries
  that already exist.
- **PatternManager's load path.** Every row accessor guards against
  `kMaxArrangementRows` before subscripting, so the unclamped `trackRow` read at
  `:1775` cannot reach an array. `automationLaneFromValueTree` sorts points at
  load (`:1198`) so the audio-thread evaluator's sorted-order invariant holds.
  Out-of-range enum values hit safe `default:` branches.
- **Rack/effect restore** drops unrecognised kind labels and range-checks the
  computed channel id (`BaySickGraph.cpp:2117-2129`); slot index bounds-checked
  (`EffectRack.cpp:862-863`).
- **Corrupt blobs fail loudly.** `StandaloneEditor.cpp:17204-17218` and
  `PagePresetIO.cpp:143-157` both check the JUCE binary magic number after
  base64-decode and route failures to the missing-file report rather than
  calling `setStateInformation` on junk.
- **MIDI Learn / drum triggers** bounds-check the drum index and clamp note and
  channel; the registry keys a string map with no array indexing.
- **The bridge's message parsing is well built.** Body length derived from the
  buffer JUCE actually delivered, never from `Header.payloadBytes` (which is not
  read host-side at all). Every fixed payload length-checked before its `memcpy`.
- **Bridge shared memory.** Host never reads a size or count from the shared
  header. Channel pointers bounds-checked. Mapping names are a fresh `Uuid` per
  prepare, so they cannot be squatted.
- **Helper launch.** Absolute path from the running exe's own directory, fixed
  filename, `existsAsFile` gate. No PATH search, no shell.
- **Zip extraction is safe.** JUCE rejects entries resolving outside the target
  and refuses paths through a symlink; the installer passes
  `FollowSymlinks::no` (`CoreLibraryInstaller.h:862`) and never trusts the
  archive's own folder name (`:814-823`).
- **IR and NAM loading.** Every user-IR path probes with an `AudioFormatReader`
  first; no allocation sized from a header field in our code. Both NAM load
  sites wrap the parse in `try`/`catch` including a bare `catch (...)`.
- **Project name validation** rejects illegal characters, control characters,
  Windows reserved device names and over-length names on every entry point.
- **No credentials, confirmed rather than assumed.** A sweep for API keys,
  secrets, passwords, bearer tokens and AWS/GitHub key prefixes returned only
  unrelated "lifetime token" comments. Both file loggers are `#if JUCE_DEBUG`
  with no-op release stubs.

---

## Not covered - the honest gap list

- **`libs/` entirely.** Most relevant: `NeuralAmpModelerCore` does the actual
  `.nam` JSON parse where layer sizes and weight counts drive allocation - our
  only defence is the `catch (...)`. **`libs/sfizz` has its own SFZ parser**,
  used by Guitars / Basses / Rusty Drums; it is a completely different parser
  from the one audited here, and NONE of findings 3, 4 or 9 should be assumed to
  apply to it or to have been checked against it.
- **`juce/` beyond four targeted reads.** In particular `juce::XmlDocument` was
  not checked for entity expansion (billion laughs) or stack exhaustion on deeply
  nested documents - both plausible against a hostile `project.xml`.
- **`Source/Hosting/Helper/`** - the helper's own side.
- **Cross-process window reparenting** - what a hostile child window can do to
  the parent's message loop.
- **`plugins.xml` and the plugin scan path** (`PluginManager.cpp:539-584`,
  `architectureOf` reads PE headers off disk at `:290-334`).
- **Per-effect `setStateInformation` bodies** - ~40 DSP classes each parse their
  own restored state; the dispatcher was read, the bodies were not.
- **Not read at all:** `BaySickVocal/`, `DenoiseDSP`'s base64 profile restore,
  `Mp3Writer`, `AudioFileRecorder`, `MidiRecorder`, `EffectPresetIO.cpp`,
  `UserFileSave.h`, `SharedUI.cpp`, `PianoRoll.cpp`, `EventEditor.cpp`,
  `EffectEditorPanels.cpp`.
- **Sampled, not exhaustive:** the remaining ~18,000 lines of
  `StandaloneEditor.cpp` and all 10,668 lines of `BuilderPage.cpp` were grepped
  for index-shaped XML reads and array subscripts, not read line by line.
- **No dynamic testing.** Every finding is a code-reading argument.
