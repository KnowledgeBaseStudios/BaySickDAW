# Standalone UI Changes Reference
# Records every deliberate UI change made during active development sessions.
# Referenced by CLAUDE.md. Read this before touching any of the files listed below.

---

## 1. Effect Editor Panels — Output Volume Knob (replaces fader)

**Files:** `EffectEditorPanels.h`, `EffectEditorPanels.cpp`, `SlotComponent.h`, `SlotComponent.cpp`, `SharedUI.h`, `SharedUI.cpp`

### What changed
- `EditorPanelBase` previously had a `SnapSlider` vertical fader (`outputFader`) for per-slot gain.
- Replaced entirely with `std::unique_ptr<VKnob> outputVolKnob` placed to the left of the DBFS meter.
- Fader asset is kept and will be reused on the Mixer page.

### Volume knob filmstrips
- `Filmstrips::volumeBlack()` loads `Files For Claude/Filmstrips/Volume Black.png` — 70×70, 100 frames.
- `Filmstrips::volumeWhite()` loads `Files For Claude/Filmstrips/Volume White.png` — 70×70, 100 frames.
- **Dynamics panels** (CompressorPanel, TransientShaperPanel) call `setVolumeKnobVariant(true)` → renders **black** filmstrip.
- All other panels default to **white** filmstrip.
- The variant is stored as slider property: `slider.getProperties().set("volumeKnob", "black"/"white")`.
- `VibeLAF::drawRotarySlider` checks this property at the top and renders the filmstrip if present.

### Layout in each panel's resized()
```cpp
dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
b.removeFromRight(2);
outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
b.removeFromRight(4);
```

### Knob range
```cpp
outputVolKnob->slider.setRange(-24.0, 12.0, 0.1);
outputVolKnob->slider.setValue(0.0, juce::dontSendNotification);
outputVolKnob->slider.setDoubleClickReturnValue(true, 0.0);
```

### SlotComponent wiring
```cpp
if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
{
    const int slot = mSlotIndex;
    base->onOutputGainChanged = [this, slot](float db) {
        if (mRack) mRack->setSlotOutputGain(slot, db);
    };
    if (mRack)
        base->outputVolKnob->slider.setValue(mRack->getSlotOutputGain(slot),
                                             juce::dontSendNotification);
}
```

---

## 2. Effect Panel Toggle Widths

**Files:** `EffectEditorPanels.cpp`

### What changed
Toggle switch buttons in effect panels were too narrow — the label text was clipped by the switch image.
All toggle buttons were widened so the switch image occupies the left ~40px of the button, with the label text to its right.

### Per-panel widths (removeFromRight values in resized())
- **EditorPanelBase base loop:** `removeFromRight(94).withSizeKeepingCentre(90, 40)`
- **PhaserPanel:** `removeFromRight(90).withSizeKeepingCentre(86, 40)`
- **ReverbPanel extra toggles:** 90px
- **SaturationPanel toggles:** 90px
- **DelayPanel toggles:** 80px

### How switch image vs label split works (VibeLAF)
`drawButtonBackground` draws the pill toggle image into a fixed ~40px square on the left side of the button rect.
`drawButtonText` draws the label into the remaining right portion.

---

## 3. SnapSlider (kept, not used on effects page)

**File:** `SharedUI.h`

```cpp
class SnapSlider : public VibeSlider
{
public:
    SnapSlider() : VibeSlider(juce::Slider::LinearVertical, juce::Slider::NoTextBox) {}
    double snapValue(double v, DragMode) override
    {
        return (std::abs(v) < 1.5) ? 0.0 : v;
    }
};
```

In use as the Mixer strip fader (`MixerTrackStrip::mFader`).  The `VibeSlider` base is
load-bearing: VibeSlider swallows the right-click so it reaches the per-window
`GlobalAutoRightClick` listener that raises the Automate menu.  Reverting the base to
`juce::Slider` kills the mixer faders' Automate menu.

---

## 4. Combined Toolbar Layout

**Files:** `StandaloneEditor.h`, `StandaloneEditor.cpp`, `GlobalTransportBar.h`, `GlobalTransportBar.cpp`, `RibbonTabBar.cpp`, `StandaloneApp.cpp`

### What changed
Three separate bars (Header 36px + Transport 36px + Ribbon 32px = 104px) collapsed into one **40px combined bar**, saving 64px of vertical space for content.

### Bar layout (left to right)
```
[▶][⏸][■] [BPM][TAP][SONG][METRO]  |  [Pattern▾][+]  |  [Mixer][Effects][Builder][Layers][Bass][Drums]  |  CPU:12% 340MB
```

### Z-order approach
`mTransport` is added as the **first** child component so it is the background layer (lowest z-order).
Pattern widgets and `mRibbon` are added after it, so they render on top of the transport's brushed-aluminum paint.
```cpp
// In StandaloneEditor ctor -- order matters:
addAndMakeVisible(*mTransport);    // z=background (paints brushed-aluminum for full bar)
addAndMakeVisible(*mPosReadout);   // overlay -- added AFTER the bar or its paint hides it
addAndMakeVisible(*mTitleLabel);   // hidden
addAndMakeVisible(*mPatternBtn);   // on top of transport
addAndMakeVisible(*mRibbon);       // on top of transport
```

The page menu is not an editor child: each contained window carries its own title strip.

### GlobalTransportBar changes
- `PlayModeCombo` (juce::ComboBox for Song/Pattern) **removed** — redundant with the `SONG` toggle button.
- Public constant `kControlsWidth` added -- the pixel x-position where transport controls end;
  `StandaloneEditor::resized()` uses it to start the pattern button.  Its value tracks whatever
  the transport section currently holds, so read it from `GlobalTransportBar.h` rather than
  copying the number here.
- CPU/RAM label (`mPerfLabel`) stays inside GlobalTransportBar at the far right of its bounds,
  which span the full bar width → CPU shows at the true window right edge.

### StandaloneEditor::resized() combined bar section
```cpp
static constexpr int kBarH       = 40;
static constexpr int kCPUReserve = 120;  // kept clear on right for CPU label
static constexpr int kPatBoxW    = 140;
static constexpr int kAddBtnW    =  32;
static constexpr int kPatStart   = GlobalTransportBar::kControlsWidth + 8;

auto bar = b.removeFromTop(kBarH);
mTransport->setBounds(bar);   // full width background layer

int py = bar.getY() + (kBarH - 28) / 2;
mPatternBtn->setBounds(bar.getX() + kPatStart, py, kPatBtnW, 28);

int ribX = kPatStart + kPatBoxW + 4 + kAddBtnW + 8;
int ribW = bar.getWidth() - ribX - kCPUReserve;
if (ribW > 60)
    mRibbon->setBounds(bar.getX() + ribX, bar.getY(), ribW, kBarH);
```

### RibbonTabBar transparent background
`RibbonTabBar::paint()` no longer fills its background with `VC::Bg`.
The parent `GlobalTransportBar`'s brushed-aluminum fills the whole bar including the tab area.
Tab paths themselves still draw their colored fills on that background.

### Hidden elements
- `mTitleLabel` -- `setVisible(false)`. Title now lives in the OS window title bar
  ("BaySickDAW"; Debug builds append " [DEBUG]").
