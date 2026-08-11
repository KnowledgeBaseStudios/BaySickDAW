# Workspace and Windows

**Purpose** - BaySickDAW does not swap one full-screen page for another. The app
frame is fixed and always fills the screen; inside it is the **workspace**, a
dark region where every page - Mixer, Builder, an instrument, an effect panel -
lives in its own small window that you can move, resize and close. Several
windows are open and alive at once, and where you put them is remembered.

---

## How it operates

Two classes in `Source/Standalone/WorkspaceWindow.cpp/.h`:

* **`Workspace`** - an ordinary child component of the editor that fills the area
  under the transport bar and the ribbon tabs. It draws the empty backdrop and,
  more importantly, it is the **anchor**: it supplies the native window handle
  that contained windows parent to, and the origin their coordinates are measured
  from.
* **`WorkspaceWindow`** - one contained window. It draws its own frame and title
  strip, owns a close button, a fill toggle, a resize border and a `PageMenuBar`,
  and hosts exactly one page or panel.

**Why they are real OS windows.** A contained window is created with
`addToDesktop(0, parentHwnd)`, which makes it a genuine `WS_CHILD` window rather
than a drawn rectangle. This is not decoration: a hosted VST3 plugin editor is a
foreign OS window, and a foreign OS window always paints on top of anything the
app draws into its own client area. Making our windows native children too puts
them in the same z-order space as plugin surfaces, which is the only arrangement
where "bring this window to the front" can work for both.

**The coordinate contract** (verified against the vendored JUCE and written down
in `WorkspaceWindow.h`): a child peer both *sets* and *reads back* its position
in the **parent window's client space**, not screen space. So a window's stored
position is workspace-local, and `attachTo` adds `Workspace::originInParentClient()`
on the way in and `saveBounds` subtracts it on the way out. Getting this wrong is
what once made windows creep down and right on every open/close cycle.

**The z-order trap.** Because a native child peer always renders above anything
drawn into its parent, `setAlwaysOnTop` cannot cross that boundary. Any overlay
meant to cover the workspace has to be promoted to its own desktop window - which
is why the app's single tooltip window is parentless.

**Peer-keyed work.** Repeating UI work (page poll timers, monitor-refresh meter
drains) is started and stopped in `parentHierarchyChanged`, keyed on whether the
component has a live peer - never in a constructor, because a page can exist
without a window.

**Placement lives in three stores** (the "three-lifetime model"):

1. **The in-memory map** (`sessionBounds`) is the one live store. *Every* window
   writes it on move, resize and close, and reads it on open - so closing and
   reopening a window always returns it to the same spot.
2. **`settings.xml`** is written once, at app exit, from a filtered view of that
   map. Only the four default tabs (Builder, Mixer, Effects, Piano Roll) go in
   it, with size *and* position. Nothing else.
3. **The project file** carries the full map plus which windows were open, and
   *replaces* the in-memory map on load - except the four default tabs, whose
   placement is a global preference and is deliberately carried across.

Programmatic repositioning (the clamp-into-view sweep that runs whenever the
workspace changes size) is wrapped in `ScopedSaveSuppress` so it can never
overwrite a placement the user chose.

---

## User-facing behavior

### The frame

The application window is fixed at full screen and cannot be resized or
maximized - there is only one valid frame geometry. Top to bottom it holds the
menu bar, the transport row (transport controls, pattern button, position
readout, ribbon tabs, performance readout), and then the workspace filling
everything below.
### The menu bar and the Help menu

Across the top of the frame sits the application menu bar - **File**, **Edit**,
**Patterns**, **View**, **Options**, **Help**. It is the only menu bar the frame
itself owns; every window inside the workspace carries its own menu in its title
strip instead.

**Help** holds four entries, with a separator before the last one:

