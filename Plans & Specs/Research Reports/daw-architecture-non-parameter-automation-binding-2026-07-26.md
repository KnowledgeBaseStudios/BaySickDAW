# DAW Architecture Research — Non-Parameter Automation Binding + Applicator Lifetime — 2026-07-26

> Dispatched via `/architecture` at Jeff's request during QA-ProjectSave, to decide between
> options (b) RAII token / (c) SafePointer-beside-closure sweep / (e) promote targets to real
> parameters, for the `mAutomationApplicators` lifetime problem (sweep item 5).
>
> **Parent-session verification (2026-07-26).** Per `feedback_verify_subagent_finding_premise`,
> the two load-bearing claims were desk-verified in our own tree before being acted on:
>
> 1. **FX-rack channel-switch hole — CONFIRMED.** Full chain read in source:
>    `mTrackBox->onChange` -> `onChannelChanged()`
>    ([EffectsPage.cpp:23](../../Source/Standalone/EffectsPage.cpp:23)) -> `setRack(rack)`
>    ([:494](../../Source/Standalone/EffectsPage.cpp:494)) -> `rebuildSlotEditor(i)` for all
>    `kNumSlots` ([:500-502](../../Source/Standalone/EffectsPage.cpp:500)) ->
>    `mSlots[i]->setEditor(std::move(editor))` ->
>    `SlotComponent::setEditor` assigns `mEditor = std::move(editor)`
>    ([SlotComponent.cpp:127-133](../../Source/Standalone/SlotComponent.cpp:127)), destroying the
>    previous panel. Switching the Effects page channel dropdown therefore destroys every slot
>    editor of the previously-viewed channel, leaving its `mixer_<chan>_<uuid>_*` applicators as
>    live-but-inert entries. Recovers on navigating back (keys are stable, so the rebuilt panels
>    re-register over them). NOT yet ear-confirmed — see step 0.
> 2. **`componentBeingDeleted` fires from the base destructor — CONFIRMED** in our vendored JUCE
>    at [juce_Component.cpp:275](../../juce/modules/juce_gui_basics/components/juce_Component.cpp:275),
>    as the FIRST statement of `Component::~Component()`. The b-prime mechanism is sound.
>
> Report body below is the agent's, applied verbatim per `feedback_drafter_output_verbatim_no_restyle`.

## Problem statement

BaySickDAW binds some automation lanes to UI widgets through a message-thread registry of
`std::function` closures keyed by paramId. Widgets can die without their tab closing, the only
unregistration path is a hand-maintained list of ~17 prefix literals, and a stale entry dispatches
successfully into a no-op, so a broken lane is indistinguishable from a working one. The question
is what mature DAWs / synths / frameworks bind automation to instead, how they manage that
binding's lifetime, and which of options (b) / (c) / (e) we should take.

## What BaySickDAW currently does

Two push-only registries owned by the editor:

- [StandaloneEditor.h:763](../../Source/Standalone/StandaloneEditor.h:763) — `std::map<juce::String, std::function<void(float)>> mAutomationApplicators`
- [StandaloneEditor.h:767](../../Source/Standalone/StandaloneEditor.h:767) — `std::map<juce::String, std::function<float()>> mAutomationValueReaders`

Installed as global static hooks so any panel can push into them without knowing the editor exists:

- `SharedUI.h:675-679` — `VKnobAutomation::sOnRegisterApplicator` / `sOnRegisterReader` declarations
- `SharedUI.cpp:1702-1703` — definitions
- `StandaloneEditor.cpp:630-641` — the editor assigns both hooks; each is a bare `map[pid] = std::move(fn)`

Five convenience wrappers do the actual capture, all with a `SafePointer` inside the closure:
`registerSliderAutomation` / `registerButtonAutomation` / `registerComboAutomation` /
`registerParameterAutomation` / `registerSelectorAutomation` at `SharedUI.cpp:1723-1839`, declared
with their ownership contract spelled out at `SharedUI.h:681-717`. 29 uses across 11 files, plus
direct hook calls at `EffectEditorPanels.cpp:211-265`, `MixerTrackStrip.cpp:454-478`, and
`SharedUI.cpp:5382-5389`.

