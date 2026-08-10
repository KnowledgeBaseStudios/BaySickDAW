# Plugins Page

**Purpose** - A Plugins tab hosts one third-party VST3 **instrument** inside BaySickDAW. The plugin brings its own interface; the tab's job is to pick it, give it a mixer channel, a piano roll and an effects rack, and then get out of the way. This is the thinnest page in the app on purpose - every other page type owns knobs because it drives one of our engines, and a hosted plugin owns its own.

Only VST3 is supported, deliberately. VST2 hosting needs SDK headers that cannot be licensed, so a VST2 file found during a scan is listed as skipped with a reason rather than silently dropped.

---

## How it operates

**The instance lives in the model.** `Source/Standalone/PluginsPage.h/.cpp` is a view. The hosted instance is a `Hosting::HostedPluginInstance` (`Source/Hosting/HostedPlugin.h/.cpp`) owned by `EngineRig` under `TabKind::Plugins`. `HostedPluginInstance` is itself a `juce::AudioProcessor`, so the rig's generic engine slot needed no new plumbing - a hosted instrument is "just another engine". Closing the window leaves the plugin playing.

**Which plugin is the tab's engineType.** The rig stores the plugin's `PluginDescription::createIdentifierString()` as the tab's engine-type string, so "which plugin" needs no separate persistence and rides the existing tab serialization.

**Description resolution has two sources, in order.** `EngineRig::createEngineFor` asks `PluginManager::findAdded(identifier)` first; if that misses it falls back to a description **stashed with the saved tab** by the restore walker. The stash is single-use and erased whether or not it was needed. That fallback is why a project keeps loading its plugins after you remove them from your added list. `PluginsPage::selectPluginById` deliberately does **not** filter on the added list - filtering there made the fallback unreachable.

**Identity and routing.** Cap is `kMaxPluginPages = 20`.

| Thing | Value |
|---|---|
| Mixer channel id | `MixerChannelIds::pluginInsert(i)` = 900 + i |
| Strip APVTS prefix | `mixer_plugin_<i>` |
| Default output | Plugins Bus (`kPluginsBus`, id 13), and unlike Rusty strips it is **not** main-out locked - a plugin strip can be moved under the Layers or Bass bus |
| Page accent | `VC::Purple` |
| Piano roll | `EngineKind::Plugin`, index = page index |
| APVTS prefix | none - the plugin's parameters belong to the plugin |

**MIDI reaches it through the live-MIDI route.** A hosted plugin has no `auditionNote`; selecting its roll in the Piano Roll page makes it the live MIDI target (target kind 10). Do not add a second MIDI path for it.

**The bridge (out-of-process hosting).** `Hosting::SandboxedPluginClient` launches one architecture-matched helper process per bridged plugin - `BaySickPluginHost64.exe` or `BaySickPluginHost32.exe`. A 64-bit process physically cannot load a 32-bit DLL, so **32-bit plugins are always bridged** (`isBridgeForced()`); 64-bit plugins run in-process by default. The audio thread never blocks on the helper: `processBlock` writes the block into shared memory, signals an event and waits with a hard deadline; a helper that misses it yields silence for that block. A bridged plugin's editor lives in the helper, so the host supplies a native child window for the helper to reparent the plugin's UI into.

The **"Run bridged (separate process)" toggle exists only on the FX-rack slot window**, not on a Plugins tab. That is an accident of which window grew which menu, and it means a 64-bit tab instance is pinned in-process and a 32-bit one is bridged from birth.

**Health states.** `HostedState` is `Ok`, `FailedToLoad`, `NeedsBridge` or `Crashed`, and they stay distinct on purpose. A crashed plugin keeps its window open showing a message instead of the window vanishing - merging "the user deleted this" with "this died" would make a crash look like a deletion.

**Threading.** Picking, restoring, retrying and state pushes are message thread. The revival path raises the project-load audio shield and settles a block before publishing the rebuilt engine, because pushing state into a live VST3 concurrently with its own `processBlock` is not something most plugins tolerate. IPC callbacks arrive on the connection's reader thread and are marshalled to the message thread before any user-facing callback fires.

---

## User-facing behavior

### Before you can add one: Options > Plugins

Plugins do not appear anywhere until you tell the app about them. **Options > Plugins** opens a window with three sections, top to bottom:

1. **Scan folders** - the folders that get searched, seeded with the standard VST3 install locations, plus a button that opens a folder picker to add more.
2. **Added** - the plugins you have chosen to use. **This is the list every other surface reads** - the "+" menu, the tab dropdown and the effects rack picker.
3. **Scan results** - blank until you press Scan. Afterwards it lists everything found that is not already added, each with a checkbox, and an Add button that moves the checked ones into section 2. It also lists what the scan **could not** use with the reason, so "my plugin isn't in the list" always has an answer ("Skipped: VST2 is not supported" being the common one).

### Making a Plugins tab

- **Ribbon "+" > VSTPlugin >** - a side menu of your added instruments, alphabetical. Pick one and you get a tab with that plugin already loaded.
- **Plugins ribbon slot > "+ Add VSTPlugin" >** - the same alphabetical side menu.