| Entry | What it does |
|---|---|
| **Help Index  (F1)** | Opens the manuals window, described below. The label carries two spaces before the key hint. |
| **Key Binds...** | Opens the **Key Binds** window: every rebindable command with its current shortcut, plus every page-local key and mouse gesture as a reference row. See `Keyboard Shortcuts.md`. |
| **Rusty Drums Map...** | Opens the **Rusty Drums Map** window, a Key / MIDI / Sound / Articulation table of every note the Rusty kit answers to. It does not need a Rusty Drums tab open: with no kit loaded it parses the installed Big Rusty Drums kit instead, and if the Core Library is not installed either, the table comes up empty. See `BaySickRustyDrums.md`. |
| **About BaySickDAW v1.0** | An information box titled "BaySickDAW v1.0" carrying the build line, a short "Powered by:" attribution list naming sfizz and LAME, and a single **OK** button. That list is knowingly incomplete. |

### The manuals window

**Help > Help Index**, or **F1** (its default key) from the main frame or any
page window, opens **BaySickDAW Manuals**. It is a desktop window rather than a
contained one: 1100 x 800 when it first opens, centered, freely resizable, with
a close button and no minimize or maximize, wearing the same 26-pixel title
strip as the app's other desktop windows. Like them it is *owned* by the main
window, so it floats above the app and minimizes with it but never above other
applications. Only one can exist - asking for it a second time brings the open
window to the front rather than opening another.

