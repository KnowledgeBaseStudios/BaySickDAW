# MIDI Learn

**Purpose** - MIDI Learn ties a knob, fader or switch on screen to a physical
control on your MIDI keyboard or controller, so you can reach for the hardware
instead of the mouse. A separate but related system, the drum trigger map, ties a
pad or key to one drum in the kit so you can play the drums live.

## How it operates

Two independent stores, deliberately not built on each other, because they answer
different questions. The parameter registry maps hardware events to *parameter
values*; the drum map maps hardware events to *note triggers* injected into a
drum tab's MIDI stream.

### The parameter registry

`Source/MidiLearn/MidiLearnRegistry.h/.cpp`. One mapping per parameter id -
re-learning replaces rather than stacking. A mapping records:

| Field | Meaning |
|---|---|
| `paramId` | The parameter it drives. |
| `msgType` | Control Change, Pitch Bend, or Channel Aftertouch. |
| `ccNumber` | 0-127, for CC mappings. |
| `channel` | 0 means Omni (any channel); 1-16 pins one. |
| `deviceName` | Empty means any device; otherwise this port only. |
| `formula` | Reserved. Version 1 is linear only - CC 0..127 maps straight across the parameter's range. |

Reading and writing the map is guarded by a spin lock. The audio thread's dispatch
*try*-locks and skips the event if it cannot get in: one missed frame of a knob
someone is turning by hand is not noticeable, and blocking the audio thread would
be.

MIDI arrives on a per-device thread, not the audio thread, so
`MidiLearnEventQueue` bridges the two. It holds up to 1024 events (oldest dropped
past that) and interns device names to small integers, because a reference-counted
string destroyed during the audio thread's drain would mean freeing heap inside the
render callback.

`Source/MidiLearn/MidiLearnUI.h` is the front end: it owns learn mode's lifecycle,
the 30-second timeout, the Escape key, and the dashed-yellow outline that marks
which control is listening. The capture itself is a lock-free handshake - the
audio thread fills a pre-built mapping and sets a flag, and a 20 Hz message-thread
poll commits it, so the map insert and the UI notification never run on the audio
thread.

### The drum trigger map

`Source/MidiLearn/DrumTriggerMap.h/.cpp`. One binding per drum tab (up to 32),
each a MIDI note **or** a CC, with an optional channel. Device is deliberately not
recorded - the trigger path runs in the live-MIDI loop, which preserves the event's
position within the audio block but discards which port it came from. Channel
scoping covers the collision that actually happens, which is a pad's note range
overlapping a keyboard's.

The audio thread reads only a packed 32-bit word per drum, behind a single "is
anything bound at all?" atomic so the per-block scan is skipped entirely until you
bind something. No locks, no allocation, no strings on that path.

## User-facing behavior

### Binding a control

**Right-click any knob, fader or switch.** Below the automation items you get:

| Item | When it appears | What it does |
|---|---|---|
| MIDI Learn | Always (grayed as "MIDI Learn (no MIDI input devices)" when no MIDI input is connected) | Arms learn mode on this control. |
| MIDI Forget: `<summary>` | Only when this control already has a mapping | Removes it. The summary reads like "USB Keyboard CC#74 ch1", "Pitch Wheel omni" or "Aftertouch ch2". |
| Save MIDI mappings as global default | Only when at least one mapping exists | Writes every current mapping to `Documents\BaySickDAW\MidiMappings.xml`, so new projects start with them. |

While a control is armed it wears a **dashed yellow outline**. Move the hardware
control you want and the binding is made immediately; the outline clears. If you
change your mind, press **Escape**, or just wait - learn mode cancels itself after
**30 seconds**. Arming a second control cancels the first.

MIDI device hot-plug is not supported: plug your controller in before opening the
menu. If no MIDI input is present when you open it, the Learn item is grayed with
the reason in its own label.

Which MIDI inputs are enabled is set in **Options > Audio Settings...**. The main
Options menu carries a read-only reminder row, "MIDI is Omni (all devices)".

### Playing the drums from a pad controller

Each drum tab's **Menu** carries, under **MIDI Note** (which sets the note the drum
sounds at):

