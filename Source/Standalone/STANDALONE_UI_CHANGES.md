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