The ownership rule is documented as deliberate at `SharedUI.h:686-688`: "the registry has no
erase-on-destroy path, so these closures outlive a closed tab — the SafePointer inside makes a dead
control a no-op, and a rebuilt tab re-registers over the stale key."

Removal:

- `StandaloneEditor.cpp:11317-11330` — `eraseAutomationEntriesWithPrefix`, a linear prefix scan of both maps
- called with ~17 hand-written literals from the tab-close paths at `StandaloneEditor.cpp:4436-4686`

Dispatch:

- `StandaloneEditor.cpp:3134-3145` — APVTS lanes resolve through `apvts.getParameter`; everything else falls through to `mAutomationApplicators.find(...)` and calls it if non-empty
- `StandaloneEditor.cpp:3189-3191` — reader used to seed a new lane
- `StandaloneEditor.cpp:896-907` — song-entry/exit baseline capture + restore through the same two maps
- `StandaloneEditor.cpp:3011-3013` — the Event Editor's param browser lists `mAutomationApplicators` keys verbatim, so a stale key is an offered target

We already have a partial stale detector, but it only covers APVTS:
`StandaloneEditor.cpp:3032-3036` — `onIsParamStale` returns `apvts.getParameter(pid) == nullptr`.
A registry-only paramId is therefore never reported stale, by construction.

Three widget-dies-without-tab-close paths, verified in source:

1. FX rack panel rebuild on effect swap / reorder — `EffectsPage.cpp:822-863`
2. FX rack channel switch — `EffectsPage.cpp:24` then `:494` `setRack(rack)`
3. Pedal slot rebuild — `BaySickPedalsEditor.cpp:224-240`

Plus `HarmlessEditor::rebindToPart`, which is why `registerParameterAutomation` exists at all.

We did get one thing right that a major DAW gets wrong: our rack + pedal automation keys are slot
UUIDs, not slot indices. See REAPER below.

### A finding that changes the decision

Case 2 above is not hygiene, it is a functional hole. When the user switches the Effects page
channel dropdown from Layer 0 to Layer 1, Layer 0's slot editors are destroyed, so every
`mixer_layer_0_<uuid>_*` applicator becomes a live-but-inert entry. An automation lane on a
Layer 0 rack knob then silently stops applying, and starts working again only if the user
navigates the Effects page back to Layer 0.

**Neither (b) nor (c) fixes it** — (b) removes the entry, (c) marks it dead, and in both cases the
lane still does nothing. Only moving the applicator off the widget and onto the DSP fixes it.

## State of the art — summary table

| System | Automation target | Keyed by | UI relationship | Unregistration | Missing-target behavior |
|---|---|---|---|---|---|
| VST3 | `IEditController` parameter | 32-bit ParamID | reports edits via `IComponentHandler` | n/a | set cannot change at runtime |
| CLAP | plugin parameter | stable id, never changes | same | n/a | explicit `clear(CLAP_PARAM_CLEAR_ALL)` |
| Tracktion | `AutomatableParameter` (ref-counted) | const `paramID` + `EditItemID` | `ListenerList` listener | explicit `removeListener`; refcount | data persists in ValueTree |
| Ardour | `AutomationControl` (`shared_ptr`) | `Evoral::Parameter` typed key | GUI observes control | shared_ptr + `clear_controls()` | lazy `control_factory` on `create=true` |
| Vital | `ValueBridge : AudioProcessorParameter` | parameter index/name | UI reads the bridge | n/a | n/a |
| Surge XT | `Parameter` in fixed patch arrays | `destination_id` int | UI reads storage | n/a | none — no validation at all |
| iPlug2 | `IParam` owned by delegate | `paramIdx` int | `SetParamIdx`, no registry | n/a | `kNoParameter` = unbound |
| JUCE APVTS | `RangedAudioParameter` | String parameterID | RAII attachment | dtor unregisters | `jassertfalse`, null attachment |
| REAPER | envelope on FX param | fxindex + parameterindex | n/a | n/a | positional, inherently fragile |
| **BaySickDAW today** | **UI widget closure** | **String paramId** | **the UI IS the target** | **hand-written prefix list** | **silent no-op** |

