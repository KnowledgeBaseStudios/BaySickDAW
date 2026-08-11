#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// SafeSfzKit - pre-flight gate on an SFZ kit before it reaches sfizz.
//
// SECURITY (QA-Cleanup 2026-08-10, CL-289 Tier-1 parts 1 + 2).
//
// An SFZ is a TEXT file plus whatever samples it names, and a kit path is
// stored in the project - so a shared project folder can carry its own kit and
// point at it.  sfizz's OPCODE layer is well built (typed, per-opcode bounds,
// explicit reject rather than clamp - `sw_down=9999` is dropped, `seq_length`
// cannot be 0).  Its TEXT preprocessor and its FILE POOL are where the problems
// are, and its bundled decoders are a second, independent attack surface from
// JUCE's.
//
// Everything below is checked BEFORE `loadSfzFile` / `loadSfzString`, because
// after that call it is too late - the hangs never return and the overflow has
// already happened.  Our own kit scanners already walk this exact file set for
// `set_cc` defaults; they just used to run AFTER the load.
//
// WHAT THIS CLOSES
//
// * MACRO SELF-REFERENCE (sfizz Parser.cpp:504-558).  `#define $NAME value`
//   expansion re-sweeps while it made any substitution, with no iteration cap
//   and no self-reference check.  `#define $A $A` re-expands to identical text
//   forever; `#define $A $A$A` doubles it each pass and exhausts memory in ~30.
//   Two lines in a sample pack freeze the app on the UI thread.
//   -> refuse any #define whose value contains '$'.  Nothing legitimate needs a
//      macro that expands to another macro.
//
// * INCLUDE CYCLES (sfizz Parser.h:104-105).  sfizz HAS a recursive-include
//   guard and it is OFF by default (`_recursiveIncludeGuardEnabled = false`),
//   with no public way to switch it on - `setRecursiveIncludeGuardEnabled` is
//   never called anywhere.  Only a nesting cap of 32 applies, and a depth cap
//   is not a work cap: a file with two `#include "self.sfz"` lines is 2^31 file
//   opens.  -> visited set + a total-file budget.
//
// * OPCODE INDEX BLOWUP (sfizz Region.cpp:24-36, used at :576, :721, :1248).
//   The number inside an opcode NAME (`lfo1_freq`) grows a vector with no
//   ceiling other than the 16-bit field's 65,535.  A 250 KB file of
//   `lfo65535_freq=1` regions asks for ~200 GB.  -> cap the index.
//
// * CVE-2023-47212 / CVE-2023-45681 (stb_vorbis 1.22, CVSS 9.8), vendored at
//   sfizz/external/st_audiofile/thirdparty/stb_vorbis.  An Ogg comment count is
//   multiplied by 8 and passed to an allocator taking an `int`; ~268M wraps to
//   a small positive, then that many pointers are written.  Heap overflow from
//   a `.ogg` in a sample pack.  -> we ship no Ogg content, so refuse kits that
//   reference one.  The complete fix is a version bump inside vendored sfizz,
//   which any sfizz update would revert.
//
// * UNMAINTAINED AIFF PARSER (libaiff, no version marker, last upstream
//   activity over a decade ago, reachable from any pack).  No published CVEs -
//   which for an unfuzzed C parser means nobody has looked.  -> we ship no AIFF
//   content either; refuse it for the same cost.
//
// * DECLARED-SIZE ALLOCATION (sfizz FilePool.cpp:100-107).  sfizz sizes its
//   buffer from the frame count the audio file declares, with no comparison to
//   the real file and no ceiling, on a background thread inside a `noexcept`
//   function - so `std::bad_alloc` becomes `std::terminate` and the process
//   dies mid-song with no dialog.  -> reject a referenced sample whose declared
//   length is impossible for its size on disk.
//
// * UNC SAMPLE PATHS (sfizz Region.cpp:62-76, Synth.cpp:427-430).  A
//   `sample=\\server\share\x.wav` makes Windows offer the user's credentials to
//   that host when the note plays - the same mechanism as the project-XML XXE.
//   -> refuse `\\` in sample= / default_path=.
//
// WHAT THIS DOES NOT CLOSE.  The general `../` escape: an SFZ can still name a
// file elsewhere on the machine, exactly as a project can.  Confining kits to
// the Core Library root is a product decision, not a code one, and the app
// already resolves absolute paths from projects by design.
// ─────────────────────────────────────────────────────────────────────────────
namespace SafeSfzKit
{
    inline constexpr int kMaxIncludeDepth = 8;