If you have not added anything yet the submenu shows a single disabled row: **"None added - see Options > Plugins"**.

New tabs are named `Plugin 1`, `Plugin 2`, counting upward without reusing deleted numbers - but the tab is renamed to the plugin's own name the moment it loads, so in practice you see the plugin's name.

The Plugins ribbon slot is purple, carries the usual tab-count badge and frozen dot, and its dropdown lists every plugin tab, then a **Pages:** section (**Player**, **Piano Roll**, **Pre EQ**, **Post EQ**), then **Rename...** and **Delete**, then the add submenu.

### The tab window

The window is a **"Select plugin..." button** and, underneath it, the plugin's own interface. Once a plugin is loaded the button turns into a plain, disabled label showing the plugin's name - **one plugin per tab, for the tab's life.** To use a different plugin, delete the tab and add another. (That is deliberate: swapping the engine under a loaded tab would destroy a live hosted instance under the audio thread for no benefit.)

The window **fits itself to the plugin's own surface** and follows the plugin if it resizes itself. Dragging the window's edge behaves in one of three ways:

- **Resizable plugin, in-process** - the resize is pushed through the plugin's own resize path and the plugin decides what it accepts.
- **Fixed-size plugin, in-process** - the plugin snaps any size change back, so the app scales the picture instead. Aspect is preserved and centered, so you get letterbox bars rather than a stretched or clipped interface. It refuses to shrink below half size rather than render something unreadable.
- **Bridged plugin** - cannot be scaled at all (its surface is another process's window). It is centered at its natural size and clipped to the frame.

If no plugin is loaded the page reads **"No plugin loaded"**.

The window's top strip carries the **Menu** button and a **Swing Mix** knob (0.00-1.00, default 1.00, right-click for **Truncate Swing Notes**) - the same per-player swing every note-driven tab has. Parameters: `swing_plugin_<n>_mix`, `swing_plugin_<n>_trunc`.

### The Menu (page actions)

| Item | What it does |
|---|---|
| **Piano Roll** | Jumps to the Piano Roll tab with this plugin selected. This is the tab's only navigation entry - there are no sub-pages to switch between. |
| **Save Page Preset As...** | Saves the plugin's settings plus the mixer strip, the effects rack and the EQs. |
| **Load Page Preset** | Loads one back. It instantiates the preset's plugin first, then applies state. If that plugin is not on your added list you get "This preset uses a plugin that is not in your added list. Add it under Options > Plugins, then load the preset again." |
| **Automate >** | The lane-creation surface for this plugin. A hosted plugin's editor is a foreign window with no right-click hook of ours, so this menu is the only way in. Top row is **"Last Touched: `<name>`"** - move any control inside the plugin and it names that control; before you have touched anything it reads "Last Touched (move a control in the plugin first)" and is grayed. Below it is every parameter the plugin reports, chunked into submenus of 30 ("1 - 30", "31 - 60", ...) so thousand-parameter synths stay navigable. Picking one opens the automation editor for it. |
| **Retry Loading Plugin** | Only appears while the plugin is not alive. |
| **FX Rack** | Jumps to this tab's effects rack. |
| **Freeze / Frozen** | Renders the tab to a file so the plugin stops costing CPU; its effects, EQ and fader stay live. |
| **Delete Plugin** | See below. |

A Plugins tab has **no Lock, no Rename and no Duplicate on this menu**. It cannot be locked at all, so Delete is never refused. Rename is available from the ribbon dropdown's **Rename...** row like any other tab.

**Deleting** warns: "Deleting this plugin tab removes its Mixer Strip, Effects Rack, and Piano Roll. The plugin itself stays installed and can be added again." If you have touched a control inside the plugin or changed its program since the tab settled, you get **Save Page Preset & Delete / Delete / Cancel**; otherwise **Delete / Cancel**. "Changed" is measured by a touch counter and the program name, not by comparing state blobs - many plugins keep volatile bytes (timestamps, window positions) in their state and would read dirty when untouched.

### Naming follows the plugin

The tab, the mixer strip and the piano-roll label all take the plugin's **current program name** when the plugin publishes one, and the plugin's name when it does not. Most modern synths run their own preset browsers that a host cannot see, so those simply stay on the plugin's name. Switching programs inside a plugin that does publish them renames the tab to follow. The very first name that arrives after a load or a project restore is absorbed quietly, so a tab name you typed yourself is never stomped by restore.

### "(missing)" on a Plugins tab

When the hosted instance is not alive - the file is gone, it refused to load, the bridge helper is missing, or the plugin crashed - the tab, strip and roll label gain a **" (missing)"** suffix. Two things about it are worth knowing:

- It is built on the **tab's own name**, not the plugin's, so it never replaces a name you chose.
- It is **display only.** `setTabName` strips it straight back off, so nothing the project saves ever carries it.

The same wording is used by the effects-rack slot for the same condition, on purpose.

**Recovery.** The app watches your added-plugin list; the moment it changes - which is what happens when you put the plugin back under Options > Plugins - every dead plugin tab retries automatically, once, on that edge. It is edge-triggered deliberately: a plugin that is gone for good must not be retried on a clock forever. **Retry Loading Plugin** on the Menu does the same thing by hand.

A successful retry **keeps the plugin's settings**: the outgoing instance hands back the bytes it was last restored with, and its full description travels with it, so a revival does not reset the plugin to defaults. It also **keeps the tab's freeze** - the same plugin restored to working is not a change in what the tab produces.

---

## Parameters and persistence

**There are no BaySickDAW parameters for the plugin's own controls.** Automation lanes address them by the plugin's stable parameter id, never by index - index-keyed lanes silently repoint themselves when a plugin update inserts or reorders a parameter. Lane ids are formed as `plugtab<n>_vst_<paramId>`.

**Channel parameters** live in the main APVTS under `mixer_plugin_<n>`: `_sendTo` (default Plugins Bus), `_send0..3_to`/`_amount`/`_prepost`, `_sc_recv0..3_from`, `_chokeGroup` (0-16), `_preeq_*` and the strip EQ banks.

**Saved with the project** - one `<Tab type="Plugins">` record carrying `pageIndex`, `name` (undecorated - the "(missing)" marker is stripped), `engine` (the plugin's identifier string), `engineData`, `locked` (always 0 - Plugins tabs cannot be locked), and freeze state when frozen. `engineData` is the `HostedPluginInstance` blob, which contains **the full `PluginDescription`**, the bridge preference and the plugin's own state. Storing the whole description rather than just the identifier is what makes a project keep loading its plugins after they leave your added list.

A save made while the plugin is **not** alive still writes your settings: the instance retains the last state blob it restored or read back, so a missing DLL does not silently erase that plugin's entire stored state from the project on the next save.

**Saved with a page preset** (`Documents/BaySickDAW/Presets/Plugin Page/My Presets/*.xml`): the plugin identifier as the engine type, the plugin's state, every `mixer_plugin_<n>` parameter, the insert rack and the EQs. The default filename offered is the plugin's name with any "(missing)" marker stripped, because that becomes a filename on disk.

**Per-machine, not per-project:** the scan folders, the added-plugins list and the per-plugin bridge preference all live in the plugin manager's own data file, not in a project.

**Not saved:** the "(missing)" marker, the dirty baseline behind the delete prompt, and the discovered parameter list (rebuilt on every load; bridged lists arrive asynchronously after load).

---

## Lifetime and teardown

`PluginsPage`'s constructor registers the tab with the rig immediately - the model owns the tab whether or not a plugin has been picked - and subscribes to the plugin manager's change broadcast. It subscribes from the **page** rather than a window because page destruction on window close is off, so the page outlives every window its tab opens and is torn down only with the tab.

The **mixer strip is created on the first plugin pick**, not at tab open, matching every other tab type.

A 4 Hz poll watches for the engine pointer changing, the instance dying or coming back, the discovered parameter count changing (bridged lists land late, and lanes can only be registered once the parameters exist) and the display name changing. That poll is peer-keyed: it stops when the window closes and restarts when one opens. Anything that has to work with **no** window open - the added-list retry edge - refreshes the editor and the name marker itself rather than waiting for a poll that is not running.

The page owns the editor (`Hosting::HostedPluginEditor`, a plain `Component`, never an `AudioProcessorEditor` of the hosted instance - a back-reference like that cannot safely outlive the instance). A rebuild always destroys the old editor before building the new one, because a hosted plugin has exactly one editor instance. When the instance is destroyed underneath it, `~HostedPluginInstance` tells the editor to release the plugin's editor first, then the editor goes inert and paints from cached strings.

On close, `StandaloneEditor::onTabClosed` frees the plugin index slot, drops the piano-roll registration and removes the mixer strip **before** the page dies (the roll connection's closures capture the page pointer), then `EngineRig::removeTab` runs its ordered teardown.

The revival path (`EngineRig::retryDeadPluginTab`) is the most order-sensitive flow here: stash the outgoing description, mark this page as reviving so a rebuild is not mistaken for a content change, raise the audio shield and settle, rebuild, push the retained state, lower the shield, re-seed the freeze-staleness counter from the fresh listener, and re-publish the frozen source (the teardown destroyed the render task, and a new one starts with no frozen source at all).

---

## Cross-references

- **Engine Tabs (Layers, Bass, Drums).md**, **Clips Page.md**, **Inst Page.md** - the other "+"-menu tab families.
- **Effect Racks.md** - VST3 **effects** in rack slots are the other consumer of the same hosting layer (`HostedPluginEffect`), and the rack slot window is where the "Run bridged" toggle lives.
- **Automation.md** (lanes for plugin parameters), **Mixer.md**, **Freeze and Export.md**.

---

## Differs from Carry-Forward

Carry-Forward predates plugin hosting entirely - it describes an app with no third-party plugin support at all. Everything in this document is new relative to that snapshot: the `Hosting` layer, the `Plugins` ribbon tab type and its `TabKind`/`PageKind`, the Plugins Bus (channel id 13) and Plugins Bus 2 (17), the `mixer_plugin_<n>` strip family, the out-of-process bridge and its two helper executables, and the Options > Plugins manager window.