**Across eight codebases and three DAW vendor docs, no system maintains a UI-keyed automation
applicator map.** In every case the automation target is a model- or processor-owned object with a
stable id, and the UI is an observer that can come and go freely.

### Key per-system findings

- **VST3** — spec forbids runtime churn of the automatable set outright. HIGH confidence.
- **CLAP** — allows dynamic parameter sets, but requires `clap_host_params->clear()` with
  `CLAP_PARAM_CLEAR_ALL` when a target dies. Closest analogue to our situation; the lesson is
  "when a target dies, say so out loud". HIGH.
- **Tracktion Engine** — `AutomatableParameter` is a ref-counted model object; UI attaches via
  `ListenerList` with explicit add/remove. `MacroParameter` is the precedent for synthesising a
  real parameter for a non-parameter target. No map of UI closures anywhere. HIGH.
- **Ardour** — automation keyed by `Evoral::Parameter` typed key; controls lazily created on demand
  via `automation_control(param, create)`. Lazy-resolve-by-key is the pattern most applicable to us. HIGH.
- **Vital** — `ValueBridge` derives from `juce::AudioProcessorParameter` and wraps an internal
  engine value. The canonical "my thing is not a parameter" answer. Directly relevant to
  BaySickPedals knobs. HIGH.
- **Surge XT** — fixed generic `Parameter p[n_fx_params]` bank per FX slot, reinterpreted when the
  effect type changes. Exactly our hot-swappable-rack problem, solved by fixed storage + dynamic
  semantics. HIGH.
- **iPlug2** — no registry at all; `SetParamIdx` indexes a delegate-owned array. Control
  destruction is a non-event. HIGH.
- **JUCE** — `ParameterAttachment` registers in ctor / unregisters in dtor; `ListenerList::addScoped()`
  returns an `ErasedScopeGuard` RAII token. JUCE converged on RAII over time. Unknown parameterID
  hits `jassertfalse` and yields a null attachment. HIGH.
- **JUCE `ComponentListener::componentBeingDeleted`** — fires from the base `Component` destructor;
  only base `Component&` identity is usable, which is all a pointer-keyed erase needs. HIGH.
- **Ableton** — Rack macros are real device parameters with a separate mapping layer. HIGH for that;
  silent on target deletion.
- **Bitwig** — device-owned modulation routings, UI is an inspector. MEDIUM (search snippets only).
- **REAPER — the counterexample** — FX envelopes addressed positionally by `fxindex` +
  `parameterindex`, no stable GUID. Our UUID slot keys are ahead of this; worth not regressing. HIGH.

### Lifetime mechanisms in the wild

Three, in increasing strength: (1) no registry at all — the target outlives every view by
construction (iPlug2, Vital); (2) refcount/shared_ptr on the target so it cannot dangle
(Tracktion, Ardour) with explicit `removeListener` for observers; (3) RAII token, which is what
JUCE itself converged on. **Nobody uses a weak-pointer sweep, a generation counter, or a
hand-maintained key-prefix list. Our prefix list has no analogue anywhere.**

## Recommendation — hybrid, in this order

**Step 0 — confirm the functional hole (2 minutes).** Automate an FX rack knob on Layer 0, switch
the Effects page channel dropdown to Layer 1, play. If Layer 0's automation stops applying, step 3
is mandatory.

**Step 1 — adopt (b) as the mechanism, implemented as a self-cleaning registry ("b-prime"), not as
44 handle members.** Use `ComponentListener::componentBeingDeleted` plus a `Component* -> {paramIds}`
reverse index. On deletion, erase that component's paramIds from both maps. Gets (b)'s
correct-by-construction property — engine swap, `rebindToPart`, slot hot-swap, sub-editor rebuild
all clean up automatically — **without changing any of the 29 helper call sites**, because the five
wrappers already hold the widget reference. `eraseAutomationEntriesWithPrefix` and all ~17 literals
then delete outright.

