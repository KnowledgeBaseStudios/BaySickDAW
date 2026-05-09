# QA-A — STYLE Cluster: Unified BaySickTitleBar Component

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a shared `Source/Standalone/BaySickTitleBar.h/.cpp` component (standardized height + font + padding + per-engine accent color), refactor all 7 player editors to use it, and fix the BaySickPlayer ribbon-tab text clipping.

**Architecture:** New `juce::Component` subclass owns engine-name paint + accent + standardized background. Parent editor positions trailing widgets (preset dropdown / A-B toggle / help button) using `getTrailingArea(int width)`. `juce::Colour` accent passed at construction (decision via AskUserQuestion 2026-05-09). Component-first + one-editor proof + sweep landing per Jeff's choice.

**Tech Stack:** JUCE 7 (`juce::Component`, `juce::Graphics`, `juce::Font`), MSVC, `do_build.bat` (Release + Debug).

---

## Context

QA-A is the next batch in the post-Batch-10 QA plan after QA-Md (closed 2026-05-09 at commit `1af61be`). It's a UI-only batch consolidating six STYLE items into a single Unified TitleBar Component:

- **STYLE-01** — BaySickPlayer ribbon tab text clips (slot too narrow per Jeff's confirmation; the brand is "BaySickPlayer" singular, NOT plural).
- **STYLE-02** — Standardize logo/font/size across player pages (folded LDT-167 from QA-Inventory close — see Main Plan §9 fifth Forks entry).
- **STYLE-03** — `BaySickVocalEditor`'s `BaySickVocalsPanel` sub-panel section header reads "PAGE CONTROLS" at [Source/BaySickVocal/BaySickVocalEditor.cpp:294](Source/BaySickVocal/BaySickVocalEditor.cpp:294); should read "BAYSICKVOCALS".
- **STYLE-04** — `BaySickPedalsEditor` (Inst BaySickGuitars) currently has no engine title; needs "BaySickGuitars" label between player area and sub-tabs.
- **STYLE-05** — `BaySickNAMIREditor` has an extra/redundant black bar in the title area.
- **STYLE-06** — `BaySickSynthEditor` + `BaySickBassEditor`: preset dropdown should sit RIGHT (currently LEFT at `(6, 5, 88, 22)`); engine title should render in the engine's green accent (`BaySickSynthLAF::kGreen` 0xFFA0DB2B for Synth; `BaySickBassLAF::kGreen` 0xFF33FF88 for Bass).

**Risk:** very low (UI-only, no audio path changes).
**Effort:** medium (~4-6 hours).
**Dependencies:** none.

### Spec calls confirmed via AskUserQuestion 2026-05-09

1. **Component scope:** Universal — every engine page gets a TitleBar. STYLE-03 and STYLE-04 use the new TitleBar (not a string change / juce::Label).
2. **Color API:** `juce::Colour` accent passed in by each parent editor at construction. Each engine keeps its own color (existing engines reuse their LAF's accent; new entries get colors picked from RibbonTabBar's tab palette OR Mesa red for NAMIR — see per-engine table below). Zero LAF coupling inside the TitleBar component.
3. **Refactor staging:** Component-first + one-editor proof (VibePlayer) + sweep the rest.
4. **STYLE-01 root cause:** Ribbon slot too narrow — `g.drawText` clips. Fix in ribbon paint / slot sizing.
5. **Bloom support:** BaySickTitleBar takes an optional `bool bloom = false` ctor flag. HarmlessEditor opts in (`true`) so its long-standing orange-glow signature is preserved. Other engines stay flat / single-pass; future engines can opt in if their visual identity demands it.

### Existing per-editor header geometry (current state, before refactor)

| Editor | Header height | Title text now | Title color | Preset position |
|--------|---------------|----------------|-------------|-----------------|
| `HarmlessEditor` | `kHdrH = 36` ([:7](Source/Harmless/HarmlessEditor.cpp:7)) | "HARMLESS" (bloomed twice — paint at [:637-642](Source/Harmless/HarmlessEditor.cpp:637)) | `HarmlessLAF::kAccent = 0xFFFF6600` | RIGHT — `(getWidth()-92, 4, 86, kHdrH-8)` ([:773](Source/Harmless/HarmlessEditor.cpp:773)) |
| `VibePlayerEditor` | `kHdrH = 36` ([:5](Source/VibePlayer/VibePlayerEditor.cpp:5)) | "BAYSICKPLAYER" via `juce::Label` ([:47-50](Source/VibePlayer/VibePlayerEditor.cpp:47)) | `0xFFE0E0E0` (white) | RIGHT — `(w-200-kPad, 7, 110, 22)` + help btn `(w-82-kPad, 7, 24, 22)` ([:404-406](Source/VibePlayer/VibePlayerEditor.cpp:404)) |
| `BaySickSynthEditor` | 32 (hardcoded) | none — only preset btn | n/a | **LEFT** — `(6, 5, 88, 22)` ([:537](Source/BaySickSynth/BaySickSynthEditor.cpp:537)) |
| `BaySickBassEditor` | 32 (hardcoded) | none — only preset btn | n/a | **LEFT** — `(6, 5, 88, 22)` ([:529](Source/BaySickBass/BaySickBassEditor.cpp:529)) |
| `BaySickNAMIREditor` | `kHeaderH = 28` ([:37](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:37)) | "BaySickNAM/IR" via `juce::Label` ([:409](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:409)) | label default | RIGHT — A/B slot toggles ([:411-412](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:411)); 1px divider at `y=kHeaderH` ([:402-403](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:402)) |
| `BaySickVocalEditor::BaySickVocalsPanel` (inner) | section header inside panel | "PAGE CONTROLS" via `g.drawText` ([:294](Source/BaySickVocal/BaySickVocalEditor.cpp:294)) | `Colours::white.withAlpha(0.5f)` | n/a |
| `BaySickPedalsEditor` | none | none | n/a | n/a |

**Standardize at 32 px height** — Harmless + VibePlayer shrink 4px (4px more body), BaySickNAMIR grows 4px (4px less body), BaySickSynth + BaySickBass unchanged.

**Per-engine accent color (confirmed by Jeff via AskUserQuestion 2026-05-09; expanded 2026-05-09 to cover BaySickGuitars / BaySickBasses / BaySickRustyDrums):**

| Engine | Title text | Accent | Source |
|--------|-----------|--------|--------|
| Harmless | "HARMLESS" | `HarmlessLAF::kAccent` (#FF6600 orange) | Existing engine LAF. Bloom default flipped on at Step 1b so all engines get the halo. |
| VibePlayer (BaySickPlayer) | "BAYSICKPLAYER" | `0xFFD4A017` (amber / gold) | Same as Clips + Builder tab active color (`VC::Warm`); see [RibbonTabBar.cpp:11,15](Source/Standalone/RibbonTabBar.cpp:11) and [SharedUI.h:26](Source/Standalone/SharedUI.h:26) |
| BaySickSynth | "BAYSICKSYNTH" | `BaySickSynthLAF::kGreen` (#A0DB2B FL green) | Existing engine LAF (STYLE-06) |
| BaySickBass | "BAYSICKBASS" | `BaySickBassLAF::kGreen` (#33FF88 B1 neon green) | Existing engine LAF (STYLE-06) |
| BaySickNAMIR | "BaySickNAM/IR" | `0xFFE0303F` (Mesa red) | New — Jeff's pick |
| BaySickVocal | "BAYSICKVOCALS" | `0xFF0FAFA5` (bright teal) | Same as Vox tab **active** color; see [RibbonTabBar.cpp:20](Source/Standalone/RibbonTabBar.cpp:20) |
| BaySickPedals | **"BAYSICKPEDALS"** *(corrected 2026-05-09 — was "BAYSICKGUITARS" originally per STYLE-04 phrasing, but Pedals is a distinct engine from Guitars)* | `0xFF1C3A8A` (navy / royal blue) | Same as Inst tab **active** color; see [RibbonTabBar.cpp:21](Source/Standalone/RibbonTabBar.cpp:21) |
| **BaySickGuitars** *(scope expansion 2026-05-09)* | "BAYSICKGUITARS" | `0xFF1C3A8A` (navy — shared with Pedals + Basses) | Inst tab active color; Jeff's pick |
| **BaySickBasses** *(scope expansion 2026-05-09)* | "BAYSICKBASSES" | `0xFF1C3A8A` (navy — shared with Pedals + Guitars) | Inst tab active color; Jeff's pick |
| **BaySickRustyDrums** *(scope expansion 2026-05-09)* | "BAYSICKRUSTYDRUMS" | `0xFFCC2222` (Drums tab red) | Same as Drums tab active color; Jeff's pick |

---

## Files to create or modify

**Create (2):**
- `Source/Standalone/BaySickTitleBar.h` — class declaration.
- `Source/Standalone/BaySickTitleBar.cpp` — implementation.

**Modify (7 player editors):**
- `Source/VibePlayer/VibePlayerEditor.h` + `.cpp` (proof-of-concept).
- `Source/Harmless/HarmlessEditor.h` + `.cpp`.
- `Source/BaySickSynth/BaySickSynthEditor.h` + `.cpp` (STYLE-06: preset L→R, green accent).
- `Source/BaySickBass/BaySickBassEditor.h` + `.cpp` (STYLE-06: preset L→R, green accent).
- `Source/BaySickNAMIR/BaySickNAMIREditor.h` + `.cpp` (STYLE-05: black-bar removal via standardized paint).
- `Source/BaySickVocal/BaySickVocalEditor.cpp` (STYLE-03: PAGE CONTROLS → BAYSICKVOCALS via TitleBar in inner `BaySickVocalsPanel` class).
- `Source/BaySickPedals/BaySickPedalsEditor.h` + `.cpp` (introduce TitleBar above pedal grid; **title = "BAYSICKPEDALS"** per 2026-05-09 correction; pedalboard preset button migrates from InstPage chrome into the title bar's trailing widget slot).

**Modify (scope expansion 2026-05-09 — sfizz-engine UIs that didn't exist as targets in the original plan):**
- `Source/Standalone/AriaControlPanel.h` + `.cpp` — extend `Binding` struct with optional `engineName` + `accentColor` fields; render an internal `BaySickTitleBar` at the top of the panel when both are set. Title bar height is taken out of the kit-artwork area.
- `Source/Inst/InstPage.h` + `.cpp` — wire BaySickGuitars + BaySickBasses source modes to set the AriaControlPanel binding's engine name + accent. Remove the `kHeaderRowH` page chrome (the dark `mPedalsHeaderTitle` strip) — each engine UI now owns its title bar.
- `Source/Standalone/BaySickRustyDrumsPage.h` + `.cpp` — add a `BaySickTitleBar` at the top of the page's content area. The existing menu buttons (the black bar Jeff referenced) re-anchor against the top edge of the player area, with the title bar above them.

**Modify (build + ribbon):**
- `CMakeLists.txt` — add `Source/Standalone/BaySickTitleBar.cpp` to source list.
- `Source/Standalone/RibbonTabBar.cpp` — STYLE-01 truncation fix.

**Modify (plan tracking):**
- `Plans & Specs/Main Plan.md` — §5 QA-A entry's `**Plan file:**` line.
- `Plans & Specs/Batch Plans/twinkling-herding-twilight.md` — mirror of this plan.
- `Plans & Specs/Implemented Work Log.md` — close entry (appended at batch close).

---

## BaySickTitleBar API

Full code shown so the parent editor refactor tasks have something concrete to call.

```cpp
// Source/Standalone/BaySickTitleBar.h
#pragma once

#include <JuceHeader.h>

/** Shared engine-page title bar.
    Standardized height (32) + font (16pt bold) + left-anchored engine name in
    accent color + standardized dark background + 1px bottom divider.
    Trailing widgets (preset dropdown / A-B toggles / help button) are owned by
    the parent editor; the parent calls `getTrailingArea(width)` to lay them out.

    No LAF coupling - accent is a `juce::Colour` parameter at construction.

    Optional bloom (per-engine opt-in, decided 2026-05-09): paints the engine
    name twice when enabled - 17pt bold underlay at 15% accent opacity, offset
    by (-1, -1), creating a halo around the standard 16pt bold overlay. Used
    today by HarmlessEditor to preserve its long-standing orange-glow signature;
    other engines can opt in later if their visual identity calls for it. */
class BaySickTitleBar : public juce::Component
{
public:
    BaySickTitleBar (const juce::String& engineName,
                     juce::Colour accentColor,
                     bool bloom = false);
    ~BaySickTitleBar() override = default;

    static constexpr int   kStandardHeight = 32;
    static constexpr int   kPaddingPx      = 8;
    static constexpr float kFontSizePx     = 16.0f;

    void setEngineName  (const juce::String& name);
    void setAccentColor (juce::Colour c);
    void setBloom       (bool enabled);

    juce::String getEngineName  () const { return mEngineName; }
    juce::Colour getAccentColor () const { return mAccentColor; }
    bool         getBloom       () const { return mBloom; }

    /** Returns the right-anchored rectangle the parent should use to lay out
        trailing widgets. `trailingWidth` is the pixel width the cluster needs.
        Result is full-height (0..getHeight()), starting at
        `getWidth() - kPaddingPx - trailingWidth`. */
    juce::Rectangle<int> getTrailingArea (int trailingWidth) const;

    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    juce::String mEngineName;
    juce::Colour mAccentColor;
    bool         mBloom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickTitleBar)
};
```

```cpp
// Source/Standalone/BaySickTitleBar.cpp
#include "BaySickTitleBar.h"

BaySickTitleBar::BaySickTitleBar (const juce::String& engineName,
                                  juce::Colour accentColor,
                                  bool bloom)
    : mEngineName (engineName)
    , mAccentColor (accentColor)
    , mBloom (bloom)
{
    // Title bar paints + reports geometry; parent owns the trailing widgets.
    // Pass clicks through so trailing widgets sitting on top remain interactive
    // even if the parent makes them children of the bar.
    setInterceptsMouseClicks (false, true);
}

void BaySickTitleBar::setEngineName (const juce::String& name)
{
    if (mEngineName != name)
    {
        mEngineName = name;
        repaint();
    }
}

void BaySickTitleBar::setAccentColor (juce::Colour c)
{
    if (mAccentColor != c)
    {
        mAccentColor = c;
        repaint();
    }
}

void BaySickTitleBar::setBloom (bool enabled)
{
    if (mBloom != enabled)
    {
        mBloom = enabled;
        repaint();
    }
}

juce::Rectangle<int> BaySickTitleBar::getTrailingArea (int trailingWidth) const
{
    const int x = juce::jmax (kPaddingPx,
                              getWidth() - kPaddingPx - trailingWidth);
    return { x, 0, trailingWidth, getHeight() };
}

void BaySickTitleBar::paint (juce::Graphics& g)
{
    // Standardized dark background (matches existing Harmless/VibePlayer tone).
    g.fillAll (juce::Colour (0xFF141618));

    // 1px bottom divider for visual separation against the panel below.
    g.setColour (juce::Colour (0xFF333537));
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    const auto textRect = juce::Rectangle<int> (kPaddingPx, 0,
                                                getWidth() - 2 * kPaddingPx,
                                                getHeight());

    if (mBloom)
    {
        // Halo underlay: 1pt larger font, 15% alpha, offset by (-1, -1).
        // Mirrors the original HarmlessEditor bloom (16pt underlay + 15pt
        // overlay) but pinned to standard 16pt visible text so all engines'
        // crisp glyphs render at the same size.
        g.setColour (mAccentColor.withAlpha (0.15f));
        g.setFont   (juce::Font (kFontSizePx + 1.0f, juce::Font::bold));
        g.drawText  (mEngineName,
                     textRect.translated (-1, -1).withHeight (textRect.getHeight() + 2),
                     juce::Justification::centredLeft, true);
    }

    // Crisp overlay (always).
    g.setColour (mAccentColor);
    g.setFont   (juce::Font (kFontSizePx, juce::Font::bold));
    g.drawText  (mEngineName, textRect, juce::Justification::centredLeft, true);
}

void BaySickTitleBar::resized()
{
    // Parent positions trailing widgets via getTrailingArea(); no internal layout.
}
```

---

## Tasks

### Task 0: Open the QA-A batch (chore)

**Files:**
- Mirror this plan to `Plans & Specs/Batch Plans/twinkling-herding-twilight.md` (Write the same content).
- Modify `Plans & Specs/Main Plan.md` — set `**Plan file:**` on the QA-A entry to point at `Batch Plans/twinkling-herding-twilight.md`.

- [ ] **Step 0.1:** Mirror `~/.claude/plans/twinkling-herding-twilight.md` content to `Plans & Specs/Batch Plans/twinkling-herding-twilight.md` via Write.
- [ ] **Step 0.2:** Find the QA-A `### **QA-A:**` block in `Plans & Specs/Main Plan.md` §5 (around line 691). If a `**Plan file:**` line exists, update it to `Batch Plans/twinkling-herding-twilight.md`. If no such line, add `> **Plan file:** Batch Plans/twinkling-herding-twilight.md` immediately under the heading. Match the format used by other batch entries.
- [ ] **Step 0.3:** Surface git status to Jeff (per `feedback_surface_full_git_status_before_commit.md` in memory).
- [ ] **Step 0.4:** Dispatch `/draft-commit` for the open-batch chore commit. Show Jeff the message, get his nod, then commit only `Plans & Specs/Batch Plans/twinkling-herding-twilight.md` and `Plans & Specs/Main Plan.md`.

### Phase 1 — Build the BaySickTitleBar component

#### Task 1.1: Create BaySickTitleBar.h + .cpp

**Files:**
- Create: `Source/Standalone/BaySickTitleBar.h`.
- Create: `Source/Standalone/BaySickTitleBar.cpp`.

- [ ] **Step 1.1.1:** Write `Source/Standalone/BaySickTitleBar.h` exactly per the API block above.
- [ ] **Step 1.1.2:** Write `Source/Standalone/BaySickTitleBar.cpp` exactly per the implementation block above.

#### Task 1.2: Add to CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt` (around line 323 where `target_sources(BaySickDAWStandalone PRIVATE` lives).

- [ ] **Step 1.2.1:** Find the Standalone source list (around line 323). Identify the cluster that holds other `Source/Standalone/*.cpp` entries.
- [ ] **Step 1.2.2:** Add `Source/Standalone/BaySickTitleBar.cpp` to that cluster, alphabetically near the other `BaySick*` or `S*` Standalone files.
- [ ] **Step 1.2.3:** Verify the include path list (around line 460) already covers `Source/Standalone` — should already be present.

#### Task 1.3: Build + commit Phase 1

- [ ] **Step 1.3.1:** Tell Jeff to run `do_build.bat`. Wait for `RELEASE_EXIT_CODE` and `DEBUG_EXIT_CODE` from `build_log.txt` — both must be 0. (BaySickTitleBar isn't yet referenced anywhere, so this build proves only that the new file compiles in isolation.)
- [ ] **Step 1.3.2:** If build fails, dispatch `/diagnose-build` with the error block. Fix and rebuild.
- [ ] **Step 1.3.3:** Surface git status to Jeff. Dispatch `/draft-commit`. Stage only `Source/Standalone/BaySickTitleBar.h`, `Source/Standalone/BaySickTitleBar.cpp`, `CMakeLists.txt`. Commit.

### Phase 2 — VibePlayer proof-of-concept refactor

#### Task 2.1: Refactor VibePlayerEditor.h

**Files:**
- Modify: `Source/VibePlayer/VibePlayerEditor.h`.

- [ ] **Step 2.1.1:** Add `#include "../Standalone/BaySickTitleBar.h"` near the existing `#include "VibePlayerLAF.h"` line.
- [ ] **Step 2.1.2:** Find the `juce::Label mTitleLbl;` member (private section). Replace it with `BaySickTitleBar mTitleBar { "BAYSICKPLAYER", juce::Colour (0xFFD4A017) };` initialized inline. (Colour is the amber / gold from `VC::Warm` — same hue as the Clips + Builder tab active color.)
- [ ] **Step 2.1.3:** Leave `mPresetBtn` and `mHelpBtn` members untouched.

#### Task 2.2: Refactor VibePlayerEditor.cpp constructor

**Files:**
- Modify: [Source/VibePlayer/VibePlayerEditor.cpp:46-63](Source/VibePlayer/VibePlayerEditor.cpp:46) — the `// ── Header ──` block.

Current code at [:46-63](Source/VibePlayer/VibePlayerEditor.cpp:46):
```cpp
// ── Header ────────────────────────────────────────────────────────────────
mTitleLbl.setText ("BAYSICKPLAYER", juce::dontSendNotification);
mTitleLbl.setFont (juce::Font (16.f, juce::Font::bold));
mTitleLbl.setColour (juce::Label::textColourId, juce::Colour (0xFFE0E0E0));
addAndMakeVisible (mTitleLbl);

mPresetBtn.onClick = [this] { showPresetMenu(); };
addAndMakeVisible (mPresetBtn);

mHelpBtn.onClick = [this] { /* ... */ };
addAndMakeVisible (mHelpBtn);
```

Replace with:
```cpp
// ── Header ────────────────────────────────────────────────────────────────
addAndMakeVisible (mTitleBar);

mPresetBtn.onClick = [this] { showPresetMenu(); };
addAndMakeVisible (mPresetBtn);

mHelpBtn.onClick = [this] { /* ... preserved ... */ };
addAndMakeVisible (mHelpBtn);
```

- [ ] **Step 2.2.1:** Apply the replacement above. Preserve the `mHelpBtn.onClick` lambda body verbatim.

#### Task 2.3: Refactor VibePlayerEditor.cpp paint()

**Files:**
- Modify: [Source/VibePlayer/VibePlayerEditor.cpp:349-358](Source/VibePlayer/VibePlayerEditor.cpp:349).

Current header-paint section ([:354-358](Source/VibePlayer/VibePlayerEditor.cpp:354)):
```cpp
// Header bar
g.setColour (juce::Colour (0xFF141618));
g.fillRect  (0, 0, getWidth(), kHdrH);
g.setColour (juce::Colour (0xFF333537));
g.drawHorizontalLine (kHdrH, 0.f, (float) getWidth());
```

Delete those 5 lines — `BaySickTitleBar::paint()` now owns header background + divider.

- [ ] **Step 2.3.1:** Delete the 5-line header paint block. Keep `g.fillAll(0xFF1A1C1F)` for the editor body.
- [ ] **Step 2.3.2:** Update `kHdrH` constant at [Source/VibePlayer/VibePlayerEditor.cpp:5](Source/VibePlayer/VibePlayerEditor.cpp:5) from `36` to `BaySickTitleBar::kStandardHeight` (32). This shifts box layout up by 4px (more body space).

#### Task 2.4: Refactor VibePlayerEditor.cpp resized()

**Files:**
- Modify: [Source/VibePlayer/VibePlayerEditor.cpp:399-406](Source/VibePlayer/VibePlayerEditor.cpp:399).

Current ([:404-406](Source/VibePlayer/VibePlayerEditor.cpp:404)):
```cpp
mTitleLbl.setBounds  (kPad, 0,        100, kHdrH);
mPresetBtn.setBounds (w - 200 - kPad, 7,   110, 22);
mHelpBtn.setBounds   (w - 82 - kPad,  7,   24,  22);
```

Replace with:
```cpp
mTitleBar.setBounds (0, 0, w, BaySickTitleBar::kStandardHeight);

// Preset (110px) + 8px gap + help (24px) = 142px trailing cluster.
const auto trailing = mTitleBar.getTrailingArea (110 + 8 + 24);
const int btnY = (BaySickTitleBar::kStandardHeight - 22) / 2;
mPresetBtn.setBounds (trailing.getX(),               btnY, 110, 22);
mHelpBtn.setBounds   (trailing.getX() + 110 + 8,     btnY,  24, 22);
```

- [ ] **Step 2.4.1:** Apply the replacement above.

#### Task 2.5: Build + verify

- [ ] **Step 2.5.1:** Tell Jeff to run `do_build.bat`. Confirm both exit codes 0.
- [ ] **Step 2.5.2:** Tell Jeff to launch the **Debug** exe first, open a Layers tab with VibePlayer engine, screenshot the title bar. Title should read "BAYSICKPLAYER" left-aligned in white, preset dropdown + help button on the right, no `jassert` dialogs.
- [ ] **Step 2.5.3:** Tell Jeff to launch the **Release** exe and confirm visually identical.
- [ ] **Step 2.5.4:** If layout looks off (wrong height, text clipping, button overlap), iterate inline.

#### Task 2.6: Commit Phase 2

- [ ] **Step 2.6.1:** Surface git status. Dispatch `/draft-commit`. Stage only `Source/VibePlayer/VibePlayerEditor.h` + `.cpp`. Commit.

### Phase 3 — Sweep existing title bars (4 editors)

Each editor follows the same shape: replace the existing title row with `BaySickTitleBar`, route trailing widgets through `getTrailingArea`. Per memory `feedback_commit_at_checkpoints` and `feedback_every_commit_via_draft_commit`, every editor lands as its own verified commit through `/draft-commit`.

#### Task 3.1: HarmlessEditor

**Files:**
- Modify: `Source/Harmless/HarmlessEditor.h` (add member + include).
- Modify: `Source/Harmless/HarmlessEditor.cpp` (delete bloomed paint, swap to TitleBar, route preset via `getTrailingArea`).

Current paint at [:626-642](Source/Harmless/HarmlessEditor.cpp:626) draws header background + bottom border + bloomed "HARMLESS" text twice (alpha 0.15 underlay + full-alpha overlay). Standardize to single-pass via `BaySickTitleBar`. Bloom is sacrificed — accent color preserves the engine identity.

- [ ] **Step 3.1.1:** Add `#include "../Standalone/BaySickTitleBar.h"` to `HarmlessEditor.h`.
- [ ] **Step 3.1.2:** Add `BaySickTitleBar mTitleBar { "HARMLESS", juce::Colour (HarmlessLAF::kAccent), /*bloom*/ true };` member in `HarmlessEditor`. The `true` opts into the orange-glow bloom (17pt bold halo at 15% opacity behind the crisp 16pt text) so the long-standing Harmless visual signature is preserved.
- [ ] **Step 3.1.3:** In ctor, `addAndMakeVisible (mTitleBar);`. (Find the section that adds existing widgets; place near `mPresetBtn`.)
- [ ] **Step 3.1.4:** Update `kHdrH = 36` at [HarmlessEditor.cpp:7](Source/Harmless/HarmlessEditor.cpp:7) to `BaySickTitleBar::kStandardHeight` (32).
- [ ] **Step 3.1.5:** In `paint()`, delete lines [:631-642](Source/Harmless/HarmlessEditor.cpp:631) (`// ── Header bar ──` block + bloomed title text). Keep the `g.fillAll (HarmlessLAF::kChassis)` body fill above it.
- [ ] **Step 3.1.6:** In `resized()` at [:768-773](Source/Harmless/HarmlessEditor.cpp:768), insert `mTitleBar.setBounds (0, 0, getWidth(), BaySickTitleBar::kStandardHeight);` BEFORE the existing `bounds.removeFromTop (kHdrH);` line.
- [ ] **Step 3.1.7:** Replace `mPresetBtn.setBounds (getWidth() - 92, 4, 86, kHdrH - 8);` at [:773](Source/Harmless/HarmlessEditor.cpp:773) with:
  ```cpp
  const auto trailing = mTitleBar.getTrailingArea (86);
  const int btnY = (BaySickTitleBar::kStandardHeight - 22) / 2;
  mPresetBtn.setBounds (trailing.getX(), btnY, 86, 22);
  ```
- [ ] **Step 3.1.8:** Tell Jeff to `do_build.bat` → Debug verify (open Harmless engine in a Layers tab → screenshot title bar → confirm "HARMLESS" in orange) → Release verify.
- [ ] **Step 3.1.9:** Surface git status, `/draft-commit`, commit only `Source/Harmless/HarmlessEditor.h` + `.cpp`.

#### Task 3.2: BaySickSynthEditor (STYLE-06: preset L→R, green title)

**Files:**
- Modify: `Source/BaySickSynth/BaySickSynthEditor.h`.
- Modify: `Source/BaySickSynth/BaySickSynthEditor.cpp`.

- [ ] **Step 3.2.1:** Add `#include "../Standalone/BaySickTitleBar.h"` to `BaySickSynthEditor.h`.
- [ ] **Step 3.2.2:** Add `BaySickTitleBar mTitleBar { "BAYSICKSYNTH", juce::Colour (BaySickSynthLAF::kGreen) };` member.
- [ ] **Step 3.2.3:** In ctor, `addAndMakeVisible (mTitleBar);`.
- [ ] **Step 3.2.4:** In `paint()` at [:519-528](Source/BaySickSynth/BaySickSynthEditor.cpp:519), keep `g.fillAll (BaySickSynthLAF::kBgMain)` and the deck/divider lines. The TitleBar paints its own background — no header paint needed in the editor.
- [ ] **Step 3.2.5:** In `resized()` at [:532-558](Source/BaySickSynth/BaySickSynthEditor.cpp:532), replace the header layout. Currently:
  ```cpp
  // ── Header (32px) ─────────────────────────────────────────────────────────
  mPresetBtn.setBounds (6, 5, 88, 22);
  ```
  becomes:
  ```cpp
  // ── Header (32px) ─────────────────────────────────────────────────────────
  mTitleBar.setBounds (0, 0, w, BaySickTitleBar::kStandardHeight);
  const auto trailing = mTitleBar.getTrailingArea (88);
  const int btnY = (BaySickTitleBar::kStandardHeight - 22) / 2;
  mPresetBtn.setBounds (trailing.getX(), btnY, 88, 22);
  ```
- [ ] **Step 3.2.6:** Tell Jeff to `do_build.bat` → Debug verify (open BaySickSynth → screenshot → confirm "BAYSICKSYNTH" in FL green on LEFT, preset dropdown on RIGHT) → Release verify.
- [ ] **Step 3.2.7:** Surface git status, `/draft-commit`, commit only `Source/BaySickSynth/BaySickSynthEditor.h` + `.cpp`.

#### Task 3.3: BaySickBassEditor (STYLE-06: preset L→R, green title)

Identical structure to Task 3.2 with name="BAYSICKBASS" and accent=`BaySickBassLAF::kGreen`. Both `BaySickSynth` and `BaySickBass` use the SAME 32px header geometry.

**Files:**
- Modify: `Source/BaySickBass/BaySickBassEditor.h`.
- Modify: `Source/BaySickBass/BaySickBassEditor.cpp`.

- [ ] **Step 3.3.1:** Add `#include "../Standalone/BaySickTitleBar.h"` to `BaySickBassEditor.h`.
- [ ] **Step 3.3.2:** Add `BaySickTitleBar mTitleBar { "BAYSICKBASS", juce::Colour (BaySickBassLAF::kGreen) };` member.
- [ ] **Step 3.3.3:** In ctor, `addAndMakeVisible (mTitleBar);`.
- [ ] **Step 3.3.4:** Apply the same `resized()` swap as Task 3.2.5 (line numbers offset to [:524-549](Source/BaySickBass/BaySickBassEditor.cpp:524)).
- [ ] **Step 3.3.5:** Tell Jeff to `do_build.bat` → Debug verify (open BaySickBass → screenshot → confirm "BAYSICKBASS" in B1 neon green on LEFT, preset dropdown on RIGHT) → Release verify.
- [ ] **Step 3.3.6:** Surface git status, `/draft-commit`, commit only `Source/BaySickBass/BaySickBassEditor.h` + `.cpp`.

#### Task 3.4: BaySickNAMIREditor (STYLE-05: extra black bar fix via standardized paint)

`BaySickNAMIREditor` currently has `kHeaderH = 28` ([:37](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:37)) and a 1px divider at `y=kHeaderH` ([:402-403](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:402)). The "extra black bar" likely refers to the visually-redundant divider OR a duplicate dark band — once we replace the editor's custom paint with `BaySickTitleBar`'s standardized paint, the issue should resolve. Confirm visually with Jeff after refactor.

**Files:**
- Modify: `Source/BaySickNAMIR/BaySickNAMIREditor.h`.
- Modify: `Source/BaySickNAMIR/BaySickNAMIREditor.cpp`.

- [ ] **Step 3.4.1:** Add `#include "../Standalone/BaySickTitleBar.h"` to `BaySickNAMIREditor.h`.
- [ ] **Step 3.4.2:** Add `BaySickTitleBar mTitleBar { "BaySickNAM/IR", juce::Colour (0xFFE0303F) };` member. (Colour is Mesa red. `/` is ASCII; literal stays as-is.)
- [ ] **Step 3.4.3:** Find `mTitleLabel` member in the header and DELETE it (TitleBar replaces it).
- [ ] **Step 3.4.4:** In ctor, replace any `addAndMakeVisible (mTitleLabel)` + `mTitleLabel.setText(...)` calls with `addAndMakeVisible (mTitleBar);`.
- [ ] **Step 3.4.5:** Update `kHeaderH = 28` at [:37](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:37) to `32` (or alias to `BaySickTitleBar::kStandardHeight`). Adjacent `kFileRowH` math at [:432-436](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:432) should still work — the file-row code uses `kHeaderH` as a base offset.
- [ ] **Step 3.4.6:** In `paint()` at [:393-404](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:393), delete lines [:397-403](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:397) (header strip fill + divider). Keep `g.fillAll (kCabBgARGB)` body fill at [:395](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:395).
- [ ] **Step 3.4.7:** In `resized()` at [:406-412](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:406), replace the title-label setBounds + slot A/B layout. Currently:
  ```cpp
  mTitleLabel.setBounds (8, 0, 240, kHeaderH);
  const int slotsRight = getWidth() - kPad;
  mSlotBBtn.setBounds (slotsRight - kSlotABtnW,         kSlotABtnY, kSlotABtnW, kSlotABtnH);
  mSlotABtn.setBounds (slotsRight - 2 * kSlotABtnW,     kSlotABtnY, kSlotABtnW, kSlotABtnH);
  ```
  becomes:
  ```cpp
  mTitleBar.setBounds (0, 0, getWidth(), BaySickTitleBar::kStandardHeight);
  const int slotClusterW = 2 * kSlotABtnW;
  const auto trailing = mTitleBar.getTrailingArea (slotClusterW);
  const int slotY = (BaySickTitleBar::kStandardHeight - kSlotABtnH) / 2;
  mSlotABtn.setBounds (trailing.getX(),               slotY, kSlotABtnW, kSlotABtnH);
  mSlotBBtn.setBounds (trailing.getX() + kSlotABtnW,  slotY, kSlotABtnW, kSlotABtnH);
  ```
  Note: A then B (left to right). Verify with Jeff if reverse order is intended.
- [ ] **Step 3.4.8:** Verify `kSlotABtnY` constant is no longer used; if it's only used here, delete it.
- [ ] **Step 3.4.9:** Tell Jeff to `do_build.bat` → Debug verify (open NAM/IR engine, screenshot title) → confirm the extra black bar is gone. **If something extra still shows, ask Jeff to point at it on the screenshot before further code changes.** (Per memory `feedback_diagnose_before_fixing.md` — do not speculate.)
- [ ] **Step 3.4.10:** Release verify.
- [ ] **Step 3.4.11:** Surface git status, `/draft-commit`, commit only `Source/BaySickNAMIR/BaySickNAMIREditor.h` + `.cpp`.

### Phase 4 — Introduce title bars in editors that don't have one (5 editors after 2026-05-09 scope expansion)

> **Scope expansion 2026-05-09:** Phase 4 originally covered only BaySickVocalEditor (Task 4.1) and BaySickPedalsEditor (Task 4.2).  Jeff's clarification mid-execution: every player engine needs its own title bar, including the three sfizz-based engines that share `AriaControlPanel` (BaySickGuitars + BaySickBasses) and the dedicated `BaySickRustyDrumsPage`.  Tasks 4.3 / 4.4 / 4.5 / 4.6 added at that point.  Task 4.2's title text also corrected from "BAYSICKGUITARS" to "BAYSICKPEDALS" because BaySickPedals is a distinct engine from BaySickGuitars (the original STYLE-04 phrasing conflated them).

#### Task 4.1: BaySickVocalEditor — STYLE-03 (PAGE CONTROLS → BAYSICKVOCALS)

The "PAGE CONTROLS" header lives inside an inner class `BaySickVocalsPanel` in `BaySickVocalEditor.cpp` (sub-tab content panel, not the editor's top). It's drawn via `g.drawText` at [:294](Source/BaySickVocal/BaySickVocalEditor.cpp:294) inside the panel's `paint()` override at [:282](Source/BaySickVocal/BaySickVocalEditor.cpp:282). Refactoring the section to use `BaySickTitleBar` keeps consistency with the rest of QA-A.

The panel currently has a 2-section layout (top half = page controls, bottom half = realtime pitch correction). Replacing the section captions with TitleBars affects both top and bottom — but the spec only mentions the top one ("PAGE CONTROLS" → "BAYSICKVOCALS"). The bottom caption ("REALTIME PITCH CORRECTION") is descriptive of the section's contents, not an engine title — keep that as a `g.drawText` for now and leave it for a future polish pass.

**Files:**
- Modify: `Source/BaySickVocal/BaySickVocalEditor.cpp`.

- [ ] **Step 4.1.1:** Read the `BaySickVocalsPanel` inner class fully (around [:140-330](Source/BaySickVocal/BaySickVocalEditor.cpp:140)) before editing — confirm the panel's member list, layout assumptions, and where the top-half content lives.
- [ ] **Step 4.1.2:** Add `#include "../Standalone/BaySickTitleBar.h"` near the top of `BaySickVocalEditor.cpp` (or in `BaySickVocalEditor.h` if shared).
- [ ] **Step 4.1.3:** Add `BaySickTitleBar mTopTitleBar { "BAYSICKVOCALS", juce::Colour (0xFF0FAFA5) };` as a member of `BaySickVocalsPanel`. (Colour is the bright teal from the Vox tab's **active** state at [RibbonTabBar.cpp:20](Source/Standalone/RibbonTabBar.cpp:20).)
- [ ] **Step 4.1.4:** In `BaySickVocalsPanel`'s ctor, `addAndMakeVisible (mTopTitleBar);`.
- [ ] **Step 4.1.5:** In `paint()` at [:282-298](Source/BaySickVocal/BaySickVocalEditor.cpp:282), delete the `g.drawText ("PAGE CONTROLS", ...)` at [:294-295](Source/BaySickVocal/BaySickVocalEditor.cpp:294). Keep the divider at [:288-289](Source/BaySickVocal/BaySickVocalEditor.cpp:288) and the bottom caption "REALTIME PITCH CORRECTION" at [:296-297](Source/BaySickVocal/BaySickVocalEditor.cpp:296).
- [ ] **Step 4.1.6:** In `resized()` at [:300+](Source/BaySickVocal/BaySickVocalEditor.cpp:300), insert `mTopTitleBar.setBounds (0, 0, getWidth(), BaySickTitleBar::kStandardHeight);` BEFORE existing top-half layout. Adjust the top-half content layout to start AT `BaySickTitleBar::kStandardHeight` instead of from the panel's top (was `top.removeFromTop(half).reduced(16, 24)` — adjust the reduced offset to skip the title bar).
- [ ] **Step 4.1.7:** Tell Jeff to `do_build.bat` → Debug verify (open Vox tab, navigate to BaySickVocals sub-tab, screenshot top section — should read "BAYSICKVOCALS" instead of "PAGE CONTROLS") → Release verify.
- [ ] **Step 4.1.8:** Surface git status, `/draft-commit`, commit only `Source/BaySickVocal/BaySickVocalEditor.cpp`.

#### Task 4.2: BaySickPedalsEditor — introduce "BAYSICKPEDALS" title bar + migrate pedalboard preset button

`BaySickPedalsEditor` currently has no engine title (it's a 4×2 grid of pedal slots). The InstPage's `kHeaderRowH` chrome holds the pedalboard preset button (`mPedalsPresetBtn`) today; that chrome goes away in Task 4.4 when each engine UI owns its own title bar. The pedalboard preset button migrates into BaySickPedalsEditor's title bar trailing slot.

**Files:**
- Modify: `Source/BaySickPedals/BaySickPedalsEditor.h`.
- Modify: `Source/BaySickPedals/BaySickPedalsEditor.cpp`.

- [ ] **Step 4.2.1:** Read `BaySickPedalsEditor.cpp` fully — understand the 4×2 grid layout in `resized()` and any top-of-component offset already in use.
- [ ] **Step 4.2.2:** Add `#include "../Standalone/BaySickTitleBar.h"` to `BaySickPedalsEditor.h`.
- [ ] **Step 4.2.3:** Add `BaySickTitleBar mTitleBar { "BAYSICKPEDALS", juce::Colour (0xFF1C3A8A) };` member. (Colour is the navy / royal blue from the Inst tab's **active** state at [RibbonTabBar.cpp:21](Source/Standalone/RibbonTabBar.cpp:21). Title text "BAYSICKPEDALS" — corrected 2026-05-09 from "BAYSICKGUITARS" since BaySickPedals is a distinct engine from BaySickGuitars.)
- [ ] **Step 4.2.4:** Add `juce::TextButton mPresetBtn { "Preset v" };` member. This is the pedalboard preset button (saves the 8-slot rack as Documents/BaySickDAW/Presets/Pedalboards/{name}.xml). Currently lives in InstPage chrome at `mPedalsPresetBtn`.
- [ ] **Step 4.2.5:** Add `std::function<void()> onPedalboardPresetMenu;` callback member. InstPage will set this callback to its `showPedalboardPresetMenu()` method so the button keeps its existing behavior.
- [ ] **Step 4.2.6:** In ctor, `addAndMakeVisible (mTitleBar);` and `addAndMakeVisible (mPresetBtn);` and `mPresetBtn.onClick = [this] { if (onPedalboardPresetMenu) onPedalboardPresetMenu(); };`.
- [ ] **Step 4.2.7:** In `resized()`, set `mTitleBar.setBounds (0, 0, getWidth(), BaySickTitleBar::kStandardHeight);`. Right-anchor the preset button via `const auto trailing = mTitleBar.getTrailingArea(88); const int btnY = (BaySickTitleBar::kStandardHeight - 22) / 2; mPresetBtn.setBounds(trailing.getX(), btnY, 88, 22);`. Then shift the pedal grid down by `BaySickTitleBar::kStandardHeight` (32px). The grid layout uses `kRows = 2, kCols = 4` near [BaySickPedalsEditor.cpp:54-55](Source/BaySickPedals/BaySickPedalsEditor.cpp:54). Subtract `kStandardHeight` from the available content height before computing tile sizes; offset the grid Y by `kStandardHeight`.
- [ ] **Step 4.2.8:** Tell Jeff to `do_build.bat` → Debug verify (open an Inst tab → BaySickPedals, screenshot — should show "BAYSICKPEDALS" title bar above the 4×2 pedal grid, preset button right-anchored in the title bar) → Release verify. **NOTE:** the preset button won't yet trigger the menu because InstPage hasn't been wired to set `onPedalboardPresetMenu` — that happens in Task 4.4 (InstPage cleanup).
- [ ] **Step 4.2.9:** Surface git status, `/draft-commit`, commit only `Source/BaySickPedals/BaySickPedalsEditor.h` + `.cpp`.

#### Task 4.3: AriaControlPanel — extend Binding with engineName + accentColor + render internal TitleBar

`AriaControlPanel` is the shared kit-artwork renderer used by BaySickGuitars / BaySickBasses / BaySickRustyDrums. To give each of those engines a BaySickDAW-style title bar, the panel hosts an internal `BaySickTitleBar` at the top of its area when the parent supplies an engine name + accent color via the `Binding` struct.

**Files:**
- Modify: `Source/Standalone/AriaControlPanel.h`.
- Modify: `Source/Standalone/AriaControlPanel.cpp`.

- [ ] **Step 4.3.1:** Read `Source/Standalone/AriaControlPanel.h/.cpp` end-to-end — understand the existing `Binding` struct (apvts + ccParamId + kitDefaultCc + ccLabel closures), the `paint()` method (background fill + static text + tab strip), and `resized()` (panel sizing + tab bar layout). Identify where the kit-artwork rendering area starts in `resized()`.
- [ ] **Step 4.3.2:** Add `#include "BaySickTitleBar.h"` to `AriaControlPanel.h`.
- [ ] **Step 4.3.3:** Extend `Binding` struct with two new optional fields: `juce::String engineName {}; juce::Colour accentColor { juce::Colours::transparentBlack };`. Both empty by default — `setEngine()` callers can supply them or leave them unset.
- [ ] **Step 4.3.4:** Add a `std::unique_ptr<BaySickTitleBar> mTitleBar;` member to `AriaControlPanel` (private). Use unique_ptr so it can be swapped in/out as the binding changes.
- [ ] **Step 4.3.5:** In `setEngine(Binding binding)`, if `binding.engineName.isNotEmpty()`: create or update mTitleBar with name + color (use `setEngineName`/`setAccentColor` if exists, else recreate). `addAndMakeVisible(*mTitleBar)`. If engineName is empty: `mTitleBar.reset()`.
- [ ] **Step 4.3.6:** In `resized()`, if `mTitleBar` is set: lay it at `(0, 0, getWidth(), BaySickTitleBar::kStandardHeight)`. Reduce the kit-artwork rendering rect's top by `kStandardHeight` so the kit content moves down 32 px.
- [ ] **Step 4.3.7:** Tell Jeff to `do_build.bat` → Debug verify (no UI changes visible yet because no callers set the engineName field; this is just plumbing) → Release verify.
- [ ] **Step 4.3.8:** Surface git status, `/draft-commit`, commit only `Source/Standalone/AriaControlPanel.h` + `.cpp`.

#### Task 4.4: InstPage — remove kHeaderRowH chrome + wire BaySickGuitars / BaySickBasses / BaySickPedals titles

InstPage currently paints a `kHeaderRowH = 36 px` dark chrome strip with `mPedalsHeaderTitle` (engine name label) + `mPedalsPresetBtn` (preset button visible only in Pedals mode). Each engine UI now owns its title bar (Pedals via Task 4.2, Guitars/Basses via Task 4.3 + this task), so the InstPage chrome becomes redundant.

**Files:**
- Modify: `Source/Inst/InstPage.h`.
- Modify: `Source/Inst/InstPage.cpp`.

- [ ] **Step 4.4.1:** Find and remove the `kHeaderRowH = 36` constant in `InstPage.cpp` (constexpr at line 14).
- [ ] **Step 4.4.2:** In `InstPage::paint()` ([:1192-1199](Source/Inst/InstPage.cpp:1192)), delete the header fill + divider lines. Keep the body fill `g.fillAll(0xff181818)`.
- [ ] **Step 4.4.3:** In `InstPage::resized()` ([:1283-1287](Source/Inst/InstPage.cpp:1283)), delete the header layout block (`auto header = r.removeFromTop(kHeaderRowH)` + the mPedalsPresetBtn / mPedalsHeaderTitle setBounds calls). Remove the `r.removeFromTop(kHeaderRowH)` call so engine UIs fill the full area.
- [ ] **Step 4.4.4:** Delete `mPedalsHeaderTitle` member (no longer used). Delete `mPedalsPresetBtn` member — the button moved to BaySickPedalsEditor in Task 4.2.
- [ ] **Step 4.4.5:** Find where InstPage instantiates BaySickPedalsEditor; wire `mPedalsEditor->onPedalboardPresetMenu = [this] { showPedalboardPresetMenu(); };` so the migrated preset button still routes to InstPage's existing menu.
- [ ] **Step 4.4.6:** Find where InstPage configures the AriaControlPanel binding for BaySickGuitars (the source-mode-aware setup). Set `binding.engineName = "BAYSICKGUITARS"` and `binding.accentColor = juce::Colour (0xFF1C3A8A)`.
- [ ] **Step 4.4.7:** Same for BaySickBasses: `binding.engineName = "BAYSICKBASSES"`, `binding.accentColor = juce::Colour (0xFF1C3A8A)`.
- [ ] **Step 4.4.8:** Tell Jeff to `do_build.bat` → Debug verify: (a) Inst tab → BaySickPedals shows BAYSICKPEDALS title bar with working preset button; (b) Inst tab → BaySickGuitars shows BAYSICKGUITARS title bar above the kit artwork; (c) Inst tab → BaySickBasses shows BAYSICKBASSES title bar; (d) no leftover dark chrome strip from the old InstPage header. Release verify same.
- [ ] **Step 4.4.9:** Surface git status, `/draft-commit`, commit only `Source/Inst/InstPage.h` + `.cpp`.

#### Task 4.5: BaySickRustyDrumsPage — add title bar at top, re-anchor menu buttons below

`BaySickRustyDrumsPage` currently fills its content area with one of three sub-tab components (DrumKit / Player / PianoRoll) at full bounds. The "menu buttons" Jeff referenced (the dark bar visible somewhere in the page) need to sit against the top edge of the player area, with a `BaySickTitleBar` above them.

**Files:**
- Read: `Source/Standalone/BaySickRustyDrumsPage.h` — confirm the existing layout members + identify the menu-button bar.
- Modify: `Source/Standalone/BaySickRustyDrumsPage.h`.
- Modify: `Source/Standalone/BaySickRustyDrumsPage.cpp`.

- [ ] **Step 4.5.1:** Read `BaySickRustyDrumsPage.h/.cpp` end-to-end — confirm where the existing menu bar lives (which member draws / hosts it; sub-tab buttons; etc.). Without knowing exactly which bar Jeff means, the diagnose-before-fixing rule applies: ask Jeff for a screenshot if the layout-edit target is ambiguous.
- [ ] **Step 4.5.2:** Add `#include "BaySickTitleBar.h"` to `BaySickRustyDrumsPage.h`.
- [ ] **Step 4.5.3:** Add `BaySickTitleBar mTitleBar { "BAYSICKRUSTYDRUMS", juce::Colour (0xFFCC2222) };` member. (Colour is the Drums tab active red.)
- [ ] **Step 4.5.4:** In ctor, `addAndMakeVisible (mTitleBar);`.
- [ ] **Step 4.5.5:** In `resized()` ([:76-91](Source/Standalone/BaySickRustyDrumsPage.cpp:76)), reserve the top 32 px for the title bar. Lay `mTitleBar.setBounds (0, 0, getWidth(), BaySickTitleBar::kStandardHeight)`. Then position the existing menu-button bar (per Step 4.5.1's diagnosis) immediately below the title bar, against the top edge of the player area. Sub-tab content (mDrumKitTab / mPlayerTab / mPianoRollTab + AriaControlPanel inside Player) fills the area below.
- [ ] **Step 4.5.6:** Note: AriaControlPanel will receive the engineName + accentColor via the binding for the Player sub-tab too — Task 4.3's plumbing covers it. Confirm the binding setup happens in BaySickRustyDrumsPage's AriaControlPanel ctor / setEngine call. If the engine title bar there is redundant with the page's title bar, set `binding.engineName = ""` so AriaControlPanel skips its internal title bar (only the page-level one shows).
- [ ] **Step 4.5.7:** Tell Jeff to `do_build.bat` → Debug verify (open the Drums tab, screenshot — BAYSICKRUSTYDRUMS title bar in red at top, menu buttons immediately below against player area, sub-tab content fills the rest cleanly) → Release verify.
- [ ] **Step 4.5.8:** Surface git status, `/draft-commit`, commit only `Source/Standalone/BaySickRustyDrumsPage.h` + `.cpp`.

### Phase 5 — STYLE-01 ribbon truncation fix

**Files:**
- Read: `Source/Standalone/RibbonTabBar.h` + `.cpp` (especially around `paintTabButton` / `getSlotDisplayName` / wherever `g.drawText (name, ...)` is called).
- Modify: `Source/Standalone/RibbonTabBar.cpp`.

The Explore agent reported "BaySickPlayer" tab text clips because `g.drawText (name, textR.reduced (8, 0), ..., true)` runs out of room in narrow ribbon slots. Confirm the actual paint site, then pick a fix. Likely options:

- **(a)** Widen the slot allocated to engine instances — adjust the slot-width formula in `RibbonTabBar::resized()` if it's too tight for "BaySickPlayer" (12 chars).
- **(b)** Reduce the font slightly when the text doesn't fit (font-fitting via `g.drawFittedText` with min size).
- **(c)** Shorten the displayed text (e.g. "BSP" or "Player") when too narrow — least preferred since user-recognizable name should win.

Pick the smallest fix once confirmed by reading paint code; do not unilaterally restructure ribbon layout.

#### Task 5.1: Diagnose

- [ ] **Step 5.1.1:** Read `Source/Standalone/RibbonTabBar.cpp` end-to-end. Identify (1) where slot width is computed in `resized()`, (2) where the tab name string is drawn in `paintTabButton` or equivalent, (3) whether there's a name-resolution function that returns the engine display name.
- [ ] **Step 5.1.2:** Identify the failing case Jeff observed — is it the engine type "BaySickPlayer" appearing as the tab name when he hasn't renamed the tab? Or is it user-created instance names that happen to be long? Confirm with Jeff if uncertain (one quick screenshot is enough).

#### Task 5.2: Fix

- [ ] **Step 5.2.1:** Apply the smallest viable fix — most likely `g.drawFittedText` instead of `g.drawText` so the text auto-shrinks when the slot is narrow, OR slot-width adjustment if the slot calc is the culprit.
- [ ] **Step 5.2.2:** Per memory `feedback_check_code_before_calling_it_expected.md`, read both branches of the slot-width formula (with-engine-active vs no-engine-yet) before changing — don't assume which path produces the truncation.
- [ ] **Step 5.2.3:** Tell Jeff to `do_build.bat` → Debug verify (create a new Layers tab with VibePlayer engine; tab should now display "BaySickPlayer" without clipping) → Release verify.
- [ ] **Step 5.2.4:** Surface git status, `/draft-commit`, commit only `Source/Standalone/RibbonTabBar.cpp`.

### Phase 6 — Cross-engine consistency check

#### Task 6.1: Visual sweep

- [ ] **Step 6.1.1:** Tell Jeff to launch Debug exe; open every player engine tab in turn (Layers VibePlayer, Layers Harmless, Layers BaySickSynth, Bass, Drums, Vox, Inst BaySickGuitars, Inst BaySickNAM/IR). Screenshot each title bar.
- [ ] **Step 6.1.2:** Cross-compare screenshots: same height (32px), same font (16pt bold), same horizontal padding (8px), each shows correct engine name in correct accent color, trailing widgets correctly right-aligned without overlap.
- [ ] **Step 6.1.3:** If anything looks inconsistent, identify the editor + apply a final tweak. If all consistent, skip.

#### Task 6.2: Commit any final tweaks

- [ ] **Step 6.2.1:** If Step 6.1.3 changed code, surface git status, `/draft-commit`, commit per-editor.
- [ ] **Step 6.2.2:** If no tweaks needed, skip this commit.

### Phase 7 — Mandatory close sequence (per Main Plan §0 Agent Orchestration Rules)

#### Task 7.1: Draft batch-close entry

- [ ] **Step 7.1.1:** Dispatch `/draft-doc batch-close` with QA-A as context. Drafter compiles the Implemented Work Log entry from running notes.
- [ ] **Step 7.1.2:** Review the draft; adjust if needed.
- [ ] **Step 7.1.3:** Apply the draft via Edit to `Plans & Specs/Implemented Work Log.md` (parent session, not the agent — drafter-only enforcement).

#### Task 7.2: Review batch

- [ ] **Step 7.2.1:** Dispatch `/review-batch QA-A`. Agent reviews diff vs plan + CLAUDE.md rules + canonical conventions + memory-tracked gotchas.
- [ ] **Step 7.2.2:** Address any BLOCKERs immediately. Address NEEDS-FIX items inline. Defer NITs (note in close entry).

#### Task 7.3: Route side findings (if any)

- [ ] **Step 7.3.1:** Per Rule 3 (Main Plan §0): for each finding logged during execution, route to (a) not-yet-started batch's scope, (b) completed batch's annotation + §9 Forks entry, (c) new dedicated §5 batch row, or (d) Future State + Phase 6 QA-Audit docket.

#### Task 7.4: Final close commit

- [ ] **Step 7.4.1:** Surface git status. Confirm working tree only has the close-entry edits.
- [ ] **Step 7.4.2:** Dispatch `/draft-commit` for the close commit.
- [ ] **Step 7.4.3:** Commit the close (separate commit from the batch's source commits — clean rollback boundary).

---

## Verification

End-to-end test after Phase 6:

1. `do_build.bat` produces both Release + Debug exes with `RELEASE_EXIT_CODE=0` and `DEBUG_EXIT_CODE=0`.
2. **Debug exe**: launch, open each of the seven player engine tabs in turn. Each shows its title bar with the standardized 32px height, 16pt bold engine name in its accent color, and trailing widgets right-aligned. No `jassert` dialogs.
3. **Release exe**: same visual checks. Audio plays correctly across all engines (no audio path changes were made).
4. Save → close → reopen → load round-trip: project state restores cleanly; title bars draw correctly on freshly loaded tabs.
5. STYLE-01 specific: create a new Layers tab with VibePlayer engine; ribbon shows "BaySickPlayer" without clipping.
6. STYLE-06 specific: open BaySickSynth + BaySickBass — both show preset dropdown on RIGHT, engine name in green accent on LEFT.
7. STYLE-05 specific: open BaySickNAM/IR — confirm Jeff doesn't see the "extra black bar" he reported.
8. MT toggle round-trip: hamburger → toggle MT off → audio plays → toggle on → audio plays. (Not affected by UI work, but standing per-batch verification.)

---

## Carry-Over (filled at session pause / batch end)

```
## Carry-Over

- **Completed:** [tasks done + verified]
- **In-flight:** [task ID + state of the code (uncommitted edits? failing build? partial test?)]
- **Assumptions changed:** [findings that contradict plan / carry-forward / CLAUDE.md / implemented-work log so far — carry-forward contradictions get logged in Implemented Work Log as new entries, NOT edited into carry-forward]
- **Resume action:** [literal first thing to do next session]
- **Implemented-work entry needed:** [one-line summary for batch-close log + side-finding routings per Rule 3]
```

---

## Self-review checklist (post-write, pre-ExitPlanMode)

- ✓ Spec coverage: STYLE-01 (Phase 5) / STYLE-02 (Phase 1 component + accent palette + standardized 32px height across all editors) / STYLE-03 (Task 4.1) / STYLE-04 (Task 4.2) / STYLE-05 (Task 3.4) / STYLE-06 (Tasks 3.2 + 3.3).
- ✓ No placeholders. Every step has concrete file path + line range + code or command.
- ✓ Type consistency: `BaySickTitleBar` API (ctor / `kStandardHeight` / `getTrailingArea`) is used identically in every editor refactor task.
- ✓ ASCII-only UI strings: "BAYSICKPLAYER" / "HARMLESS" / "BAYSICKSYNTH" / "BAYSICKBASS" / "BaySickNAM/IR" / "BAYSICKVOCALS" / "BAYSICKGUITARS" — all ASCII.
- ✓ Per-commit `/draft-commit` rule honored — every commit step says "dispatch `/draft-commit`".
- ✓ "Surface FULL git status" rule honored — every commit step says "Surface git status".
- ✓ "Stage specific files only" rule honored — every commit step lists the explicit file set.
- ✓ Jeff runs builds — every build step says "Tell Jeff to run `do_build.bat`".
- ✓ Debug-first verification — every verification step says Debug first, Release second.
- ✓ Carry-Forward not edited (frozen). Main Plan §5 only updated for `**Plan file:**` pointer.
- ✓ Non-uniform STYLE-05 / STYLE-01 diagnoses noted as "ask Jeff if uncertain" rather than speculative fixes (per `feedback_diagnose_before_fixing.md`).

---

**End of plan.** Ready for ExitPlanMode.