| Item | Behavior |
|---|---|
| MIDI Learn / MIDI Learn: `<binding>` | Arms this drum. The label doubles as the readout of its current binding, e.g. "MIDI Learn: CC 42 ch1" or "MIDI Learn: D5 omni". |
| MIDI Forget: `<binding>` | Only shown when a binding exists. Clears it. |

Picking Learn raises **MIDI Learn - "Hit a pad or key to assign it to this drum.
Waiting 30 seconds..."** with a Cancel button. Hit the pad and the binding is made
and the box closes.

If what you hit was a **note** (not a CC) and it differs from the drum's current
play note, you are then asked whether to set the drum's play note to that note as
well. Answering no leaves the trigger bound while the drum keeps sounding at its
own pitch.

**Trigger Velocity** lives in **Options > Audio Settings...**, beside the MIDI
input list:

| Option | Effect |
|---|---|
| From controller (default) | How hard you hit the pad sets how loud the drum is. |
| Fixed | Every hit lands at the same level - for pads that are not velocity sensitive and would otherwise send one arbitrary constant. |

The change applies immediately, not at the next launch.

## Parameters and persistence

| Store | Where it is saved | Load behavior |
|---|---|---|
| Parameter mappings | `<MidiCCMappings>` in `project.xml`, **and** `Documents\BaySickDAW\MidiMappings.xml` as global defaults | The global file is loaded once at app start. A project's own mappings then **overlay** those globals rather than replacing them, so a project with no mappings of its own keeps whatever you set as your defaults. |
| Drum trigger bindings | `<DrumTriggers>` in `project.xml` | There are no global defaults behind these, so a project with no `<DrumTriggers>` element **clears** them. Without that, project B would inherit project A's bindings against project B's drum tabs - the pad would fire, and recorded MIDI would land on the wrong drum. |
| Trigger velocity source | `<MidiTriggerVelocity fixed>` in `settings.xml` | Per machine. Loaded before the audio device opens so the first block sees the right value. |

Both are written into the project by `serializeProject` and deliberately **not**
into the shared block a template save reuses - a template must not carry one
machine's controller bindings.

Message type is stored as a **string** rather than a number, so adding a new
message type later cannot renumber existing files. The drum binding's kind
(none / note / CC) is stored as an explicitly pinned number. Drum bindings are
written sparsely - one `<Trigger drum kind number channel>` per bound drum - and
all 32 slots are cleared before a load reads them.

`File > New` clears the drum trigger bindings along with the rest of the project
state.

## Lifetime and teardown

- Both stores live on the processor and outlive every page and window.
- Drum learn arms the **shared map**, and every open drum tab polls it on its own
  timer. The ownership check is inside the take (`takeCapturedBindingFor(drumIdx,
  ...)`) on purpose: an unconditional take would let whichever tab polls first
  swallow a capture meant for another drum, so with N drum tabs open the learn
  would succeed about one time in N.
- The drum learn dialog holds a reference to the map, **not** to its page. If you
  close the drum tab while the learn box is open, the page dies first; disarming
  through a page pointer would never run, and the map would stay armed forever,
  swallowing the next note played anywhere.
- Disarming only happens if the map is still armed **for that drum** - another tab
  may have started its own learn since, and stomping that is the same bug in the
  other direction.
- Learn-mode UI teardown (the outline, the timer) runs unconditionally on cancel,
  not gated on "am I still learning". Escape pressed in the window between the
  audio thread capturing and the poll committing would otherwise strand the
  outline on screen.
- The event queue's device-name table is a fixed 32 slots that is never resized
  and whose entries are never destroyed. That is what makes it safe for the audio
  thread to hold a reference into it while a MIDI thread appends.
- The queue's drain releases its lock before running the callbacks, because those
  callbacks reach parameter notification and holding across them would make the
  MIDI input thread spin-wait for the whole notification.

## Cross-references

- *Automation* - the other half of the same right-click menu, and the other way
  to drive a control without touching it.
- *Projects and Saving* - the `<MidiCCMappings>` and `<DrumTriggers>` elements.
- *Templates* - why controller bindings are excluded from templates.

## Differs from Carry-Forward

Nothing to record. The frozen Carry-Forward snapshot predates MIDI Learn.