Two hazards: `registerParameterAutomation` already takes a `lifetimeGuard` Component — use it as the
listener target, making the existing contract enforced rather than advisory. And teardown order —
set an `mTearingDown` flag in `~StandaloneEditor` so the callback no-ops once the maps are going away.

**Step 2 — take the diagnostic half of (c), following JUCE's `jassertfalse` precedent.** Change
dispatch to distinguish resolved from unresolved; `jassertfalse` in Debug once per paramId per
session; extend `onIsParamStale` from "not in APVTS" to "not in APVTS AND not in the registry" so
the Event Editor browser's dim/red row covers registry lanes. **Keep the lane inert, do not delete
it** — matches Ardour and Tracktion, where automation data outlives the live control object.

**Step 3 — targeted (e)-lite for the two families that genuinely break: FX rack panels and
BaySickPedals.** Move the applicator's target from the `juce::Slider` to the DSP/slot object,
resolved lazily at apply time by `(channelPrefix, slotUuid, knobSuffix)` — the key we already stamp.
The registry entry then captures the processor/rack (which outlives every panel), not the widget.
This is the Vital `ValueBridge` / iPlug2 delegate shape. Fixes the channel-switch hole, the
pedal-rebuild hole, and makes step 1's cleanup a no-op for those families.

Cost: each DSP type needs a `setParamByKey(suffix, value01)` or a per-type suffix table, because
today the value only reaches the DSP via the slider's `onValueChange`.

**Do not do full (e) now.** Promoting every registry target to APVTS before release means a
save-format change, a parameter-count explosion across 16 drum tabs / 50 audio inserts / 8 pedal
slots, and touching BaySickPedals during the last coding group. The right post-v1 target is Surge's
`FxStorage` model — a fixed generic parameter bank per slot, reinterpreted per effect type.

### Where the agent is extrapolating

- The FX-rack claim was read from source, not observed. (Parent session has since verified the
  code chain; ear confirmation still outstanding — see step 0.)
- "Nobody keeps a UI-keyed applicator map" is an argument from absence across eight codebases.
  Strong, but no vendor writes down "do not do this", so there is no positive citation.
- No vendor surfaces a user-visible warning for a dead automation target. Step 2's UI treatment is
  a design call extended from our own `onIsParamStale`, not a copied pattern.
- Tempo automation modelling was not verified anywhere, so `global_tempo` is out of scope for the
  cited evidence. It is singleton and editor-lifetime-scoped, and is fine under any option.

## Open questions

- Step 0's result. Step 3's scope hangs on it.
- Are there registrants that are NOT `juce::Component`s? `ChickenHeadSelector` and
  `DualLabelToggle` need checking.
- Does any registration happen from a floating window (EventEditor, CallOutBox) not owned by
  `StandaloneEditor`? If so, teardown ordering needs more than a single flag.
- Ardour's actual processor-removal path (`Route::remove_processor` -> `AutomationList`) — not
  stated in the headers read.

## Sources

WebFetched (HIGH confidence): VST3 dev portal Parameters+Automation; CLAP `ext/params.h`;
Tracktion `tracktion_AutomatableParameter.h` / `tracktion_MacroParameter.h` /
`tracktion_AutomatableEditItem.h`; Ardour `automatable.h` / `automatable.cc`; Vital
`value_bridge.h`; Surge `SurgeStorage.h` / `ModulationSource.h`; iPlug2 `IControl.h`; JUCE
`juce_ParameterAttachments.h` / `juce_AudioProcessorValueTreeState.h` / `.cpp` + docs for
`ParameterAttachment`, `ListenerList`, `ComponentListener`; Ableton Rack manual; REAPER ReaScript
reference; Ardour automation-lanes manual.

WebSearch snippets only (MEDIUM): Bitwig modulators (bitwig.com, polarity.me); REAPER tutorials
(reapertips, whyreaper); Ableton help centre.

Deliberately skipped: Dexed and Helm — plain JUCE parameter-array plugins with no dynamic target
problem; citations without information.