- `mPatternNameEdit` — `setVisible(false)`. Rename will move into the Pattern▾ dropdown (pending).

### Window title
`VibeSynthWindow` ctor now passes `"BaySickDAW"` instead of `" "` to `DocumentWindow`.

---

## 5. do_build.bat — Full cmake path

**File:** `do_build.bat`

`cmake` was not on the stripped PATH. Fixed by using the full path:
```bat
"C:\Program Files\CMake\bin\cmake.exe" --build ...
```

---

## 6. Tab Dropdown Arrows + Badges

**Files:** `RibbonTabBar.h`, `RibbonTabBar.cpp`, `StandaloneEditor.h`, `StandaloneEditor.cpp`

### Hit testing
- Click body → navigates to the active tab of that type (`getActiveTabForType`)
- Click ▾ arrow region (rightmost `kArrowW=22px`) → opens the dropdown popup

### Badge
- Static badges (Effects=2, Builder=3, Drums=2): always the same
- Dynamic badges (Layers, Bass): `countTabsOfType(type)` — number of open instances

### Layers/Bass dropdown
```
  ✓ Lead Synth
    Pad
    Arp
  ─────────────
  Rename...        ← applies to active instance
  Delete           ← greyed out if only 1 instance; shows confirmation dialog
  ─────────────
  + Add New Layers
```

Delete shows an AlertWindow: "This action cannot be undone. Are you sure?"
Rename opens an AlertWindow with a text editor for the new name.

### Callbacks
```cpp
onTabSelected(int tabId)                          // body click or instance picked from dropdown
onTabClosed(int tabId)                            // Layers/Bass delete confirmed
onAddTabRequest(TabType)                          // Layers/Bass "Add New" clicked
onSubPageSelected(TabType, int subPageIndex)      // Effects/Builder/Drums sub-page picked (0-based)
onTabRenamed(int tabId, const juce::String& name) // rename confirmed
```

### StandaloneEditor changes
- `mHasDrumsTab` removed (Drums is permanent)
- `onAddTabRequest()` now only accepts Layers/Bass types
- `onSubPageSelected()` added -- routes Effects (Rack / Pre EQ / Post EQ) and Builder
  (browser tab) picks only; every other type's dropdown lists that instance's WINDOWS
  instead, via `StandaloneEditor::buildPageWindowRows`.
- Menu "New Drums Tab" permanently disabled

---

## 2026-07-08 — QA-TransportDisplay: position readout + typing-keyboard button

**Files:** `GlobalTransportBar.h/.cpp`, `StandaloneEditor.h/.cpp`, `StandaloneApp.h`, `KeyBindings.h/.cpp`, `TypingKeyboardMap.h` (new)