    // RAISED 256 -> 4096 (2026-08-11).  MEASURED, not guessed: with the include
    // resolution corrected to match sfizz, the walk finally reaches the whole
    // kit, and the largest kit we ship - Big Rusty Drums `01-full.sfz` - takes
    // 529 visits.  At 256 it was REFUSED, which is the same false-refusal
    // failure that an earlier version of this gate hit on 22 kits.
    //
    // The count is VISITS, not distinct files (a shared file legitimately
    // included from several places is walked once per path), so it runs ahead of
    // the file count - 02-basic.sfz is 82 distinct files and 104 visits.
    //
    // Headroom is deliberate and the exact value is not delicate: this bounds
    // exponential FAN-OUT, where the numbers are astronomical rather than
    // marginal.  A file including ten others at depth 8 is 10^8 opens, which
    // trips any ceiling in this range immediately; the depth cap alone already
    // bounds a binary fan-out to ~511.  So the cap only has to sit above real
    // content, and 8x the worst kit we ship is a sane place for it.
    inline constexpr int kMaxIncludeFiles = 4096;

    inline constexpr int kMaxOpcodeIndex  = 512;

    // Per-FILE ceiling.  The include budget counts files, not bytes, so without
    // this a kit could be 4096 x however large each one is.  16 MB is orders
    // above any real SFZ (ours run to a few hundred KB) and small enough that
    // 4096 of them cannot be used as an allocation attack.
    inline constexpr juce::int64 kMaxFileBytes = 16 * 1024 * 1024;

    // sfizz's own decoders handle these; the two we refuse are the ones with a
    // known-vulnerable or unmaintained parser behind them.
    inline bool isRefusedSampleExtension (const juce::String& path)
    {
        return path.endsWithIgnoreCase (".ogg")
            || path.endsWithIgnoreCase (".aif")
            || path.endsWithIgnoreCase (".aiff");
    }

