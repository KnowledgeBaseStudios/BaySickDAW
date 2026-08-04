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
class SnapSlider : public juce::Slider
{
public:
    SnapSlider() : juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox) {}
    double snapValue(double v, DragMode) override
    {
        return (std::abs(v) < 1.5) ? 0.0 : v;
    }
};
```

Kept for future use on the Mixer page faders. Do not remove.

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
// In StandaloneEditor ctor — order matters:
addAndMakeVisible(*mTransport);    // z=background (paints brushed-aluminum for full bar)
addAndMakeVisible(*mTitleLabel);   // hidden
addAndMakeVisible(*mPatternBox);   // on top of transport
addAndMakeVisible(*mAddPatternBtn);
addAndMakeVisible(*mPatternNameEdit); // hidden
addAndMakeVisible(*mRibbon);       // on top of transport
addAndMakeVisible(*mPageMenuBar);
```

### GlobalTransportBar changes
- `PlayModeCombo` (juce::ComboBox for Song/Pattern) **removed** — redundant with the `SONG` toggle button.
- Public constant `kControlsWidth = 406` added — the pixel x-position where transport controls end.
  `StandaloneEditor::resized()` uses this to start the pattern selector.
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
mPatternBox   ->setBounds(bar.getX() + kPatStart,                 py, kPatBoxW, 28);
mAddPatternBtn->setBounds(bar.getX() + kPatStart + kPatBoxW + 4,  py, kAddBtnW, 28);

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
- `mTitleLabel` — `setVisible(false)`. Title now lives in the OS window title bar ("VibeDAW").
- `mPatternNameEdit` — `setVisible(false)`. Rename will move into the Pattern▾ dropdown (pending).

### Window title
`VibeSynthWindow` ctor now passes `"VibeDAW"` instead of `" "` to `DocumentWindow`.

### Record button placeholder
A Record button will be added to GlobalTransportBar between ■ (Stop) and BPM when recording
is implemented. Space is available in the transport section (after the gap following Stop).

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

### What changed
RibbonTabBar completely rewritten. Now shows exactly 6 permanent slots (no + button, no close X, no overflow):

| Slot     | Dropdown? | Badge  | Dropdown contents                                    |
|----------|-----------|--------|------------------------------------------------------|
| Mixer    | No        | None   | —                                                    |
| Effects  | Yes       | ② static  | Rack / EQ                                         |
| Builder  | Yes       | ③ static  | Patterns / Audio Clips / Automation               |
| Layers   | Yes       | dynamic   | Instance list + Rename + Delete + Add New Layers  |
| Bass     | Yes       | dynamic   | Instance list + Rename + Delete + Add New Bass    |
| Drums    | Yes       | ② static  | Sounds / EQ                                       |

### How the 6-slot display works
- `slotType(int slotIndex)` maps index 0..5 → TabType in fixed order
- `slotRect(int slotIndex)` divides the ribbon width equally among 6 slots
- `isSlotSelected(int slotIndex)` checks if the currently selected tab's type matches the slot type
- `getSlotDisplayName(int slotIndex)` returns the active instance name for that type

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
- `onSubPageSelected()` added — currently a stub (TODO: wire to page sub-tab switching)
- Menu "New Drums Tab" permanently disabled

---

## Pending UI Tasks (not yet implemented)

### Sub-page switching wiring
`onSubPageSelected(type, subPageIndex)` is called when user picks from Effects/Builder/Drums dropdown.
Need to wire it to internal tab-switching methods on EffectsPage, BuilderPage, and DrumsPage.

### Pattern Dropdown (full implementation)
`[Pattern▾]` should open a popup menu showing:
- All patterns (to switch between them)
- Rename current pattern (inline or dialog)
- Delete current pattern
Currently `mPatternBox` is a plain ComboBox. Needs replacing with a TextButton + PopupMenu.
`mPatternNameEdit` is hidden until this is implemented.

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