### TransportPositionReadout
LCD-styled (BPM-field palette) live position readout, an overlay child of
StandaloneEditor placed between the pattern button and the ribbon — the ribbon
absorbs the ~108px width loss (no-expand rule; bar stays 40px). Click toggles
`bars:beats:ticks` (96 PPQ, 1-based) <-> `M:SS.mmm`; mode persists app-wide via
`<TransportDisplay showTime="0|1"/>` in settings.xml. Own 30 Hz timer; repaints
only when the formatted string changes. Song mode counts 4 beats/bar (matches
playback's grid); pattern mode counts the pattern's tsNum (matches metronome
accents) and is naturally pattern-relative (the clock loop-wraps).

## 2026-07-29 — QA-ModelShell TS5: the Effects surface becomes many windows

**Files:** `EffectsPage.h/.cpp`, `EffectWindows.h/.cpp` (new), `FxRackPresetIO.h/.cpp` (new),
`SlotComponent.h/.cpp`, `SharedUI.h/.cpp`, `EffectEditorPanels.cpp`, `StandaloneEditor.h/.cpp`,
`RibbonTabBar.cpp`

### What changed
The Effects page stopped being one page showing everything (three sub-tabs, six stacked panels) and
became a small **rack window** that opens the rest:

- **Rack window** = strip picker + FX Bypass, two EQ buttons, six slot rows. A row is
  `[bypass LED] [effect name button] [up] [down] [picker chevron] [remove X]`; the name button opens
  that effect in its **own window**, and remove prompts first. Title-bar menu carries Save / Load FX
  Rack Preset (six slots + both EQs, via `FxRackPresetIO`) and the VU calibration that used to be a
  "Meters" button.
- **EQ windows** — one Pre, one Post, each fixed to its own EQ; the title strip's two-tab strip
  OPENS the other window rather than swapping contents, so both can be on screen.
- **Panel windows** — one per effect. Basic/Advanced, Mode, SC source and Presets live in that
  window's title-bar MENU (they were buttons on the slot header); the bypass LED sits on its title
  strip and is the same control as the row's.

### Things to know before editing these
- `SlotComponent` now has a `Presentation`: `Inline` (header + editor — BaySickVocal's Vocal Chain
  still uses it) and `PanelOnly` (editor only, chrome on the window). Do not assume a header exists.
- The bypass LED is drawn by `EffectBypassLed::paint` in SharedUI — one routine, three call sites.
  Recolour it there, not per site.
- Satellite windows are keyed by slot **UUID**, not index: removal packs slots up and reorder swaps
  them. Their bounds are `WorkspaceWindow::Persistence::Session` (in-memory, per session) on purpose.
- Every panel mount fires `SlotComponent::onEditorMounted`, and the host answers it with
  `EffectsPage::stampAndRegisterSlotEditor`. A panel built without that has no automation stamps and
  leaves the slot registered against the previous DSP variant.

### FX rack picker reorganised (2026-07-29, Jeff)
The rack picker had been a near-copy of the pedals board's picker — same sections, same order, 13 of
its 24 entries pedal-native (Phase I alpha-merged them in). Rack effects are now the top level and
the pedal types live in a **"Pedals" submenu**; nothing was removed, and saved projects are
unaffected since slots load by `EffectType` and never consult this menu. **Gate** and **De-reverb**
were added — both had DSP, panel and automation tables but appeared in no picker at all, so a rack
slot could never hold either.

**`HeaderSubMenuItem` gotcha (read before reusing it for the VST Plugins group):** a
`PopupMenu::CustomComponent` replaces the LAF's `drawPopupMenuItem` for that row, so it owns
*everything* that call would have drawn — background, **hover highlight**, text and submenu arrow.
The highlight was missed on the first cut and had to be added back by hand
(`isItemHighlighted()` → fill `bounds.reduced(1)` with `highlightedBackgroundColourId`, matching
LookAndFeel_V4). Anything you don't draw is silently absent.

### Window raise-on-click (2026-07-29, Jeff)
A contained window is raised by a click ANYWHERE in it, via
`WorkspaceWindow`'s `setBroughtToFrontOnMouseClick(true)` — JUCE walks the clicked component's
ancestor chain on mouse-down, so clicks on a page's own controls raise the window containing them.
The ribbon sync lives in the `broughtToFront()` override, not in `mouseDown`, so every raise route
keeps the tab bar in step. Do not move it back.

### CL-299 Delay deltas shipped (items 1, 2, 4 — item 3 dropped by owner ruling)
- **Feed knob warning ring**: opt-in via `TimeLAF::kWarnRingFrom` (value = normalized start of the
  warning zone). Drawn as an arc OVER the filmstrip; green -> orange -> red past 100 %.
- **FB-distortion transfer curve**: `FbCurveDisplay` in `EffectEditorPanels.cpp`, input vertical /
  output horizontal, fed by `DelayDSP::shapeFeedbackForDisplay` — which mirrors the processBlock
  branch and must be edited with it.
- **Delay model selector order**: displays Mono / Stereo / PingPong / Off via `kModelOptionValues`.
  Serialized model values are unchanged.

---

### Typing-keyboard MIDI (D-4)
The ~40px slot reserved next to the metronome since D-5 polish now holds the
KeyboardMidiButton (piano-keys icon, amber when on). StandaloneEditor owns the
mode (Ctrl+T = cmdToggleTypingKeyboard 0x10071 is the other entry point) and
pushes visuals back via setTypingKeyboardOn. Two-row map (Z-row octave +
S D G H J sharps; Q-row octave above + number-row sharps), PgUp/PgDn octave
shift [-5..+3], velocity 0.8. Notes inject into the live-MIDI collector →
active-tab routing + recorder + on-screen keyboard lighting for free. While the
mode is on, `TypingKeyboardMap::shouldBypassLocalKeys` makes the three grid
key handlers (PianoRollGrid / DrumKitGrid / ArrangementGrid) decline bare
mapped keys so they bubble to the editor's converter; any modifier = normal
shortcut. Held notes release on mode-off, octave shift, and tab switch.

---

## 2026-08-03 — QA-Layout T1: transport readout, two-row ribbon tabs, centred app title

**Files:** `GlobalTransportBar.h/.cpp`, `StandaloneEditor.cpp`, `RibbonTabBar.h/.cpp`, `SharedUI.h/.cpp`

- **Perf readout** is now `TransportPerfReadout` (custom component, not a `juce::Label`): three
  9pt monospaced rows `SYS/DSP`, `MEM/LAT`, `UND/PF`, right-aligned in a 120px box
  (`TransportPerfReadout::kWidth`).  Per-token colouring (L20): the SYS token colours off
  whole-machine CPU alone; DSP load/overload colours the DSP token and rows 2-3.
  `kCPUReserve` in `StandaloneEditor::resized()` derives from `kWidth + 12` so the ribbon can
  never underlap the readout (fixes the old 120-reserve vs 160-label gutter mismatch).
- **Ribbon tabs** are two-row (L25): name on the top row (`kNameRowH` 22), badge + dropdown
  arrow on the bottom row; the arrow hit zone is the bottom-row right (a name-row click
  anywhere navigates).  The camelCase wrap machinery (`splitCamelCase` / `slotWraps`) is
  retired — long labels shrink via `drawFittedText` instead of wrapping — and
  `naturalSingleLineWidth` no longer adds arrow/badge width.  The frozen-tab dot moved to the
  bottom row's left edge, clear of the name.
- **Tab order** (L27): `order[]` is now Builder / Mixer / Effects / Piano Roll, then the
  instance types.  Order array only — the persisted `addFixed` tab ids are untouched.
- **`kMaxSlots` corrected 11 → 12**: TS6's Plugins type made it 11 types + the "+" slot; with
  every type visible the width solver's stack arrays overflowed by one.
- **App title** (L26): `VibeLAF::drawDocumentWindowTitleBar` uses stock-JUCE placement — icon +
  title centred as one unit, clamped into the title space.  Applies to every non-native
  `DocumentWindow`; the main frame is the only caller that sets an icon.

---

## 2026-08-03 — QA-Layout T2: "+" menu reorder, engine-picker retirement, engine-named dropdown adds, LiveInst rename, "Menu" button

**Files:** `RibbonTabBar.h/.cpp`, `LayersPage.h/.cpp`, `BassPage.h/.cpp`, `ClipsPage.h/.cpp`
(`Source/Clips/`), `DrumPage.h/.cpp`, `SharedUI.h/.cpp`, `StandaloneEditor.h`,
`InstPage.cpp` (`Source/Inst/`), `MixerPage.cpp`

- **"+" menu** rebuilt to Jeff's locked order (L2/L3/L28): BaySickVocal · BaySickLiveInst ·
  BaySickGuitars · BaySickBasses · VSTPlugin ▸ · Harmless ▸ (Layers/Bass) · BaySickSynth (flat →
  Layers) · BaySickPlayer ▸ (Layers/Bass/Audio Clips) · BaySickBass · **BaySickDrums ▸
  (BaySickPlayer/BaySickSynth — absorbs the old drum routes)** · BaySickRustyDrums.  The VSTPlugin
  list stays alphabetical (`getAddedInstruments()`).
- **Ribbon dropdown add rows** (Jeff map 2026-08-03): the generic "+ Add New X" rows (which made
  ENGINELESS pages) are replaced by engine-named rows per type — Layers: Harmless/BaySickPlayer/
  BaySickSynth; Bass: Harmless/BaySickPlayer/BaySickBass; Drums: BaySickPlayer/BaySickSynth (+
  existing Rusty row); Vox: BaySickVocal (+ export submenu); Inst: BaySickLiveInst (+ Guitars/
  Basses rows); Clips: "+ Add BaySickPlayer…" (file-picker route unchanged); Plugins: "+ Add
  VSTPlugin ▸" side menu, alphabetical.  All ride `onAddEngineRequest` — the engine loads at
  creation.
- **Engine pickers deleted (L4):** Layers/Bass `LockableCombo` + "Engine:" label rows, Clips'
  decorative locked BaySickPlayer combo, Drums' "Pick a sound v" button + sound label.  Each
  page's engine context menu merged into its Menu-dropdown `showPageActionsMenu` (Lock /
  Polyphony / Rename / Duplicate / Choke / Save Patch / Load Preset + page-preset entries + ONE
  Delete).  DrumPage's Menu forwards to `showContextMenu(anchor, false)` — kit pads keep
  `fromKit=true` (MIDI Note/Learn rows, no page presets); empty-drum sound picking lives on the
  kit grid's per-pad pickers.  Clips' "Load Page Preset" keeps the wider factory+user recursive
  root from its old context menu.  Engine editors now fill their Player tabs full-height.
- **LiveInst rename (L2):** new live-input tabs are "LiveInst N" (`nextInstTabName`), InstPage
  title default, mixer strip default name + `getInstStripName` fallback, input-picker header
  "LiveInst Input" (was "Instrument Input").  Family-generic "Inst" labels that also cover
  Guitars/Basses (automation "Inst N" prefix labels, "Inst Bus", browser category, channel-list
  fallbacks) deliberately stay "Inst".
- **Hamburger → "Menu" (L31):** PageMenuBar's "=" TextButton is now a 46px "Menu" button
  (`kMenuBtnW`); title x-offset follows.  It is the app's only hamburger-style button — every
  window title strip shares PageMenuBar.

---

## 2026-08-03 — QA-Layout T3: window title strips — fill toggle, dissolved engine title bars, preset buttons on the strip

**Files:** `WorkspaceWindow.h/.cpp`, `SharedUI.h/.cpp`, `VibePlayerEditor.h/.cpp`,
`BaySickSynthEditor.h/.cpp`, `BaySickBassEditor.h/.cpp`, `HarmlessEditor.h/.cpp`,
`BaySickPedalsEditor.h/.cpp`, `LayersPage.h/.cpp`, `BassPage.h/.cpp`, `DrumPage.h/.cpp`,
`ClipsPage.h/.cpp`, `BaySickRustyDrumsPage.cpp`, `InstPage.h`, `StandaloneEditor.cpp`