    // Empty return = accept.  Non-empty = human-readable reason to refuse.
    inline juce::String rejectReason (const juce::File& sfzPath)
    {
        if (! sfzPath.existsAsFile())
            return "the kit file is missing";

        // THE BASE DIRECTORY IS THE TOP-LEVEL FILE'S, FOR EVERY INCLUDE AT EVERY
        // DEPTH.  This is not a style choice, it is what sfizz does, and a gate
        // that walks a different graph than the parser it guards is not a gate.
        //
        // sfizz: Parser.cpp:70-74 resolves `_originalDirectory / path`, and
        // `_originalDirectory` is assigned ONCE, from the first file, under an
        // `if (_pathsIncluded.empty())`.  It is never reassigned.
        //
        // This used to resolve against the INCLUDING file's parent, and the miss
        // was silent (see the existsAsFile early-out below), so on the shipped
        // Big Rusty Drums kit the gate opened 20 files where sfizz opens 82, and
        // saw 16 of 1468 `sample=` opcodes.  Everything below depth 2 was
        // unguarded: an attacker needed no parser trick, just one more directory
        // of nesting.  BaySickRustyDrumsProcessor.cpp:835 already had this right
        // ("matching sfizz's #include resolution semantics") while this did not.
        const auto root = sfzPath.getParentDirectory();

        // ANCESTOR chain, not a global visited set.  Real kits legitimately
        // include one shared file from several places - Big Rusty Drums and
        // Black&Blue Basses both do - and that is a DAG, not a cycle.  A first
        // version of this refused 22 of our OWN shipped kits by treating any
        // repeat visit as a loop.  A genuine cycle is a file that appears in its
        // own ancestor chain; unbounded FAN-OUT is what the budget is for.
        juce::StringArray ancestors;
        int budget = kMaxIncludeFiles;
        juce::String failure;

        // $macro definitions, accumulated across the WHOLE walk rather than per
        // file, because sfizz's `_definitions` is a Parser member that outlives
        // each included file.
        std::map<juce::String, juce::String> macros;

        // sfizz expands macros in opcode NAMES, opcode VALUES and INCLUDE PATHS
        // (Parser.cpp:206 expands the include path before resolving it), so a
        // gate that pattern-matches raw text sees none of what actually loads:
        // `#define $X 65535` + `lfo${X}_freq=1` hid the index rule, and
        // `#define $P ../../../` + `#include "$Pevil.sfz"` hid an escape.
        //
        // ONE substitution pass is complete here, and that is guaranteed rather
        // than hoped for: a macro whose value contains '$' is refused below, so
        // no expansion can introduce another macro and there is nothing to
        // re-sweep.  That is also why this cannot loop the way sfizz's own
        // uncapped expander does.
        const auto expand = [&macros] (const juce::String& in)
        {
            if (! in.containsChar ('$') || macros.empty())
                return in;

            // Longest name first, so `$AB` is never eaten by `$A`.
            std::vector<const std::pair<const juce::String, juce::String>*> byLen;
            for (const auto& m : macros) byLen.push_back (&m);
            std::sort (byLen.begin(), byLen.end(), [] (auto* a, auto* b)
                       { return a->first.length() > b->first.length(); });

            auto out = in;
            for (const auto* m : byLen)
                out = out.replace (m->first, m->second);

            return out;
        };

        std::function<void (const juce::File&, int)> walk;
        walk = [&] (const juce::File& f, int depth)
        {
            if (failure.isNotEmpty()) return;

            if (depth > kMaxIncludeDepth)
            { failure = "its #include chain is nested too deeply"; return; }

            if (budget <= 0)
            { failure = "it pulls in too many files through #include"; return; }

            const auto key = f.getFullPathName();
            if (ancestors.contains (key))
            { failure = "its #include chain refers back to itself"; return; }

            if (! f.existsAsFile()) return;   // a missing include is sfizz's to report

            ancestors.add (key);
            --budget;

            // READ AS BYTES, NOT AS A STRING.  juce::String is NUL-terminated, so
            // loadFileAsString() silently truncates at the first NUL byte - while
            // sfizz's reader treats NUL as ordinary data and keeps parsing.  One
            // NUL near the top of a file therefore hid the entire remainder of it
            // from this gate.  An SFZ is a text format; a NUL in one is either
            // corruption or an evasion attempt, and neither is worth loading.
            // SIZE FIRST, before the file is read at all.  Two reasons, and the
            // second is memory-unsafe rather than merely slow:
            //   * `(int) getSize()` truncates past 2 GB and goes NEGATIVE, which
            //     skips the NUL scan below and then hands String::fromUTF8 a
            //     negative length - it falls back to treating the buffer as
            //     NUL-terminated, and a MemoryBlock is not, so that is a heap
            //     over-read.
            //   * the include budget bounds the NUMBER of files, not their size,
            //     so 4096 visits x an unbounded file x ~3 copies (MemoryBlock,
            //     String, StringArray) is the DoS the budget was supposed to stop.
            // An SFZ is a text file; the largest we ship is a few hundred KB.
            if (f.getSize() > kMaxFileBytes)
            { failure = "one of its files is too large to be an SFZ"; return; }

            juce::MemoryBlock raw8;
            if (! f.loadFileAsData (raw8))
                return;

            const auto* bytes = static_cast<const char*> (raw8.getData());
            const auto  nbyte = (int) raw8.getSize();

            for (int i = 0; i < nbyte; ++i)
                if (bytes[i] == 0)
                { failure = "it contains binary data where text was expected"; return; }

            juce::StringArray lines;
            lines.addLines (juce::String::fromUTF8 (bytes, nbyte));

            for (const auto& rawLine : lines)
            {
                if (failure.isNotEmpty()) return;

                // SFZ comments are "//" to end of line, and they must be
                // stripped BEFORE any scanning below.  A first version did not,
                // and refused two perfectly ordinary lines: a banner comment
                // carrying a number and an '=' ("// recorded at 44100 Hz,
                // tuning = 440") tripped the opcode-index rule, and a
                // commented-out "//sample=old.ogg" tripped the extension rule.
                //
                // Safe for the UNC check below: a network path is written
                // "\\server\..." in the raw text and contains no literal "//"
                // until after the backslash normalisation, which happens later.
                auto line = rawLine;
                if (const int comment = line.indexOf ("//"); comment >= 0)
                    line = line.substring (0, comment);

                // A TAB IS A SEPARATOR, NOT PART OF A TOKEN.  sfizz skips
                // " \t" between every token (Parser.cpp:153, :163), so
                // "#define\t$A\t$A" is a valid self-referencing macro to it.
                // This gate split on a literal space only, and JUCE's
                // fromFirstOccurrenceOf returns EMPTY when the needle is absent -
                // so the macro value came back "", the '$' test saw nothing, and
                // the line that hangs the app passed.  Normalising first means
                // every token rule below gets the same view sfizz has.
                line = line.replaceCharacter ('\t', ' ');

                // A DIRECTIVE CAN SIT MID-LINE.  sfizz ends an opcode value at
                // whitespace followed by '#' and then honours the directive
                // (Parser.cpp:336-344), so "sample=x.wav #define $A $A" carries a
                // live macro definition.  Matching startsWith on the trimmed line
                // saw only an ordinary sample= line and walked straight past it.
                // Split at each directive boundary and scan the pieces
                // independently - which also handles several on one line.
                juce::StringArray segments;
                {
                    int segStart = 0;

                    for (int i = 1; i < line.length(); ++i)
                    {
                        if (line[i] != '#' || ! juce::CharacterFunctions::isWhitespace (line[i - 1]))
                            continue;

                        const auto rest2 = line.substring (i);

                        if (rest2.startsWithIgnoreCase ("#define") || rest2.startsWithIgnoreCase ("#include"))
                        {
                            segments.add (line.substring (segStart, i));
                            segStart = i;
                        }
                    }

                    segments.add (line.substring (segStart));
                }

                for (const auto& segment : segments)
                {
                if (failure.isNotEmpty()) return;

                auto t = segment.trim();
                if (t.isEmpty()) continue;

                // Parsed from the RAW line, before expansion, so an existing
                // macro cannot rewrite the name a later #define is declaring.
                if (t.startsWithIgnoreCase ("#define"))
                {
                    // "#define $NAME value" - both single tokens.
                    const auto rest  = t.fromFirstOccurrenceOf ("#define", false, true).trim();
                    const int  sp1   = rest.indexOfChar (' ');
                    const auto name  = (sp1 > 0) ? rest.substring (0, sp1) : rest;
                    const auto after = (sp1 > 0) ? rest.substring (sp1 + 1).trim() : juce::String();
                    const int  sp2   = after.indexOfChar (' ');
                    const auto value = (sp2 > 0) ? after.substring (0, sp2) : after;

                    if (value.containsChar ('$'))
                    { failure = "one of its $macros expands to another macro, which never terminates"; return; }

                    if (name.startsWithChar ('$') && name.length() > 1)
                        macros[name] = value;

                    // THE TAIL IS LIVE TEXT.  sfizz consumes the name and value
                    // and then PUTS BACK the rest of the line, re-parsing it as
                    // ordinary opcodes.  Skipping to the next line here - which is
                    // what this did - let `#define $A 1 sample=evil.ogg` past
                    // every rule below.
                    t = (sp2 > 0) ? after.substring (sp2 + 1).trim() : juce::String();
                    if (t.isEmpty()) continue;
                }

                t = expand (t).trim();
                if (t.isEmpty()) continue;

                if (t.startsWithIgnoreCase ("#include"))
                {
                    const int q1 = t.indexOfChar ('"');
                    const int q2 = (q1 >= 0) ? t.indexOfChar (q1 + 1, '"') : -1;

                    if (q1 >= 0 && q2 > q1)
                    {
                        const auto rel = t.substring (q1 + 1, q2).replaceCharacter ('\\', '/');

                        if (rel.startsWith ("//"))
                        { failure = "it includes a file from a network share"; return; }

                        // An include that leaves the kit folder is this scanner
                        // being walked somewhere sfizz would not follow, which is
                        // an attack on the gate rather than a kit layout.  Sample
                        // paths are deliberately NOT confined this way (see the
                        // header's "what this does not close") - an include is
                        // different, because it steers the scan itself.
                        if (rel.startsWithChar ('/') || rel.containsChar (':'))
                        { failure = "it includes a file from outside the kit folder"; return; }

                        const auto target = root.getChildFile (rel);

                        if (! target.isAChildOf (root))
                        { failure = "it includes a file from outside the kit folder"; return; }

                        walk (target, depth + 1);
                    }

                    continue;
                }

                // sample= / default_path= - path shape and decoder allowlist.
                //
                // EVERY occurrence on the line, not just the first.  A line can
                // carry several regions - "<region> sample=ok.wav lokey=0
                // <region> sample=payload.ogg" is two perfectly ordinary regions
                // to sfizz - so a single indexOf examined ok.wav and passed the
                // line, defeating the .ogg/.aiff decoder refusal (the stb_vorbis
                // and libaiff CVEs this rule exists for) and the UNC refusal with
                // it.  This is the SAME defect already fixed for the opcode-index
                // rule below; the sample scan did not get the same treatment.
                for (const auto* key2 : { "sample=", "default_path=" })
                for (int at = t.indexOfIgnoreCase (key2); at >= 0;
                     at = t.indexOfIgnoreCase (at + 1, key2))
                {
                    auto val = t.substring (at + (int) juce::String (key2).length()).trim();

                    // An SFZ opcode value runs to the START of the next opcode,
                    // not to the next space - sample names legitimately contain
                    // spaces ("Grand Piano C4.wav").  So find the next '=', then
                    // cut back to the space before the name attached to it.
                    const int nextEq = val.indexOfChar ('=');
                    if (nextEq >= 0)
                    {
                        const int cut = val.substring (0, nextEq).lastIndexOfChar (' ');
                        if (cut > 0)
                            val = val.substring (0, cut).trim();
                    }

                    const auto norm = val.replaceCharacter ('\\', '/');

                    if (norm.startsWith ("//"))
                    { failure = "it loads a sample from a network share"; return; }

                    if (isRefusedSampleExtension (norm))
                    { failure = "it uses a sample format we cannot load safely ("
                              + norm.fromLastOccurrenceOf (".", true, false) + ")"; return; }
                }

                // Opcode index blowup, e.g. `lfo65535_freq=1`.
                //
                // SFZ packs MANY opcodes onto one line, so the name to examine
                // is the token immediately before EACH '=', not the text before
                // the first one.  A first version scanned only up to the first
                // '=' and therefore missed the exact attack this rule exists to
                // stop: in "<region> sample=Kick.wav lfo65535_freq=1" it saw
                // only "<region> sample" and passed the line.
                // LENGTH FIRST, THEN VALUE.  juce::String::getIntValue accumulates
                // into a 32-bit int and WRAPS with no range check
                // (juce_CharacterFunctions.h getIntValue), so the check it used to
                // do was defeated by making the number bigger: "4294967296" wraps
                // to 0 and "4294967808" wraps to exactly 512, both of which pass
                // "> 512" cleanly.  A legitimate index is at most three digits, so
                // any run longer than the cap's own width is refused before a
                // conversion can wrap.  This is also the case that reaches
                // sfizz's empty-vector front() crash (Region.cpp:1248).
                const auto checkDigits = [&failure] (const juce::String& digits)
                {
                    if (digits.isEmpty()) return;

                    if (digits.length() > 5 || digits.getIntValue() > kMaxOpcodeIndex)
                        failure = "one of its opcodes uses an absurd index (" + digits + ")";
                };

                for (int eq = t.indexOfChar ('='); eq > 0; )
                {
                    const auto before = t.substring (0, eq);
                    const int  sp     = before.lastIndexOfChar (' ');
                    const auto name   = before.substring (sp + 1);

                    juce::String digits;

                    for (int i = 0; i < name.length(); ++i)
                    {
                        const auto c = name[i];

                        if (c >= '0' && c <= '9')
                        {
                            digits += c;
                        }
                        else if (digits.isNotEmpty())
                        {
                            checkDigits (digits);
                            if (failure.isNotEmpty()) return;
                            digits.clear();
                        }
                    }

                    checkDigits (digits);
                    if (failure.isNotEmpty()) return;

                    const int next = t.indexOfChar (eq + 1, '=');
                    eq = next;
                }
                }
            }

            // Leaving this file: it is no longer an ancestor of anything.
            // Without this the list only ever grows and behaves like the global
            // visited set that wrongly refused our own kits.
            ancestors.removeString (key);
        };

        walk (sfzPath, 0);
        return failure;
    }
}