Inside it is an embedded browser. The manuals are a static web site the app
expects to find staged beside the application executable at `Manuals\`, entry
point `index.html`. Nothing in the app authors, validates, downloads or installs
that content; the window only reads it.

**When the manuals are not installed** the window does not open blank. It says
so, in plain text over the app background:

> The manuals are not installed.
>
> They belong at:
> *(the full path it looked for, spelled out)*
>
> A full install places them there. If you are running a development build, the
> manuals are built separately and staged into that folder.

Below that message is one button, **Open Manuals Folder**. It creates the folder
if it is not there and opens it in Explorer, so the manuals can be dropped in
without hunting for the path.


### Opening and closing windows

* **Clicking a ribbon tab** brings that page's window to the front. If the window
  was closed, clicking the tab opens it again in the same place it was.
* **Closing a window** (the `x` at its top right) closes the *window*, not the
  work. The engine keeps running, the audio keeps playing, and every automation
  lane stays live - the tooltip on the button says exactly this. Click the tab
  again to bring the window back.
* At launch, four windows open automatically: **Builder, Mixer, Effects and Piano
  Roll**. Instrument and player windows open the first time you select their tab.
* Opening a project reopens exactly the windows that project had open when it was
  saved, at the sizes and positions it saved.
* A brand-new window that has never been placed opens near the top left, stepped
  down and right by 28 pixels for each already-open window (up to six steps) so
  it never lands exactly on top of the last one.

### The title strip

Every contained window has the same 26-pixel strip across the top. From the left
it carries the window's **Menu** (and, on some windows, further headings such as
`Add`, `Edit`, `View`), any tab-slot buttons the page mounts, the page or engine
name, and then on the right:

| Button | What it does |
|---|---|
| **Fill / restore** (square outline) | Fills the whole workspace. Click again to go back to the size and place it had before. Dragging or resizing the window by hand also cancels the filled state, so the next click fills again rather than restoring something you have abandoned. |
| **x** (close) | Closes the window. |

The menu row **is** the title strip - there is no separate menu bar under the
transport. Each window carries its own menu, because several windows are visible
at once and one shared bar could only ever show one of them.

### Moving windows

Drag the title strip. Two behaviors apply while you drag:

* **Magnetism.** When an edge of the window you are dragging comes within about
  10 pixels of another window's edge, or of a workspace edge, it is nudged flush
  so the two line up. It is deliberately soft: keep dragging and you push
  straight through it, and windows may still overlap freely.
* **The mouse pointer is the boundary, not the window.** A window may hang off
  any edge of the workspace; what cannot leave the workspace is your cursor. This
  guarantees a window can never be dragged somewhere you cannot grab it back
  from, without costing you a size you deliberately chose. The pointer stays
  stuck to the title bar at the edge rather than sliding off it.

### Resizing windows

Drag any edge or corner. The same magnetism applies, but per edge - dragging the
right edge does not drag the left one along. A resize is trimmed at the workspace
boundary: the edge you are dragging simply stops rather than the whole window
sliding back in.

**Every window has a minimum size, and that minimum is also its default opening
size.** These are the measured "smallest still readable" figures for each page,
so a window that refuses to shrink further is behaving correctly. The one
exception is a window hosting a third-party VST3 plugin. Nothing scales a
plugin's own interface, so once the plugin has reported its size those windows
drop to the bare 120 x 80 grab minimum and are free to be smaller than the
plugin they contain. A window smaller than its plugin clips it - there are no
scrollbars, and the plugin's own magnify control is what makes it fit.

### Effect windows and their visual windows (the tether)

Opening an effect panel that has a visual display automatically opens the visual
underneath it, centered and matched to the effect window's width. The pair is
**locked** by default and behaves as one object:

* Dragging **either half** moves both.
* Bringing either one forward brings the other with it, with the half you clicked
  ending on top.
* Closing either one closes both.
* Resizing the effect window (for example switching a panel between Basic and
  Advanced) re-seats the visual underneath at the new width.

The visual window's **Menu** carries one entry: **Unlock from Effect Window** (or
**Lock to Effect Window** when it is already unlocked). Unlocked, the two windows
move, front and close independently. Locking a pair you have dragged apart snaps
the visual back under the effect immediately.

Closing an *unlocked* visual by hand is treated as a dismissal - it will not
auto-open again for that effect until you ask for it from the effect window's
**Menu > Visual**. Closing a locked pair is not a dismissal, because the effect
window went with it; both come back together.

Lock state, dismissal state and both windows' positions are saved with the
project.

### Which windows exist

| Kind | Persist key shape | Opened by |
|---|---|---|
| Page window (Builder, Mixer, Effects, Piano Roll, Layers, Bass, Drums, Clips, Vox, Inst, Plugins) | `<typeNumber>:<pageIndex>` | ribbon tab |
| Effect panel | `fx:<channelId>:<slotUuid>` | Effects page |
| Effect visual | `vis:<channelId>:<slotUuid>` | automatically with its effect, or Menu > Visual |
| EQ (pre or post) | `eq:<channelId>:pre` / `:post` | Effects page |
| Vox satellite (Chain, Pitch, Align, NAM/IR) | `voxsat:<index>:<kind>` | the Vox page |
| Inst satellite (Pedals, NAM/IR) | `instsat:<index>:pedals` / `:namir` | the Inst page |
| Master analyzer | `analyzer:master` | the Master strip's **Analyzer** button |

A live-input Inst tab has no page window at all - its **Pedals** window *is* its
player, so clicking that tab opens the pedals window.

Effect panel and effect visual windows are keyed by the effect's UUID, not its
slot number, so reordering a rack moves the window with the effect rather than
repointing it at whatever landed in that slot. EQ windows are keyed by channel
and by which EQ (pre or post) they show, because a channel has exactly one of
each.

---

## Parameters and persistence

No APVTS parameters. Window state is plain geometry plus a small amount of
open/closed bookkeeping.

**In `settings.xml`** (per machine, under `Documents\BaySickDAW\`), inside a
`<WorkspaceWindows>` element, one `<W>` per **default tab only**:

| Attribute | Meaning |
|---|---|
| `key` | the persist key |
| `w`, `h` | size |
| `x`, `y` | position (written only for the four placement-persistent keys) |
| `rx`, `ry`, `rw`, `rh` | the pre-fill restore rectangle, when the window was filled at exit |

This file is written **once, at application exit**, and read only as a seed when
the in-memory map has nothing.

**In the project file**, under `<UIState><Windows>`:

* one `<W key x y w h>` per remembered window, **excluding** the four default
  tabs (whose placement is global) and excluding records for effects that no
  longer exist;
* one `<Open key>` per window that was open when the project was saved. A pedals
  window additionally records `view="1"` when it was in Compact view, and a
  visual window records `lock="0"` when its tether was unlocked - a missing
  `lock` attribute means locked, which is the default;
* `<VisClosed key>` entries for visual windows the user dismissed by hand.

**Not saved:** which window currently has focus, the z-order of the stack, and
any transient drag or clamp position.

A project saved before this system existed has no `<Windows>` element; it opens
with no page windows framed, and its tabs are one ribbon click away.

---

## Lifetime and teardown

`Workspace` is created with `StandaloneEditor` and lives for the session.

A `WorkspaceWindow` is created on demand and **destroyed on close** - the object
is short-lived, and its persist key is the only thing that carries its position
forward. Because pages are built in the editor's constructor, before the frame
has an OS handle, `attachTo` **queues** any window that asks to attach too early;
`Workspace::resized` drains that queue once the frame is laid out and both the
handle and the workspace bounds are real. A window attached against a zero-size
workspace would land at the wrong origin and at a collapsed default size - the
same cause behind both symptoms.

Ordering that matters:

* **The destructor does not save bounds.** It used to, and because it ran last it
  overwrote the good value with whatever a half-dismantled window reported. Every
  real route saves explicitly: drag release, border resize, move, the close
  button, the fill toggle, and an exit flush that runs before teardown.
* **Content ownership is two-mode.** `setContent` takes ownership and the page
  dies with the window; `setContentNonOwned` does not, and the page outlives it.
  Editor pages use the non-owning mode.
* **Closing a page window destroys the window only, not the page.** The editor
  caches raw pointers into several pages (`mMixerPage` and friends, dereferenced
  in over a hundred unguarded places), so destroying the page would dangle them.
  The window and its heavy child components go; the engine is owned by the engine
  rig and is untouched either way.
* **The tether links are `SafePointer`s** in both directions. Either half can be
  destroyed first, and a dead partner simply ends the tether instead of leaving a
  dangling pointer in a drag path.
* **The workspace's window list is also `SafePointer`s**, so a teardown-ordering
  mistake degrades to a skipped entry rather than a crash.
* **A growing workspace restores windows to their saved bounds.** At launch the
  frame passes through smaller sizes on the way to full; windows are clamped
  inward against those partial sizes, and the grow pass puts them back. That only
  works because the clamp is save-suppressed.
* Each window installs its own key listeners (the shortcut mapping set first, the
  typing-keyboard gate last) because a contained window is its own desktop
  component and a key press inside it would otherwise never reach the editor.
  Each also installs its own right-click "Automate" listener, for the same
  peer-boundary reason.

---

## Cross-references

* **Mixer.md**, **Transport and Playback.md** - the two surfaces that sit in and
  above the workspace respectively.
* **Keyboard Shortcuts.md** - F1 opens the manuals window; F5 through F12 select
  and front page windows.
* **Effect Racks.md** - the Effects page, the rack and the per-effect visuals;
  the tether described here is the geometry half of that relationship.
* The ribbon tab bar (`Source/Standalone/RibbonTabBar.h`) owns tab creation,
  renaming, deletion and the "+" engine picker; this document covers only what
  happens to the *window* when a tab is selected.

---

## Differs from Carry-Forward

Carry-Forward (2026-05-07) predates the contained-window shell entirely. At that
snapshot every page was a full-size child of the editor, shown and hidden by tab
selection, with one shared page-menu bar under the transport and a resizable main
frame whose bounds were saved and restored. All of that is gone:

* pages live in individual native child windows inside a **fixed fullscreen**
  frame that no longer saves its own geometry;
* the shared page-menu bar was dissolved into a per-window title strip;
* tab selection fronts a window rather than hiding its siblings, and several
  pages are visible and alive at once;
* the monitor-reachability lesson from the old frame restore did not retire - it
  moved to `Workspace::clampWindowsIntoView`, which keeps every contained window
  grabbable for the same reason.