- **Full-screen toggle (L5 — reverses locked call 5a's no-maximize):** every `WorkspaceWindow`
  gets a `FillToggleButton` left of close (path-drawn maximize/restore glyph).
  `toggleWorkspaceFill()` fills the workspace and toggles back to the pre-fill bounds; a manual
  drag or resize while filled clears the state.  Button order right-to-left: close, fill toggle,
  then the PageMenuBar's right-extras (preset button etc.).
- **Engine title bars dissolved (Window-4/L2):** VibePlayer / BaySickSynth / BaySickBass /
  Harmless / BaySickPedals editors lost their internal `BaySickTitleBar` (32px reclaimed —
  content starts at 0).  The colored player name now renders CENTERED on the window title strip
  via `PageMenuBar::setCenterTitle` (BaySickTitleBar's bloom painter, 15pt).  Each editor keeps
  its accent/name as `getEngineTitle()`/`getEngineAccent()` statics.  `VibePlayerEditor::
  setInfoText` deleted (caller-less; its target bar is gone).
- **Preset buttons on the strip (Window-3):** editors still OWN their `BaySickPresetButton`s;
  the pages expose `stripEngineTitle/Accent/PresetButton()` and fire `onEngineEditorRebuilt`
  after `selectEngine`, and StandaloneEditor's page-show branches mount the button via
  `addExtraRightComponent` (88px) + set the center title.  The rebuild callback matters because
  the add path SHOWS the page before the engine lands, and a Drums kit-pad pick can swap the
  engine while visible.  `PageMenuBar::ExtraComp` now holds SafePointers (an editor-owned
  mounted component can die on engine swap; dead entries are skipped).  RustyDrums' Player
  Preset (110px) + Program combo (160px) moved from the Aria bar back onto the strip (reverses
  G-16); the Aria bar itself stays (Guitars/Basses/Rusty identity).  BaySickPedals' strip mount
  lands with its T4 window.
- **L23:** the live-input Inst page's "(no audio loaded)" clip-name label mount is gone —
  nothing ever updated it on a live-input page.  The label member stays (sfizz program display
  on the Aria bar); its caller-less `getClipFileLabel()` accessor deleted.
- **L31 correction (Jeff, 2026-08-03):** the T2 cut shipped the "Menu" entry as a chrome
  `TextButton` — wrong read of "text button".  It is now `TitleStripMenuItem`: a flat
  native-menu-bar-style text heading (like "File" on a main window) with only a hover/press
  highlight, same dropdown behavior.  One shared class → every window strip corrected at once.

---

## 2026-08-03 — QA-Layout T4: Window-7 — Vox sub-page windows, LiveInst restructure, per-instance dropdown window lists

**Files:** `BaySickVocalEditor.h/.cpp`, `BaySickPitchEditor.h/.cpp`, `BaySickPitchSubEditor.h/.cpp`,
`BaySickVocalProcessor.cpp`, `SaturationDSP.h`, `VoxPage.h/.cpp`, `InstPage.h/.cpp`,
`RibbonTabBar.h/.cpp`, `StandaloneEditor.h/.cpp`

- **Vox (Window-7/L9):** `BaySickVocalEditor` is no longer a tab switcher — its content is the
  BaySickVocals main panel only (whose internal title bar dissolved; the Vox window strip shows
  the centered "BaySickVocals").  The four former sub-tabs (Vocal Chain / BaySickPitch /
  BaySickAlign / NAM-IR) are contained windows: `openVoxSatelliteWindow` hosts the editor-OWNED
  panels non-owned through `PanelSatelliteView` (per-tick resolver; a dead target closes the
  window).  The Vox strip carries four launcher slots (activeIdx −1) that open them.  Keys:
  `voxsat:<idx>:<kind>`, Session persistence until T5.
- **LiveInst (L10):** a live-input Inst tab has NO page window — the pedals window IS its player
  (tab click opens/fronts it; `hostPageInWindow` refuses LiveInput InstPages so the load sweep
  can't frame one).  The pedals window strip carries the page Menu, centered "BaySickPedals",
  the pedalboard preset button (T3's deferred mount), and a NAM/IR launcher.  sfizz Inst tabs
  frame the page (Aria player) with Pedals / NAM/IR / Piano Roll strip slots (D6: they carried
  both before, they keep them).  `InstPage` sub-tab machinery retired; Pedals/NAM-IR editors
  stay page-owned, satellite-hosted (`instsat:<idx>:<kind>`).
- **Ribbon dropdown "Pages:" (L11/D4=c):** now a per-instance WINDOW list built by
  `StandaloneEditor::buildPageWindowRows` — one body serves the labels AND the pick (rebuilt at
  pick time, so indices can't go stale).  LiveInst rows read "Pedals" / "NAM/IR".  EVERY type's
  list ends with "Pre EQ" / "Post EQ" rows opening that strip's EQ windows (Rusty uses the kit
  bus).  The old mislabeled "EQ" row (it opened the Vocal Chain on Vox since J-6) is gone;
  `onSubPageSelected` slimmed to Effects/Builder.
- **Escapes (pre-re-hosting, per plan):** `BaySickPitchEditor::showSendNotesMenu` uses injected
  `onListNoteTargets`/`onSendNotes` (wired at Vox spawn); `BaySickPitchSubEditor` title updates
  via injected `onTitleChanged`.
- **L22:** vocal-chain `sat_type` widened 0..2 (default Console) + `SaturationDSP` default
  Console — Tape now sticks.
- **VoxPage cleanup:** caller-less `showEngineContextMenu` merged into the Menu dropdown
  (restores Lock/Rename/Duplicate + the factory preset root); dead picker scaffolding removed.
- Vox/Inst tab close now closes that instance's satellites.

---

## 2026-08-03 — QA-Layout T5: window-state persistence — the three-lifetime model

**Files:** `WorkspaceWindow.h/.cpp`, `StandaloneApp.cpp`, `StandaloneEditor.h/.cpp`

- **Lifetime 1 (universal, in-memory):** the session map is now the ONE live store — every
  window (Disk or Session) writes it on move/resize/close and reads it on open.  Close/reopen
  returns to the same spot for every window type, players included.
- **Lifetime 2 (settings.xml):** written ONCE at app exit (`writeSessionToSettings`, after
  editor teardown so destructors flush final bounds) from a filtered view — SIZES for every
  page window, PLACEMENT only for the four default tabs (Mixer/Builder/Effects/Piano Roll).
  Player x/y attributes are stripped from old records.  The parse-and-rewrite-whole-file-per-
  close smell is gone; the file is read only as a seed on a map miss (size-only records take
  the default cascade position).
- **Lifetime 3 (project file):** `serializeUIState` stores the full map + an `Open` record per
  live window (pages by persist key, satellites/effect windows by aux key); load REPLACES the
  map and frames exactly the saved-open set — the load path no longer force-opens every page
  (`mLoadingWindows` guard in `hostPageInWindow`; aux keys re-dispatch through their open
  functions).  Pre-T5 projects (no `<Windows>`) frame nothing; tabs are one ribbon click away.
- **L16 crash survival:** rides the existing 15-min autosave — the serializer flushes all live
  window bounds into the map before writing, so a crash loses at most one autosave interval of
  layout.
- **persistKeyFor defect fixed:** the index fill covered only Layers/Bass/Drums, so every
  Clip/Vox/Inst/Plugins window shared one "type:-1" key (one saved position for all).  One
  resolver (`pageIndexOfEntry`) now serves the key, the hint fill, and the load-time matcher.

---

## 2026-08-03 - QA-Layout T9: piano-roll control lane resize (L29)

**Files:** `PianoRoll.h/.cpp`, `DrumKitGrid.h/.cpp`, `StandaloneEditor.cpp`

- **Header-drag resize:** the 16px lane header doubles as a resize handle - drag
  vertically to set the lane height (min = collapsed to just the header, max = the
  old fixed 240); a clean click still opens the mode dropdown (menu moved from
  mouseDown to mouseUp behind a 3px drag threshold).  Up-down resize cursor over
  the header.
- **ONE shared height:** `ControlLane::get/setUserHeight` statics - every lane in
  the app (embedded rolls, the Piano Roll page, both DrumKit lanes) shows the same
  height; other containers lockstep off their existing 200ms timers.  The grid
  keeps its 120px floor; `kLaneH` fixed-height aliases deleted from both
  containers.
- **Persistence:** `<ControlLane h= visible=>` in the project `<UIState>` (rides
  autosave/crash-flush like the T5 `<Windows>` block).  `visible` is the last
  settled Velocity Lane toggle from ANY container - the default new containers
  open with; in-session toggles stay per-container.  Restored before the tab
  rebuild on load.

---

## 2026-08-03 - QA-Layout T15: strip nav buttons into the Menu dropdown + sfizz titles

**Files:** `StandaloneEditor.cpp`, `SharedUI.h`, `LayersPage.h/.cpp`, `BassPage.h/.cpp`,
`ClipsPage.h/.cpp`, `DrumPage.h/.cpp`, `VoxPage.h/.cpp`, `InstPage.h/.cpp`,
`PluginsPage.h/.cpp`, `BaySickRustyDrumsPage.cpp`

- **Every player page's title-strip buttons (between Menu and the swing knob) are
  gone; the same entries live at the top of that window's Menu dropdown** (Jeff's
  mid-sizing ruling, 2026-08-03).  Per page: Rusty + Drums {Drum Kit, Player,
  Piano Roll}; Layers/Bass/Clips {Player, Piano Roll}; Vox {Vocal Chain,
  BaySickPitch, BaySickAlign, NAM/IR}; Inst {Pedals, NAM/IR, Piano Roll};
  Plugins {Piano Roll}.  Local-view entries show a tick on the active view.
  Mechanism: a `onBuildWindowNavMenu` hook on each page, invoked at the top of
  `showPageActionsMenu` (DrumPage: `showContextMenu`, !fromKit only); Rusty's
  editor-side menu builder gets the entries directly.  Entries are JUCE
  action-lambda items (itemID -1, self-dispatching) so they coexist with the
  pages' id-dispatched menus.  EXCLUDED per Jeff: the Piano Roll page's jump
  cluster, the pedals window's NAM/IR launcher, the EQ windows' Pre/Post pair.
- **Drums' Menu installs unconditionally** (was Player-sub-tab-only) so the nav
  entries are always reachable; post-J-6 Player is the only local sub-tab anyway.
- **The small grey tab title returns to player strips** (it was suppressed while
  tab slots existed) - each window shows its instance name at the left again.
- **sfizz titles (the missed T3 treatment):** BaySickRustyDrums, BaySickGuitars,
  BaySickBasses no longer draw the internal AriaControlPanel title band; their
  names render centered on the title strip (Rusty red #CC2222, Inst navy
  #1C3A8A).  The four widgets the Inst band hosted (program label + Load button,
  CUT SELF + mode toggles) mount on the strip as right extras; the CUT SELF
  APVTS attachments are wired independently of any title bar now.

---

## 2026-08-04 - QA-Layout T10: mixer menus, Add heading, group buses, routing menus

**Files:** `VibeGraph.h/.cpp`, `PluginProcessor.h/.cpp`, `MixerPage.h/.cpp`,
`EffectsPage.cpp`, `StandaloneEditor.cpp`, `SharedUI.h/.cpp`, `PagePresetIO.cpp`

- **L13 - "Add" titled menu:** second flat native-style heading right of "Menu"
  (strip reads "Menu  Add"; PageMenuBar::setAddMenuBuilder, hidden on pages
  without a builder).  Mixer's Add menu = the ruled seven rows: Aux Strip, Vox
  Bus, Inst Bus, Layers Bus, Bass Bus, Clips Bus, Plugins Bus (bus rows grey at
  cap).  The five title-strip add buttons are DELETED (Vox/Inst STRIP adds live
  on the ribbon "+" flow, which already creates the strip).
- **Four secondary group buses** (kLayersBus2 14, kBassBus2 15, kClipsBus2 16,
  kPluginsBus2 17) on the full kVoxBus2 pattern: registry rows, always-allocated
  graph nodes, params, render tasks, EQ tables, meters, Effects dropdown ids
  14-17, automation labels, active-gated route rules, preset fallbacks.
- **L14 lifecycle:** per-secondary-bus has-ever-had-route flag; a fresh bus stays
  visible while never-routed, auto-deactivates once used-then-emptied.  New
  <Buses> element in the structural UIState serializer persists activation +
  everRouted for all seven secondary buses; clearDynamicStrips resets them on
  project load (Vox2/Inst2/3 previously leaked across projects).  A routed-to
  inactive secondary bus SELF-ACTIVATES at layout time (preset/project loads
  write _sendTo before any flag arrives).
- **L12 - "+" target menus:** the per-strip "+" now shows Send... / Sidechain...
  / Move Output... submenus enumerating concrete legal targets (filtered by
  isValidBusSendTarget / isRouteAllowed / wouldCreateCycle; illegal = disabled
  row; current main-out ticked; "New Aux Strip" row preserves the old
  auto-create).  The click-to-place send/SC modes, the main-out socket drag,
  the ghost cables, and the red rejected-drop flash are RETIRED; cable painting
  + right-click cable menus (delete, amount, pre/post) stay; Master's "+"
  stays the Analyzer launcher.
- **L30:** "MIDI trigger velocity" moved from the Mixer Menu to the Audio
  Settings dialog as a "Trigger Velocity:" combo below the MIDI inputs (applied
  live, settings.xml persistence unchanged; dialog height +1 row).

---

## 2026-08-04 - QA-Layout T11: instance caps (L18) + two-sixteens drum kit (D3)

**Files:** `VibesynthConstants.h`, `VibeGraph.h/.cpp`, `PatternManager.h/.cpp`,
`PluginProcessor.h/.cpp`, `MixerPage.cpp`, `EffectsPage.cpp`, `BuilderPage.cpp`,
`StandaloneEditor.h/.cpp`, `DrumKitGrid.h/.cpp`

- **L18 caps:** Layers 20, Bass 10, Drums 32, Clips 100, Vox 10, Inst 30 - each
  page cap mirrored by a kMax*Strips constant in MixerChannelIds (new
  kMaxLayerStrips/kMaxBassStrips/kMaxDrumStrips/kMaxAudioStrips; Vox 6->10,
  Inst 20->30).  kMaxAudioRows/kMaxAudioInserts 50->100 (static_assert keeps
  the trio locked).
- **PR-target shift (accepted):** the PRPendingOff target bases are derived by
  summing the caps, so pre-existing projects' piano-roll routing is invalidated
  once.
- **Literal sweep:** every stale range-check literal replaced with the
  constants - VibeGraph prefix/friendly/defaultSendTo tables + pushScArrayToStrip
  (+16/+50), MixerPage color/route/aux checks, EffectsPage aux dropdown (16->18)
  + audio dropdown (450->500), PatternManager ownerCategory (Vox 606 / Inst 706
  were stale BUGS below the real strip counts), BuilderPage group-assign +
  clip-block colors (same stale bug), PluginProcessor EQ-sync tables (8/4/16/50),
  StandaloneEditor mUsedLayerIndices (literal 8 would have overflowed at cap 20),
  PatternManager layerRoll (literal 8, same overflow).
- **D3 (ruled 1c + 2a): two-sixteens drum kit.**  ONE "Drum Kit" PR target; the
  kit view gains a "1-16 / 17-32" switch beside the Kit button.  Mapping is
  FIXED by page index - drum pages 1-16 belong to view 1, 17-32 to view 2; a
  drum never moves between views (deleting one leaves a gap in its own view).
  DrumKitContainer holds the raw row provider + handlers; children see a
  view-filtered sixteen and row indices are translated back before handlers
  fire (row-click, audition, reorder; the sidebar add row maps past the raw
  end so the add branch still fires - a new drum always fills the lowest free
  page slot, so an add from view 2 can land in view 1).

---

## 2026-08-04 - QA-Layout T7: real floors + Harmless four-column rework + VibePlayer knobs + pedal tile grids

**Files:** `WorkspaceWindow.h/.cpp`, `StandaloneEditor.h/.cpp`, `EffectWindows.h/.cpp`,
`EffectRack.h`, `EffectPresetIO.cpp`, `EffectEditorPanels.cpp`,
`Harmless/HarmlessEditor.h/.cpp`, `VibePlayer/VibePlayerEditor.cpp`

- **Real floors (Jeff's approved sizing map, full-window dims):** the diag-era
  120x80 free-for-all is over - WorkspaceWindow's setMinimumSize body restored,
  new setMinimumWindowSize for map-dim callers.  Page windows floor via
  StandaloneEditor::floorSizeFor (engine-aware for Layers/Bass/Drums: Harmless
  1047x455, BaySickPlayer 490x455, Synth/BaySickBass 558x455; re-applied per
  show so Drums' live swap tracks).  Satellites: Vocal Chain 1047x723, Pitch
  1534x724, Align 1047x723, NAM/IR 843x563, Pedals board 1534x455, EQ +
  Analyzer 1047x455.  Effect windows: open at 358x268 and re-floor live via a
  new onFloorChanged hook (Basic 691x268 / Advanced 1047x268 / pedal-native
  358x268; pushed on the poll so Mode swaps + Basic toggles re-floor).  Hosted
  plugin windows keep plugin-derived floors (T12).
- **Harmless (Specific-2): the layout is REDONE, not re-hung** (Jeff's
  correction 2026-08-04 - re-flowing the old sections into columns was not the
  ask).  Design size is now the approved window (1035x425 content, was a tall
  960x620 box).  Two root causes fixed:
  - **Overflow:** `layoutRow` laid every item in ONE row and let wide sets run
    past the cell edge (Output, Timbre, FX).  It now WRAPS into as many rows as
    the cell width needs and centres the block, so a section fits at any width.
  - **Dead space:** a blank grid row, a blank bottom-left half and a blank
    row-C half were reserved as "future space" while real sections were
    squeezed.  Every cell now carries content.
  New map - TOP band: left column Output / (Tremolo | Routing) /
  Vibrato-Legato; middle column Unison alone at full height (its three faders
  are the only inherently vertical control set, and they get real throw back);
  right column (Filter 1 | its ADSR) / (Filter 2 | its ADSR) / Timbre at FULL
  width.  BOTTOM band, six columns: Pitch over LFO Mod | Strum over XYZ |
  Blur-Prism over Amp Env | FX | Spectrogram | Mod Editor.  The cramped 2x2
  filter-offset/part-mask stack inside Timbre is dissolved - those four knobs
  sit inline at full size in the full-width row instead of shrinking to ~12px.
  Part A/B dual-bind + rebindToPart untouched.
- **VibePlayer (L15/Specific-1): kKnobSz 55 -> 18** with the knob+label stack
  now vertically centered per cell; routing-arrow centers follow the same math.
- **Pedal tiles (Specific-4):** every pedal-capable panel now has a
  PanelMode::Pedal branch - a shared pedalTileGrid (generalizing the
  Octave/FurmanEQ hand-built grids) grids knobs + selectors/buttons per tile;
  fader-bank EQs + the Tuner keep their layouts and reclaim the dBFS strip.
  isPedalNativeType moved to EffectRack.h (shared with preset routing).
- **Subtractive-math sweep disposition:** the restored floors make every
  below-design-size state unreachable; no live sub-floor paint path remains to
  sweep.  (Flagged at commit for Jeff's veto.)
- **Window placement fixes (Jeff, 2026-08-04 - the Mixer would not hold its
  spot).**  FOUR causes, found in order, all fixed.  The last one was the
  headline bug and was only isolated by tracing save/write/restore to a file:
  1. A project stored the four default tabs' bounds AND replaced the whole
     in-memory map on load, so its stale copy beat the position just saved to
     settings.xml.  Projects no longer write or restore those keys, and live
     global entries survive a project load.  The key set is seeded at editor
     construction - registering it at first framing was too late when a
     project loaded first.
  2. The teardown save persisted whatever a half-dismantled window reported,
     and it ran LAST so it won.  The destructor no longer saves; every real
     route saves explicitly (drag-release, resize/move, close button, fill
     toggle) plus one flush at shutdown BEFORE editor teardown, while windows
     are still alive and placed.
  3. **Restore ran against a workspace that was not laid out yet.**
     `originInParentClient()` returned (0,0) instead of the real (1,91), so
     every restored window landed short by the origin - 91px up, every
     launch.  It also collapsed the first-open default size (a fraction of the
     workspace) to its 480x320 floor, which is what littered settings.xml.
     Attach now waits for a workspace with real bounds, not merely a native
     handle, and a fresh window opens at its own measured minimum instead of a
     workspace fraction.
  4. **The clamp captured windows during startup layout.**  The frame passes
     through a partial size (~1098x608) before reaching 1534x724, and
     `clampWindowsIntoView` squeezed every window into that partial size with
     nothing to put them back once it grew.  A GROWING workspace now re-applies
     each window's stored bounds before clamping again; programmatic clamps
     stay out of the store via a scoped suppression, so a clamp can never be
     mistaken for a placement.
- **Kit load no longer frames its drums (Jeff, 2026-08-04).**  Loading a
  16-drum kit put 16 player windows on screen.  Tabs, mixer strips and piano
  rolls are still created; the window for a drum appears when that drum's tab
  is selected.  The post-load landing spot moved from "the first new drum's
  player" to the DRUM KIT view, so the load still shows something without
  opening a player.  The one-shared-window-with-a-dropdown alternative is
  Future State CL-305.

---

## 2026-08-04 - QA-Layout T16: title-strip consolidation + Builder menu/grid/browser rework

**Files:** `SharedUI.h/.cpp`, `StandaloneEditor.cpp`, `BuilderPage.h/.cpp`,
`AriaControlPanel.h/.cpp`, `BaySickRustyDrumsPage.cpp`, `Inst/InstPage.h/.cpp`,
`BaySickNAMIR/BaySickNAMIREditor.h`, `BaySickVocal/BaySickPitchEditor.cpp`,
`BaySickVocal/BaySickAlignEditor.cpp`, `Harmless/HarmlessEditor.cpp`

- **FX Rack and Freeze are menu items, not strip buttons.**  `setFxRackSlot` /
  `setFreezeSlot` keep their registration signatures but now only store
  callbacks; `PageMenuBar::appendStandardItems` emits both into whatever menu a
  page is building, called from each page's `onBuildWindowNavMenu`.  Rusty has
  no FX Rack, so it gets Freeze alone.  Piano Roll is deliberately untouched -
  its `Player Page` / `FX Rack` slots are hotkeys back to the thing being
  edited, which is a different job from the per-window action list.
- **Freeze shows LOCKED rather than hidden (Jeff, 2026-08-04).**  It used to
  vanish entirely unless "Enable Instrument Level Freeze" was ticked in File
  Settings, so a user who had heard of freeze had no way to learn it existed.
  It now renders greyed with the unlock path in its tooltip.  JUCE popup items
  carry no tooltip, so the entry is a `PopupMenu::CustomComponent` that also
  implements `TooltipClient`.
- **Swing Mix knob moved to the far left**, immediately right of the `Menu`
  heading, so its position is fixed rather than drifting with whatever nav
  widgets a page mounts.
- **Inst (Guitars / Basses):** `CUT SELF` + cut-mode moved OFF the strip onto
  the player's own top-left corner - they act on that engine's voices, so they
  belong with the engine.  Clip-file label 200 -> 133 px and Program button
  130 -> 87 px (both 2/3) to give the strip its width back.
- **Logos moved to the hosting window strip, one name per window.**
  BaySickPitch and BaySickAlign hide their internal `BaySickEngineLabel`
  (added-but-hidden so their toolbars keep reserving the slot and nothing else
  shifts); BaySickNAM/IR's internal `BaySickTitleBar` is nameless and exposes
  `getEngineTitle` / `getEngineAccent` for the strip.  Vocal Chain has no logo
  of its own and falls through to the centered plain title.
- **Rusty's Aria title band is back.**  T15 dissolved it along with the engine
  name, which left the Program selector and Player Preset button on a strip too
  narrow to hold them - half sat on the player, half behind the window chrome.
  `AriaControlPanel::Binding::hostTitleBar` keeps the band with NO name, the
  two controls are hosted on it again, and the strip's centered
  "BaySickRustyDrums" is gone because the kit artwork already carries the logo.
- **Title text rule (Jeff, 2026-08-04):** a window with a logo shows no plain
  title text; a window without one CENTERS it instead of pinning it left.
- **Harmless `layoutRow` scales an over-tall block down.**  Wrapping alone was
  not enough: a block taller than its cell centered and spilled past both
  edges, which is what put FILTER 1 on FILTER 2 and pushed the Amp Env RAND row
  into its neighbour.  Width scales with height so knobs stay round, and since
  the factor is <= 1 the wrap decided at full width stays valid.
- **Builder's own `Edit / Tools / Clips / View` row is deleted** and the grid
  moved up into its 20 px.  Clips folds into the window `Menu`; Edit and View
  became title-strip headings via the new
  `PageMenuBar::setExtraHeadings`; Tools was removed outright because all eight
  entries (Draw / Paint / Select / Delete / Mute / Slice / Zoom / Play
  Selected) duplicate toolbar buttons one row below.  `BuilderMenuBar` and its
  `MenuBarComponent` are gone.
- **Browser collapse is a magnetic ramp.**  The `<<` button and the
  `View > Toggle Browser` entry were two click-paths to the same
  `setCollapsed()` with no drag path at all; both are removed.  The edge grip
  now snaps to the floor within 14 px and collapses when pushed 44 px past it.
  The collapsed 28 px panel is the pull-back handle - grip texture, arrow, and
  a click to reopen.
- **Track-header corner is blank and clipped.**  It carried a "BUILDER" caption
  AND the row loop drew into it, because a scrolled row 0 lands at
  `kRulerH - mYOffset` which is negative - track names scrolled up over the
  ruler.  Rows are clipped to the band below the ruler and the corner repaints
  opaque afterwards.
- **Vertical zoom decoupled from window size.**  The Alt+scroll clamps were
  `(vpH - kRulerH) / 50` and `/ 8`, so the same gesture bottomed out at a
  readable ~12 px row full-screen and a ~4 px row in a small contained window -
  every row crushed into its neighbour.  Replaced with absolute
  `kMinRowH = 16` / `kMaxRowH = 96`, so one gesture means one thing at any
  window size and rows can no longer overlap.
- **Right-click Automate restored inside contained windows (Jeff-reported,
  2026-08-04) - a QA-ModelShell regression, not a T16 one.**  StandaloneEditor
  installs one `GlobalAutoRightClick` via
  `addMouseListener(&mAutoRightClick, true)`, and "nested children" means
  components in ITS OWN tree.  Once pages moved into `WorkspaceWindow`s - real
  native child peers with separate trees - that listener stopped seeing a
  single click inside them.  `VKnob`-based controls never noticed (a VKnob
  listens to its own slider and tags it `vknob_slider` so the global handler
  skips it), which is why the effect panels and pedals kept working; every
  `VibeSlider` went dark, because VibeSlider swallows the right-click on
  purpose and depends entirely on that listener to raise the menu.  That cost
  the players and the mixer strips their Automate menu, silently, since the
  shell landed.  Each `WorkspaceWindow` now owns a `GlobalAutoRightClick` and
  installs it over its own subtree - the same per-contained-window pattern
  already used for `TooltipWindow`, and it covers page windows, satellites,
  effect-slot and EQ windows in one place.  Verified no path can double the
  menu: VKnob is skipped by tag, `AriaControlPanel`'s sliders set no
  componentID so the handler bails, and the EQ `DynamicParamsPopout` is a
  CallOutBox peer outside both scopes with its own local mirror.  Keyboard was
  never affected - key listeners were already installed per contained window.
- **Ribbon "+" slot sized to its glyph (Jeff, 2026-08-04).**  It was laid out as
  an ordinary slot: floored to `kMinFixed` / `kMinVariable` AND handed an equal
  share of every leftover pixel in the bar, so on a wide transport bar it
  ballooned into a large empty block while the real tabs stayed narrow.
  `addSlotWidth()` is now twice the width of the "+" glyph at the same 18pt
  font paint() draws it with, carved off the right edge first; the type slots
  divide what remains.
- **25 px moved from the perf readout to the tabs.**
  `TransportPerfReadout::kWidth` 120 -> 95.  120 was sized for a worst case
  ("MEM 9999  LAT 99999") needing ~10 GB of process memory or five figures of
  plugin latency.  Because that box CAN now truncate, and truncation there is
  silent -- rows 2/3 ellipsize but row 1 draws SYS/DSP as two exact-width
  right-anchored segments, so an over-wide pair pushes SYS off the left edge
  with no marker -- the tooltip carries the LIVE values above the legend and is
  rebuilt whenever a value changes.  Hover always yields the full numbers.
- **Tooltips promoted to a single desktop window (Jeff-reported, 2026-08-04) -
  another QA-ModelShell z-order regression.**  `VibeTooltip` was constructed
  with the editor as its parent, and JUCE's `displayTipInternal` forks on that:
  with a parent it positions inside the parent and DRAWS THERE; parentless it
  goes through `addToDesktop`.  A native child peer always renders above
  anything drawn into its parent, so every tip raised from the transport bar
  dropped behind the page windows - the perf readout, BPM and the position
  display all appeared to have no tooltip at all when in fact the box was
  painting underneath the workspace.  The editor's tooltip is parentless now.
  Consequently the per-`WorkspaceWindow` tooltips (added 2026-07-28 for exactly
  the reach problem this removes) and `KeyBindsContent`'s local one are GONE: a
  parentless tooltip's peer gate always passes, so leaving them in raised two
  tips at once.  One tooltip window now serves the whole app and can extend
  past a contained window's edge instead of being clipped by it.  Knob VALUE
  bubbles were never affected - `Slider::PopupDisplayComponent` is a
  `BubbleComponent` on the same `addToDesktop` route already, which is why
  those stayed visible throughout and is the precedent this follows.

---

## 2026-08-04 - QA-Layout T17: window sizing rework + Harmless re-layout

**Files:** `WorkspaceWindow.h/.cpp`, `StandaloneEditor.h/.cpp`, `SharedUI.h/.cpp`,
`RibbonTabBar.h/.cpp`, `GlobalTransportBar.h/.cpp`, `KeyBindsWindow.h`,
`Harmless/HarmlessEditor.cpp`, `Harmless/HarmlessModEditor.h/.cpp`,
`Harmless/HarmlessFilterRow.cpp`, `Harmless/HarmlessRoutingMatrix.cpp`

### Window sizing / persistence

- **No fake floor.**  `floorSizeFor` answered 490x455 for "engine not bound
  yet" -- which is BaySickPlayer's REAL floor, so an unresolved window was
  indistinguishable from a legitimately small one and nothing downstream knew
  there was anything left to correct.  It was also the smallest of the three, so
  the failure always erred toward too-small.  Renamed `defaultSizeFor`, returns
  `std::optional`, and answers NOTHING when the engine is unbound.
- **Sizes install when the engine binds**, via each page's
  `onEngineEditorRebuilt` -- the same callback that sets the strip's engine
  name.  That also tracks a live Drums engine swap.
- **Plus a healing sweep** (`pollPendingWindowDefaults`, riding the existing
  5 Hz `DenoisePollTimer`).  `onEngineEditorRebuilt` is a SINGLE callback slot,
  so an engine that bound before `showPageForTab` installed the slot fired into
  nothing and that window kept the ctor placeholder forever -- intermittent by
  nature and likelier on a second player, whose engine loads faster once warm.
  A window without a default is a known-incomplete state, so it is polled rather
  than depending on catching one event.
- **Content minimums SUSPENDED (Jeff's call).**  Only the 120x80
  anti-degenerate clamp survives until the compact-layout task decides real
  minimums.  The measured numbers are now purely the DEFAULT OPENING SIZE.  The
  ctor's 320x200 was both a resize floor AND the first-open size, which is how
  an unresolved window opened at 320x200 and looked deliberate.
- **settings.xml carries ONLY the four default tabs** (Builder / Mixer /
  Effects / Piano Roll), size AND position.  It previously defaulted every page
  window to `Persistence::Disk` and wrote every window's SIZE globally while
  gating only POSITION to the four -- the exact inverse of the three-lifetime
  ruling, and what left 143 records feeding stale sizes to players on every cold
  start.  Everything else is Session: session state plus project content, and a
  cold start with no project opens it at its default.  The stale records were
  stripped (backup: `settings.xml.bak-preclean`).
- **Resize magnetism.**  `applyMagnetism` was only ever called from
  `mouseDrag`, so the magnet worked when MOVING a window and did nothing when
  resizing one.  `applyResizeMagnetism` snaps each edge independently and only
  edges that actually moved -- translating the whole window is right for a drag
  and wrong for a resize.

### Harmless

- Every knob halved (44/32 -> 22/16); all four filter knobs one size (FREQ and
  RES were 44 against ENV/KB's 32); the type combo 80 -> 56.
- Faders converted to knobs: Unison PAN/PITCH/PHASE, the LFO depths, and the
  six Routing sliders (Routing lives in its own component and was missed on the
  first pass).
- **One box per filter**, ADSR included -- two boxes for eight related knobs
  doubled the chrome and left both halves half-empty.
- **`layoutRow` distributes**, and the box WIDTH comes from content
  (`natural()`).  A pass that packed instead was wrong for spacing; a pass that
  distributed without content-sizing was wrong for width.  Both are needed: the
  box is sized to what it holds, the knobs breathe inside it.
- **Horizontal strips, not narrow columns.**  Each of these sections is one row
  of knobs, and ~13%-wide columns forced `layoutRow` to wrap them into cramped
  lines -- the overlap.  Pitch+LFO Mod and Strum+FX share rows; Amp Env and
  Blur/Prism are half-width and stacked; the MOD pad spans both; the
  Spectrogram has its column to itself.
- **Labels size to their TEXT**, not the knob width + 8.  At 16px knobs that
  clipped everything past four characters (`VOIC...`, `DEPTH`, `LENGTH`).  Both
  the editor's `knobLabel` and the Mod Editor's own label drawing.
- **Mod Editor**: knobs 34 -> 16; its tool row shrinks its buttons to fit ONE
  row (wrapping to two steals height from the envelope graph, which is the one
  thing that box exists to enlarge).
- **Snap + grid** now use the app's unified divisions from
  `VibesynthConstants.h`, triplets included -- this was the one place in the app
  a triplet could not be snapped to.  Segment counts derive from
  `snapDivToTicks` so they cannot drift.  The grid follows the SELECTED division
  via `gridLadderForSnap` instead of a fixed 32 lines, so every snap target
  lands on a visible line.  The tick system itself deliberately does NOT reach
  in: this axis is per-note 0-1 phase, not song position.

### Transport bar

- Ribbon `+` sized to twice its glyph and carved off the right edge first; it
  was laid out as an ordinary slot, floored to 60-80px AND handed an equal share
  of every leftover pixel.
- `TransportPerfReadout::kWidth` 120 -> 95, the 25px to the tabs.  Because that
  box can now truncate and truncation there is silent (row 1 draws SYS/DSP as
  exact-width right-anchored segments, so an over-wide pair pushes SYS off the
  left edge with no marker), the tooltip carries the LIVE values above the
  legend and rebuilds whenever a value changes.
