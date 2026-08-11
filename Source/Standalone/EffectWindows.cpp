#include "EffectWindows.h"
#include "EffectsPage.h"
#include "EffectEditorPanels.h"
#include "../PluginProcessor.h"
#include "../DSP/EffectParamMap.h"
#include "../Hosting/HostedPluginEffect.h"   // QA-ModelShell TS6: window fit + bridge toggle
#include "WorkspaceWindow.h"
// QA-Layout T18: the parametric visuals read their effect's live knob state at
// paint time, so this file needs the concrete DSP types to cast to.
#include "../DSP/ChorusDSP.h"
#include "../DSP/FlangerDSP.h"
#include "../DSP/PhaserDSP.h"
#include "../DSP/DelayDSP.h"
#include "../DSP/ReverbDSP.h"
#include "../DSP/CompressorDSP.h"
#include "../DSP/TransientShaperDSP.h"
#include "../DSP/SaturationDSP.h"

// ═════════════════════════════════════════════════════════════════ EffectSlotWindow

EffectSlotWindow::EffectSlotWindow (BaySickDAWProcessor& proc,
                                    int channelId,
                                    juce::String slotUuid,
                                    std::function<juce::String(int)> resolveChannelName,
                                    UndoContext undo)
    : mProc (proc),
      mChannelId (channelId),
      mUuid (std::move (slotUuid)),
      mResolveChannelName (std::move (resolveChannelName)),
      mUndo (undo)
{
    mSlot = std::make_unique<SlotComponent> (0);
    mSlot->setPresentation (SlotComponent::Presentation::PanelOnly);

    // Every mount path -- first build, Mode switch, preset load -- lands here,
    // so the stamps and the (type, variant)-keyed registration can never be
    // left behind by a rebuild that happened inside SlotComponent's own menus.
    mSlot->onEditorMounted = [this]
    {
        EffectRack* rack = nullptr;
        const int slot = resolveSlot (rack);
        if (rack != nullptr && slot >= 0)
            EffectsPage::stampAndRegisterSlotEditor (mProc, mChannelId, *rack, slot, *mSlot);
        refreshChrome();
    };

    addAndMakeVisible (*mSlot);

    mLed.onClick = [this]
    {
        EffectRack* rack = nullptr;
        const int slot = resolveSlot (rack);
        if (rack == nullptr || slot < 0) return;
        const bool nowBypassed = ! rack->isSlotBypassed (slot);
        rack->setSlotBypassed (slot, nowBypassed);
        mLed.setBypassed (nowBypassed);
    };
    mLed.setTooltip ("Bypass this effect");

    buildPanel();
    mTitle = windowTitle();
}

EffectSlotWindow::~EffectSlotWindow()
{
    stopTimer();
    // The bar is USUALLY already gone by now -- WorkspaceWindow declares
    // mContent before mPageMenu and members destruct in reverse order, so the
    // menu bar dies first.  mBar is a SafePointer precisely so that reads as
    // null instead of as freed memory; when it IS still alive (a rebuild rather
    // than a window teardown) the LED and the menu builder must be unhooked,
    // because the bar holds a non-owning pointer to our LED.
    if (auto* bar = mBar.getComponent())
    {
        bar->removeExtraRightComponent (&mLed);
        bar->setMenuBuilder (nullptr);
    }
    if (mSlot) mSlot->setRack (nullptr);
}

// Null unless this slot currently holds a hosted plugin.  Resolved fresh each
// call rather than cached -- the slot's DSP is replaced by preset loads, undo
// and project restores without this window being rebuilt.
Hosting::HostedPluginInstance* EffectSlotWindow::hostedPluginForSlot() const
{
    EffectRack* rack = nullptr;
    const int slot = resolveSlot (rack);

    if (rack == nullptr || slot < 0)
        return nullptr;

    if (auto* eff = dynamic_cast<Hosting::HostedPluginEffect*> (rack->getSlotEffect (slot)))
        return eff->getHosted();

    return nullptr;
}

int EffectSlotWindow::resolveSlot (EffectRack*& outRack) const
{
    outRack = EffectsPage::rackForChannelId (mProc.mVibeGraph, mChannelId);
    if (outRack == nullptr) return -1;
    for (int i = 0; i < EffectRack::kNumSlots; ++i)
        if (outRack->getSlotUuid (i) == mUuid) return i;
    return -1;
}

void EffectSlotWindow::buildPanel()
{
    EffectRack* rack = nullptr;
    const int slot = resolveSlot (rack);
    if (rack == nullptr || slot < 0) return;

    auto* eff = rack->getSlotEffect (slot);
    if (eff == nullptr) return;

    const auto type = rack->getSlotType (slot);

    mSlot->setSlotIndex (slot);
    mSlot->setRack (rack);
    mSlot->setChannelContext (&mProc.apvts,
                              EffectsPage::mixerPrefixForChannelId (mChannelId),
                              [] (int id) { return MixerChannelIds::friendlyName (id); });
    // Destroy the OUTGOING panel before building its replacement.  For our own
    // panels the order is immaterial, but a hosted plugin editor is the
    // plugin's own single editor instance -- build-then-replace would have the
    // new one asking for an editor while the old one still holds it, and JUCE's
    // VST3 wrapper warns that a second editor instance crashes some plugins.
    mSlot->setEditor (nullptr);
    mSlot->setEditor (createEffectEditor (eff, type));

    // QA-ModelShell TS6 (Jeff 2026-07-29): a hosted plugin's editor arrives at
    // whatever size the plugin declares, so the window fits ITSELF to that
    // rather than leaving the panel floating in a provisionally-sized frame.
    // Our own panels are unaffected -- they are built to the window, not the
    // other way round.
    if (auto* hosted = dynamic_cast<Hosting::HostedPluginEditor*> (mSlot->getEditor()))
        hosted->onNaturalSizeChanged = [this] (int w, int h)
        {
            auto* win = findParentComponentOfClass<WorkspaceWindow>();
            if (win == nullptr) return;

            // A programmatic fit to the plugin, not a size the user chose, and
            // WorkspaceWindow::resized() persists every settled geometry change
            // -- so without the suppression each fit overwrites the user's
            // stored window size.
            const WorkspaceWindow::ScopedSaveSuppress noSave;

            win->sizeToContent (w, h);

            // NO PLUGIN-DERIVED FLOOR -- same reasoning as PluginsPage: the
            // floor beats the workspace clamp inside sizeToContent, so a plugin
            // bigger than the workspace would force an oversized window.  0
            // leaves WorkspaceWindow's anti-degenerate minimum.
            win->setResizeFloor (0, 0);
        };
    // The panel's own timer must be able to ask whether the DSP it was built
    // against is STILL the one in this slot.  Its timer and this window's
    // rebuild poll are independent, so a project load can destroy the DSP and
    // the panel can tick before we notice -- see EditorPanelBase::liveDsp.
    if (auto* panel = dynamic_cast<EditorPanelBase*> (mSlot->getEditor()))
        panel->mResolveDsp = [this]() -> DSPBase*
        {
            EffectRack* r = nullptr;
            const int s = resolveSlot (r);
            return (r != nullptr && s >= 0) ? r->getSlotEffect (s) : nullptr;
        };

    // AFTER setEditor: this forwards into the panel, so calling it first would
    // be a no-op against a panel that does not exist yet.
    mSlot->setEditorUndoContext (mUndo);

    mBuiltRack    = rack;
    mBuiltType    = type;
    mBuiltVariant = EffectParamMap::variantOf (type, eff);
    mBuiltDsp     = eff;   // address only; see the member's note
}

void EffectSlotWindow::refreshChrome()
{
    EffectRack* rack = nullptr;
    const int slot = resolveSlot (rack);
    if (rack == nullptr || slot < 0) return;
    mLed.setBypassed (rack->isSlotBypassed (slot));
}

juce::String EffectSlotWindow::windowTitle() const
{
    EffectRack* rack = nullptr;
    const int slot = resolveSlot (rack);
    const juce::String strip = mResolveChannelName ? mResolveChannelName (mChannelId)
                                                   : juce::String();
    if (rack == nullptr || slot < 0)
        return strip;

    const juce::String fx = SlotComponent::slotDisplayName (rack, slot);
    return strip.isEmpty() ? fx : strip + " - " + fx;
}

void EffectSlotWindow::configureTitleStrip (PageMenuBar& bar)
{
    mBar = &bar;

    // QA-Layout T17: "Visual" in the Menu, and the way back once its window has
    // been closed.  BOTH closures resolve the DSP LIVE through the rack rather
    // than capturing a DSPBase* or a bool -- the effect in this slot changes
    // under an open window (swap, preset load, undo), which is precisely the
    // shape that made the locked-Freeze entry go stale (Jeff, 2026-08-05: it
    // captured its unlock flag by value and the checkbox appeared to do nothing).
    //
    // T20: the second closure is a PRESENCE gate now -- false and the row is not
    // built.  hasVisual(), not hasVisualFeed(): most of the visuals draw from DSP
    // state at paint time and publish nothing, so the feed question would hide
    // the entry on effects whose window works fine.
    bar.setVisualSlot (
        [this]
        {
            if (onOpenVisual) onOpenVisual (mChannelId, mUuid);
        },
        [this]() -> bool
        {
            EffectRack* rack = nullptr;
            const int slot = resolveSlot (rack);
            if (rack == nullptr || slot < 0) return false;

            auto* dsp = rack->getSlotEffect (slot);
            return dsp != nullptr && dsp->hasVisual();
        });

    // Locked call 3a: Basic/Advanced, Mode, SC and Presets all live in the
    // window's menu rather than as buttons, so the strip stays readable at the
    // small sizes these windows are meant to run at.
    bar.setMenuBuilder ([this] (juce::Component* anchor)
    {
        juce::PopupMenu m;

        if (mSlot->hasBasicMode())
            m.addItem (mSlot->isBasicMode() ? "Show Advanced Controls"
                                            : "Show Basic Controls",
                       [this] { mSlot->toggleBasicMode(); });

        if (mSlot->hasModeMenu())
            m.addItem ("Mode: " + mSlot->modeLabel() + "...", [this] { mSlot->showModeMenu(); });

        if (mSlot->hasScMenu())
            m.addItem (mSlot->scLabel() + "...", [this] { mSlot->showScMenu(); });

        if (m.getNumItems() > 0) m.addSeparator();
        m.addItem ("Presets...", [this] { mSlot->showPresetMenu(); });

        // QA-ModelShell TS6 (BLU-302, Jeff 2026-07-29): the per-plugin bridge
        // toggle lives on THIS menu, and only for a hosted plugin.
        //
        // A 32-bit plugin's row is SHOWN BUT DISABLED with the reason in the
        // text -- never hidden.  Hiding it would leave a user who dragged in a
        // 32-bit plugin with no explanation of why it behaves differently, which
        // is the same reasoning as the scanner reporting what it skipped.
        if (auto* hosted = hostedPluginForSlot())
        {
            m.addSeparator();

            // Automate (2026-08-02, ruling 2-b): the CREATION surface for the
            // hosted plugin's lanes.  Their applicators have been registered
            // since TS6, but a foreign editor has no right-click hook of
            // ours, so until this menu there was no way to create one.  Same
            // shape as the Plugins tab: last-touched fast path + chunked list.
            {
                // Bridged parameter lists arrive async after load, so the
                // load-time registration can have seen zero params.  Re-run
                // it here (idempotent) so a lane created from this menu has
                // a live applicator behind it.
                {
                    EffectRack* rack = nullptr;
                    const int slot = resolveSlot (rack);
                    if (rack != nullptr && slot >= 0)
                        EffectsPage::registerSlotAutomationFor (
                            mProc, mChannelId,
                            EffectsPage::channelPrefixForId (mChannelId),
                            *rack, slot);
                }

                const juce::String base =
                    EffectsPage::channelPrefixForId (mChannelId) + "_" + mUuid + "_vst_";

                juce::PopupMenu autoSub;

                juce::String lastId, lastName;
                hosted->getLastTouchedParam (lastId, lastName);
                {
                    const juce::String pid = base + lastId;
                    autoSub.addItem (lastId.isNotEmpty()
                                         ? "Last Touched: " + lastName
                                         : juce::String ("Last Touched (move a control in the plugin first)"),
                                     lastId.isNotEmpty(), false,
                                     [pid]
                                     {
                                         if (VKnobAutomation::sOnAutomate)
                                             VKnobAutomation::sOnAutomate (pid);
                                     });
                }
                autoSub.addSeparator();

                const auto params = hosted->getAutomatableParams();
                constexpr int kChunk = 30;
                auto addParamItem = [&base] (juce::PopupMenu& dest,
                                             const Hosting::HostedPluginInstance::AutomatableParam& p)
                {
                    const juce::String pid = base + p.id;
                    dest.addItem (p.name, true, false, [pid]
                    {
                        if (VKnobAutomation::sOnAutomate)
                            VKnobAutomation::sOnAutomate (pid);
                    });
                };

                if (params.size() <= kChunk)
                {
                    for (const auto& p : params)
                        addParamItem (autoSub, p);
                }
                else
                {
                    for (int start = 0; start < params.size(); start += kChunk)
                    {
                        const int end = juce::jmin (start + kChunk, params.size());
                        juce::PopupMenu chunk;
                        for (int i = start; i < end; ++i)
                            addParamItem (chunk, params.getReference (i));
                        autoSub.addSubMenu (juce::String (start + 1) + " - "
                                                + juce::String (end), chunk);
                    }
                }
                if (params.isEmpty())
                    autoSub.addItem (-1, "(no parameters reported)", false, false);

                m.addSubMenu ("Automate", autoSub);
            }

            const auto lockReason = hosted->getBridgeLockReason();

            if (lockReason.isNotEmpty())
            {
                m.addItem (-1, "Run bridged (" + lockReason + ")", false, true);
            }
            else
            {
                const bool bridged = hosted->getBridgePreference();
                m.addItem ("Run bridged (separate process)", true, bridged,
                           [this, bridged]
                           {
                               if (auto* h = hostedPluginForSlot())
                                   h->setBridgePreference (! bridged);

                               // Takes effect on the next load: switching a live
                               // plugin between in-process and bridged would mean
                               // tearing down its instance under the audio thread.
                               juce::NativeMessageBox::showMessageBoxAsync (
                                   juce::MessageBoxIconType::InfoIcon,
                                   "Plugin Bridge",
                                   "This takes effect the next time the plugin loads.");
                           });
            }

            // The automatic recovery watches the added-plugin list, and putting
            // a plugin back on that list is not the gesture every user reaches
            // for -- so the retry is also here, by hand.  Shown only while the
            // plugin is not alive: retrying a working one means nothing, and
            // this menu is short by design.
            if (! hosted->isAlive())
                m.addItem ("Retry Loading Plugin", [this]
                {
                    EffectRack* rack = nullptr;
                    const int slot = resolveSlot (rack);
                    if (rack == nullptr || slot < 0) return;
                    EffectsPage::retryDeadPluginSlot (mProc, mChannelId, *rack, slot);
                });
        }

        // QA-Layout T17: emits the "Visual" entry.  This window sets no FX Rack
        // or Freeze slot -- those are player-page actions -- so on an effect
        // window this contributes Visual alone.  Without the call the entry was
        // registered and never rendered, which is how it shipped invisible.
        if (auto* b = mBar.getComponent())
            b->appendStandardItems (m);

        m.showMenuAsync (juce::PopupMenu::Options()
                             .withTargetComponent (anchor != nullptr ? anchor
                                                                     : (juce::Component*) mBar));
    });

    bar.addExtraRightComponent (&mLed, 22);
    refreshChrome();
}

void EffectSlotWindow::resized()
{
    if (mSlot) mSlot->setBounds (getLocalBounds());
}

void EffectSlotWindow::parentHierarchyChanged()
{
    if (getPeer() != nullptr)
    {
        if (! isTimerRunning()) startTimerHz (10);
    }
    else if (isTimerRunning())
    {
        stopTimer();
    }
}

void EffectSlotWindow::timerCallback()
{
    EffectRack* rack = nullptr;
    const int slot = resolveSlot (rack);

    if (rack != nullptr && slot >= 0)
        mEverResolved = true;

    // Target gone: the slot was cleared, the rack's tab was deleted, or a
    // project load rebuilt the graph.  Close rather than sit on a dead panel.
    //
    // ONLY ONCE WE HAVE RESOLVED AT LEAST ONCE (Jeff, 2026-08-05).  The project
    // restore walker opens this window from its <Open key> record, and the
    // first poll can land BEFORE the rack has been populated with that uuid --
    // so a window restored with the project closed itself immediately and the
    // effect + visual windows never came back, while page windows (a different
    // restore path) did.  Same guard EffectEqWindow and EffectVisualWindow
    // already carry; this was the one that never got it.
    if (mEverResolved && (rack == nullptr || slot < 0))
    {
        stopTimer();
        // DEFERRED, and this is not optional: onRequestClose destroys the
        // window that owns this object, and we are inside this object's own
        // timer callback -- the caller keeps running on freed memory the moment
        // it returns.  Same shape as the close-button crash TS4 already paid
        // for (a control destroying itself from inside its own event).
        juce::Component::SafePointer<EffectSlotWindow> safeThis (this);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (auto* w = safeThis.getComponent())
                if (w->onRequestClose) w->onRequestClose();
        });
        return;
    }

    // Not resolved YET (restore in flight).  Wait for the rack rather than fall
    // through -- everything below dereferences `rack` and indexes by `slot`.
    if (rack == nullptr || slot < 0)
        return;

    // The effect moved index (a reorder, or a removal above it packed the slots
    // up).  The uuid is what identifies it, so follow that rather than keep
    // driving whatever landed in the old index.
    if (slot != mSlot->getSlotIndex()) mSlot->setSlotIndex (slot);

    // Backstop for rebuilds we did not initiate -- undo/redo, preset loads and
    // project restores all reach the rack without passing through this window.
    // A DIFFERENT rack object carrying our uuid means the graph rebuilt under
    // us (project load), and the SlotComponent's cached pointer is stale.
    auto* eff = rack->getSlotEffect (slot);
    const auto type = rack->getSlotType (slot);
    // `eff != mBuiltDsp` is the load-bearing one: a project load replaces a
    // slot's DSP in place, so rack, type and variant all still match while the
    // panel holds a pointer to the destroyed instance.  Address comparison
    // only -- mBuiltDsp is never dereferenced.
    if (rack != mBuiltRack
        || eff != mBuiltDsp
        || (eff != nullptr
            && (type != mBuiltType || EffectParamMap::variantOf (type, eff) != mBuiltVariant)))
        buildPanel();

    mLed.setBypassed (rack->isSlotBypassed (slot));

    const auto t = windowTitle();
    if (t != mTitle)
    {
        mTitle = t;
        if (onTitleChanged) onTitleChanged (t);
    }

    // QA-Layout T7: the window floor tracks the panel class + mode (Jeff's
    // approved generic sizes, WINDOW dims): pedal-native tiles 358x268,
    // Advanced 1047x268, Basic + toggle-less full panels 691x268.  Pushed on
    // the poll so a Mode swap or Basic toggle re-floors without new plumbing;
    // the change guard keeps it quiet.
    if (onFloorChanged && mSlot != nullptr)
    {
        int fw = 691, fh = 268;
        if (isPedalNativeType (mBuiltType))                        { fw = 358;  fh = 268; }
        else if (mSlot->hasBasicMode() && ! mSlot->isBasicMode())  { fw = 1047; fh = 268; }
        if (fw != mLastFloorW || fh != mLastFloorH)
        {
            mLastFloorW = fw; mLastFloorH = fh;
            onFloorChanged (fw, fh);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════ EffectEqWindow

EffectEqWindow::EffectEqWindow (BaySickDAWProcessor& proc,
                                int channelId,
                                bool isPre,
                                std::function<juce::String(int)> resolveChannelName)
    : mProc (proc),
      mChannelId (channelId),
      mIsPre (isPre),
      mResolveChannelName (std::move (resolveChannelName))
{
    mFallbackEq.prepare (44100.0, 512);

    mDisplay = std::make_unique<ParametricEQDisplay>();
    mDisplay->onLatencyChanged = [this]
    {
        mProc.setLatencySamples (mProc.mVibeGraph.updateBusLatencies());
    };
    // MID/SIDE are the title strip's buttons, not an in-graph pill.
    mDisplay->showMidSideToggle (false);
    addAndMakeVisible (*mDisplay);

    bindToChannel();
    mTitle = windowTitle();
}

EffectEqWindow::~EffectEqWindow()
{
    stopTimer();
    // Same destruction-order hazard as EffectSlotWindow's dtor -- the title
    // strip's menu bar is normally already gone when the content dies, so this
    // reads through a SafePointer rather than a raw one.
    if (auto* bar = mBar.getComponent())
        if (mDisplay != nullptr)
        {
            mDisplay->uninstallPageMenu (*bar);
            bar->setBankIndicator (nullptr);
        }
}

EQ8MsDSP* EffectEqWindow::resolveEq() const
{
    auto& vg = mProc.mVibeGraph;
    if (mIsPre) return EffectsPage::preEqForChannelId (vg, mChannelId);

    EffectRack* rack = nullptr;
    EQ8MsDSP*   eq   = nullptr;
    EffectsPage::resolveChannelDsp (vg, mChannelId, rack, eq);
    return eq;
}

void EffectEqWindow::bindToChannel()
{
    EQ8MsDSP* eq = resolveEq();
    mBoundEq = eq;

    const juce::String chanPrefix = EffectsPage::mixerPrefixForChannelId (mChannelId);
    const juce::String idPrefix   = mIsPre ? "_preeq_" : "_";

    if (eq != nullptr && chanPrefix.isNotEmpty())
        mDisplay->bindMsDSP (eq, &mProc.apvts,
                             chanPrefix + idPrefix + "mid_eq",
                             chanPrefix + idPrefix + "side_eq");
    else
        mDisplay->bindMsDSP (eq != nullptr ? eq : &mFallbackEq);

    mDisplay->setStripContext (chanPrefix,
                               [] (int srcChId) { return MixerChannelIds::friendlyName (srcChId); });
    mDisplay->setShowMid (mShowMid);
}

juce::String EffectEqWindow::windowTitle() const
{
    const juce::String strip = mResolveChannelName ? mResolveChannelName (mChannelId)
                                                   : juce::String();
    const juce::String what = mIsPre ? "Pre EQ8 M/S" : "Post EQ8 M/S";
    return strip.isEmpty() ? what : strip + " - " + what;
}

void EffectEqWindow::configureTitleStrip (PageMenuBar& bar)
{
    mBar = &bar;

    // Two tabs, but the inactive one OPENS THE OTHER WINDOW instead of swapping
    // this window's contents (Jeff's spec) -- so the pair can be on screen
    // together, which is the point of splitting them.
    bar.setTabSlots ({ "Pre EQ", "Post EQ" },
                     [this] (int idx)
                     {
                         const bool wantPre = (idx == 0);
                         if (wantPre == mIsPre)
                         {
                             if (mBar != nullptr) mBar->updateTabActive (mIsPre ? 0 : 1);
                             return;
                         }
                         if (onOpenOtherEq) onOpenOtherEq (wantPre);
                         // Selection stays on OUR tab: this window is still
                         // showing what it always was.
                         if (mBar != nullptr) mBar->updateTabActive (mIsPre ? 0 : 1);
                     },
                     mIsPre ? 0 : 1);

    bar.setMidSideSlots ([this] { mShowMid = true;  mDisplay->setShowMid (true);
                                  if (mBar != nullptr) mBar->updateMidSideActive (true); },
                         [this] { mShowMid = false; mDisplay->setShowMid (false);
                                  if (mBar != nullptr) mBar->updateMidSideActive (false); },
                         mShowMid);
    bar.setMidSideVisible (true);

    mDisplay->installPageMenu (bar);
    mDisplay->refreshBankIndicator();
    bar.setBankIndicator (mDisplay->getBankIndicator());
}

void EffectEqWindow::resized()
{
    if (mDisplay) mDisplay->setBounds (getLocalBounds().reduced (4));
}

void EffectEqWindow::parentHierarchyChanged()
{
    if (getPeer() != nullptr)
    {
        if (! isTimerRunning()) startTimerHz (30);
    }
    else if (isTimerRunning())
    {
        stopTimer();
    }
}

void EffectEqWindow::timerCallback()
{
    auto* eq = resolveEq();

    // The channel DIED (its strip was deleted): left open, this window sat
    // bound to the display-only fallback forever -- a zombie editing an EQ
    // nothing plays through, against the registry's own close-on-target-loss
    // design.  "Never resolved yet" is a different state: the fallback's
    // stated role is drawing before the graph has built the node.
    if (eq == nullptr && mEverResolved)
    {
        if (onRequestClose) { onRequestClose(); return; }
    }
    if (eq != nullptr)
        mEverResolved = true;

    // The node under this window can be rebuilt without the window hearing
    // about it (a strip respawn, a graph rebuild).  Re-bind when the resolved
    // DSP is a DIFFERENT object, or the display would keep drawing -- and
    // writing -- into the old one.
    if (eq != mBoundEq)
        bindToChannel();

    // Same poll the Effects page ran: pulls the pre/post spectrum feeds and the
    // dynamic-band state back out of the bound DSP.
    if (mDisplay) mDisplay->syncFromDSP();

    const auto t = windowTitle();
    if (t != mTitle)
    {
        mTitle = t;
        if (onTitleChanged) onTitleChanged (t);
    }
}

// ═══════════════════════════════════════════════════ T18 parametric visual draws
//
// These read the effect's knob state at PAINT time rather than consuming the
// audio feed, which is what makes turning a knob move the picture immediately:
// there is nothing to publish and no ring buffer to wait for.  The 30 Hz shared
// clock (EffectVisualClock) is what animates the moving parts.
//
// The captions are not decoration.  This app's audience has never made music,
// and an unlabelled graph teaches nothing -- so every visual whose meaning is
// not self-evident says what it is showing.
namespace T18Draw
{
    // Shared chrome: faint grid + a centre line, so a flat curve still reads as
    // a measurement rather than an empty box.
    static void grid (juce::Graphics& g, juce::Rectangle<float> r, int cols = 8)
    {
        g.setColour (VC::Panel.brighter (0.06f));
        for (int i = 1; i < cols; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) cols;
            g.drawLine (x, r.getY(), x, r.getBottom(), 0.5f);
        }
        g.setColour (VC::Panel.brighter (0.14f));
        g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 0.5f);
    }

    static void label (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t)
    {
        g.setColour (VC::TextDim);
        g.setFont (9.0f);
        g.drawText (t, r, juce::Justification::topLeft, false);
    }

    // ── The audio itself: scrolling in-vs-out (Jeff, 2026-08-06 rework) ──────
    // Every visual leads with THIS -- the actual signal passing through, newest
    // at the right -- because a picture of the settings with no sound in it
    // shows a beginner nothing about what the effect is DOING.  Ghost outline =
    // what went in; filled shape = what came out.  The difference between the
    // two IS the effect.
    static void audioInOut (juce::Graphics& g, juce::Rectangle<float> area,
                            EffectVisualFeed* feed, juce::Colour outColour,
                            bool drawGhostIn = true)
    {
        auto r = area.reduced (2.0f);
        grid (g, r, 8);
        if (feed == nullptr || r.getWidth() < 2.0f) return;

        const int      cols  = juce::jmin ((int) r.getWidth(), EffectVisualFeed::kSize);
        const uint32_t write = feed->writeIndex();
        const float    midY  = r.getCentreY();
        const float    halfH = r.getHeight() * 0.5f;

        // Output: filled vertical strokes, a solid waveform body.
        g.setColour (outColour.withAlpha (0.85f));
        for (int i = 0; i < cols; ++i)
        {
            const auto& c = feed->at (write - (uint32_t) (cols - i));
            const float x = r.getX() + (float) i;
            const float e = juce::jlimit (0.0f, 1.0f, c.hi);
            if (e > 0.002f)
                g.drawLine (x, midY - e * halfH * 0.94f, x, midY + e * halfH * 0.94f, 1.0f);
        }

        // Input: a ghost OUTLINE over it (top edge only, mirrored), thin, so in
        // and out are readable at a glance without the two filling over each
        // other.
        if (drawGhostIn)
        {
            juce::Path top, bot;
            bool started = false;
            for (int i = 0; i < cols; ++i)
            {
                const auto& c = feed->at (write - (uint32_t) (cols - i));
                const float x = r.getX() + (float) i;
                const float e = juce::jlimit (0.0f, 1.0f, c.a);
                const float yT = midY - e * halfH * 0.94f;
                const float yB = midY + e * halfH * 0.94f;
                if (! started) { top.startNewSubPath (x, yT); bot.startNewSubPath (x, yB); started = true; }
                else           { top.lineTo (x, yT);          bot.lineTo (x, yB); }
            }
            g.setColour (VC::TextDim.withAlpha (0.65f));
            g.strokePath (top, juce::PathStrokeType (1.0f));
            g.strokePath (bot, juce::PathStrokeType (1.0f));
        }
    }

    // Recent input level out of the feed (max of the last ~30 columns' `a`), so
    // a display can react to how hard the signal is ACTUALLY hitting right now
    // without any extra publishing machinery.
    static float recentInputLevel (EffectVisualFeed* feed)
    {
        if (feed == nullptr) return 0.0f;
        const uint32_t write = feed->writeIndex();
        float pk = 0.0f;
        for (uint32_t i = 1; i <= 30; ++i)
            pk = juce::jmax (pk, feed->at (write - i).a);
        return juce::jlimit (0.0f, 1.0f, pk);
    }

    // ── LFO scope + comb response (Chorus / Flanger / Phaser) ────────────────
    // Left: one cycle of the LFO's ACTUAL wave with a dot riding the real phase,
    // so "rate" and "shape" stop being abstract numbers.
    // Right: what that modulation does to the frequency response -- the notches
    // a flanger/chorus comb produces, or the phaser's allpass notches.  Drawn as
    // a curve rather than a spectrum because it is the SHAPE that explains the
    // effect, and a spectrum needs signal present to show anything at all.
    static void lfoScope (juce::Graphics& g, juce::Rectangle<float> area,
                          float phase01, int wave, float depth01,
                          std::function<float(int, float)> shapeFn)
    {
        auto r = area.reduced (2.0f);
        grid (g, r);

        juce::Path p;
        constexpr int kPts = 96;
        for (int i = 0; i < kPts; ++i)
        {
            const float t = (float) i / (float) (kPts - 1);
            const float v = shapeFn ? juce::jlimit (-1.0f, 1.0f, shapeFn (wave, t))
                                    : std::sin (t * juce::MathConstants<float>::twoPi);
            const float x = r.getX() + t * r.getWidth();
            const float y = r.getCentreY() - v * r.getHeight() * 0.42f * depth01;
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff00fff2));
        g.strokePath (p, juce::PathStrokeType (1.4f));

        // The dot is the whole reason this is animated: it shows WHERE in the
        // sweep the effect currently is, which is the part a static curve of the
        // waveform cannot convey.
        const float t  = juce::jlimit (0.0f, 1.0f, phase01);
        const float v  = shapeFn ? juce::jlimit (-1.0f, 1.0f, shapeFn (wave, t))
                                 : std::sin (t * juce::MathConstants<float>::twoPi);
        const float dx = r.getX() + t * r.getWidth();
        const float dy = r.getCentreY() - v * r.getHeight() * 0.42f * depth01;
        g.setColour (juce::Colour (0xffff9100));
        g.fillEllipse (dx - 2.5f, dy - 2.5f, 5.0f, 5.0f);
    }

    // Comb / notch response.  `notches` is how many nulls to draw across the
    // strip and `depthN` how deep they cut -- both derived by the caller from
    // the effect's own controls, so the picture moves with the knobs.
    static void combResponse (juce::Graphics& g, juce::Rectangle<float> area,
                              float notches, float depthN, float sweep01)
    {
        auto r = area.reduced (2.0f);
        grid (g, r, 6);

        juce::Path p;
        constexpr int kPts = 160;
        const float n = juce::jmax (0.5f, notches);
        for (int i = 0; i < kPts; ++i)
        {
            const float t = (float) i / (float) (kPts - 1);
            // cos-comb: |cos| dips to zero at each null.  sweep01 slides the
            // whole pattern, which is exactly what the LFO does to it.
            const float c = std::cos ((t * n + sweep01) * juce::MathConstants<float>::pi);
            const float mag = 1.0f - depthN * (1.0f - std::abs (c));
            const float x = r.getX() + t * r.getWidth();
            const float y = r.getBottom() - juce::jlimit (0.0f, 1.0f, mag) * r.getHeight() * 0.9f;
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        g.setColour (VC::TextDim);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    // ── Reverb: decay envelope, three named regions ──────────────────────────
    // Pre-delay / early reflections / tail get their own regions because they
    // are three different things people conflate: silence before anything comes
    // back, the discrete first bounces, then the diffuse wash.
    static void reverbEnvelope (juce::Graphics& g, juce::Rectangle<float> area,
                                float preDelayMs, float decaySec, float erDb,
                                float diffusion)
    {
        auto r = area.reduced (2.0f);
        grid (g, r, 6);

        const float spanMs = juce::jmax (500.0f, decaySec * 1000.0f * 1.05f);
        const float preX   = r.getX() + (preDelayMs / spanMs) * r.getWidth();
        // Early reflections run from pre-delay for roughly the first 80 ms --
        // the conventional boundary where discrete bounces stop being separable.
        const float erX    = r.getX() + ((preDelayMs + 80.0f) / spanMs) * r.getWidth();

        // Region shading, faint enough to read as background.
        g.setColour (VC::Panel.brighter (0.05f));
        g.fillRect (juce::Rectangle<float> (r.getX(), r.getY(), preX - r.getX(), r.getHeight()));
        g.setColour (juce::Colour (0xffff9100).withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (preX, r.getY(), erX - preX, r.getHeight()));

        // Early reflections: discrete spikes, denser as diffusion rises.
        const int nEr = juce::jlimit (3, 14, (int) (4.0f + diffusion * 10.0f));
        const float erAmp = juce::jlimit (0.0f, 1.0f, std::pow (10.0f, erDb / 20.0f));
        g.setColour (juce::Colour (0xffff9100).withAlpha (0.9f));
        for (int i = 0; i < nEr; ++i)
        {
            const float f = (float) (i + 1) / (float) nEr;
            const float x = preX + (erX - preX) * f;
            const float h = erAmp * (1.0f - f * 0.6f) * r.getHeight() * 0.8f;
            g.drawLine (x, r.getBottom(), x, r.getBottom() - h, 1.2f);
        }

        // Tail: exponential decay to -60 dB over the decay time (RT60 is
        // literally defined as that), so the curve IS the number on the knob.
        juce::Path tail;
        constexpr int kPts = 120;
        for (int i = 0; i < kPts; ++i)
        {
            const float t   = (float) i / (float) (kPts - 1);
            const float ms  = preDelayMs + 80.0f + t * juce::jmax (1.0f, spanMs - preDelayMs - 80.0f);
            const float sec = ms * 0.001f;
            const float a   = std::pow (10.0f, -3.0f * sec / juce::jmax (0.05f, decaySec));
            const float x   = r.getX() + (ms / spanMs) * r.getWidth();
            const float y   = r.getBottom() - juce::jlimit (0.0f, 1.0f, a) * r.getHeight() * 0.8f;
            if (i == 0) tail.startNewSubPath (x, y);
            else        tail.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff00fff2));
        g.strokePath (tail, juce::PathStrokeType (1.4f));

        g.setColour (VC::TextDim);
        g.setFont (8.5f);
        g.drawText ("pre",  juce::Rectangle<float> (r.getX(), r.getBottom() - 11.0f, preX - r.getX(), 10.0f),
                    juce::Justification::centred, false);
        g.drawText ("early", juce::Rectangle<float> (preX, r.getBottom() - 11.0f, erX - preX, 10.0f),
                    juce::Justification::centred, false);
        g.drawText ("tail",  juce::Rectangle<float> (erX, r.getBottom() - 11.0f, r.getRight() - erX, 10.0f),
                    juce::Justification::centred, false);
    }

    // ── Compressor: transfer curve, knee drawn, live operating point ─────────
    // in dB on both axes over a -60..0 window.  A compressor is the effect a
    // beginner is least able to HEAR, so the picture has to carry the idea:
    // below threshold the line is 45 degrees (nothing happens), above it the
    // line tilts (louder in, less louder out), and the knee is how gradually
    // the bend arrives.
    static void compCurve (juce::Graphics& g, juce::Rectangle<float> area,
                           float thrDb, float ratio, float kneeDb, float grDb)
    {
        auto r = area.reduced (2.0f);
        constexpr float kMin = -60.0f;
        auto xFor = [&] (float db) { return r.getX() + (db - kMin) / -kMin * r.getWidth(); };
        auto yFor = [&] (float db) { return r.getBottom() - (db - kMin) / -kMin * r.getHeight(); };

        grid (g, r, 6);

        // Unity reference, so the compressed curve reads as a DEPARTURE from it.
        g.setColour (VC::Panel.brighter (0.16f));
        g.drawLine (xFor (kMin), yFor (kMin), xFor (0.0f), yFor (0.0f), 1.0f);

        const float R  = juce::jmax (1.0f, ratio);
        const float K  = juce::jmax (0.0f, kneeDb);
        auto outFor = [&] (float in) -> float
        {
            // Standard soft-knee: quadratic interpolation across the knee width,
            // straight ratio above it.  Same shape the DSP implements, so the
            // drawing is not a separate idea of what the compressor does.
            if (K > 0.0f && in > thrDb - K * 0.5f && in < thrDb + K * 0.5f)
            {
                const float d = in - thrDb + K * 0.5f;
                return in + (1.0f / R - 1.0f) * d * d / (2.0f * K);
            }
            return in <= thrDb ? in : thrDb + (in - thrDb) / R;
        };

        juce::Path p;
        constexpr int kPts = 120;
        for (int i = 0; i < kPts; ++i)
        {
            const float in = kMin + (float) i / (float) (kPts - 1) * -kMin;
            const float x = xFor (in), y = yFor (juce::jlimit (kMin, 0.0f, outFor (in)));
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff00fff2));
        g.strokePath (p, juce::PathStrokeType (1.6f));

        // Threshold marker.
        g.setColour (juce::Colour (0xffff9100).withAlpha (0.55f));
        g.drawLine (xFor (thrDb), r.getY(), xFor (thrDb), r.getBottom(), 0.8f);

        // Live dot: solve the curve backwards from the gain reduction actually
        // happening right now.  GR = in - out, and above the knee that is
        // (in - T)(1 - 1/R), so the input level follows from the one number the
        // DSP already publishes.
        if (grDb > 0.05f && R > 1.0f)
        {
            const float in = thrDb + grDb / juce::jmax (0.01f, 1.0f - 1.0f / R);
            if (in <= 0.0f)
            {
                g.setColour (juce::Colour (0xffff9100));
                g.fillEllipse (xFor (in) - 3.0f, yFor (outFor (in)) - 3.0f, 6.0f, 6.0f);
            }
        }
    }

    // ── Transient Shaper: before/after envelope ──────────────────────────────
    // One drum hit, ghosted twice: what went in, and what the Attack/Release
    // settings turn it into.  This is the only honest way to show an effect
    // whose whole job is a difference between two envelopes.
    static void transientEnvelope (juce::Graphics& g, juce::Rectangle<float> area,
                                   float attack, float release,
                                   int attShape, int relShape)
    {
        auto r = area.reduced (2.0f);
        grid (g, r, 8);

        // Shape index bends how fast each stage moves: Sharp snaps, Soft eases.
        auto shapeExp = [] (int s) { return s == 0 ? 0.55f : (s == 2 ? 1.8f : 1.0f); };
        const float aE = shapeExp (attShape), rE = shapeExp (relShape);

        auto envAt = [&] (float t, bool shaped) -> float
        {
            // Attack portion is the first 12 % of the strip; the rest decays.
            constexpr float kAtt = 0.12f;
            if (t < kAtt)
            {
                float a = std::pow (t / kAtt, aE);
                if (shaped) a *= (1.0f + attack);     // attack -1..1 cuts or boosts the spike
                return juce::jlimit (0.0f, 1.4f, a);
            }
            const float d = (t - kAtt) / (1.0f - kAtt);
            float v = std::pow (1.0f - d, 2.2f * rE);
            if (shaped) v = std::pow (juce::jlimit (0.0f, 1.0f, v),
                                      juce::jmax (0.2f, 1.0f - release));  // release +1 = longer
            const float peak = shaped ? (1.0f + attack) : 1.0f;
            return juce::jlimit (0.0f, 1.4f, v * peak);
        };

        auto drawEnv = [&] (bool shaped, juce::Colour c, float thickness)
        {
            juce::Path p;
            constexpr int kPts = 140;
            for (int i = 0; i < kPts; ++i)
            {
                const float t = (float) i / (float) (kPts - 1);
                const float v = envAt (t, shaped) / 1.4f;
                const float x = r.getX() + t * r.getWidth();
                const float y = r.getBottom() - v * r.getHeight() * 0.92f;
                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (thickness));
        };

        drawEnv (false, VC::TextDim.withAlpha (0.55f), 1.0f);   // ghost = input
        drawEnv (true,  juce::Colour (0xff00fff2),     1.6f);   // solid = result
    }

    // ── Saturation / Tape: harmonic bars, measured AT the live signal level ──
    // One sine cycle goes through the effect's real shaper at the amplitude the
    // audio is ACTUALLY hitting it with, and harmonics 1-8 are correlated out
    // of what comes back.  That is the whole point (Jeff, 2026-08-06): a soft
    // clipper adds nothing at low level and plenty when driven, so the bars
    // rise and fall with the signal passing through -- where the harmonics are
    // hitting, as they hit.  Ghost bars behind them = the potential at full
    // drive, so a quiet passage still shows what the effect WOULD add.
    static void harmonicBars (juce::Graphics& g, juce::Rectangle<float> area,
                              const std::function<float(float)>& shaper,
                              float liveLevel01)
    {
        auto r = area.reduced (2.0f);
        grid (g, r, 8);
        if (! shaper) return;

        constexpr int kN = 128, kH = 8;
        auto measure = [&shaper] (float amp, float (&mag)[kH + 1])
        {
            float wave[kN];
            for (int i = 0; i < kN; ++i)
            {
                const float ph = (float) i / (float) kN * juce::MathConstants<float>::twoPi;
                wave[i] = shaper (amp * std::sin (ph));
            }
            for (int h = 1; h <= kH; ++h)
            {
                float re = 0.0f, im = 0.0f;
                for (int i = 0; i < kN; ++i)
                {
                    const float ph = (float) (h * i) / (float) kN * juce::MathConstants<float>::twoPi;
                    re += wave[i] * std::cos (ph);
                    im += wave[i] * std::sin (ph);
                }
                mag[h] = 2.0f * std::sqrt (re * re + im * im) / (float) kN;
            }
        };

        float ghostMag[kH + 1] = {}, liveMag[kH + 1] = {};
        measure (0.8f, ghostMag);
        const bool haveLive = liveLevel01 > 0.02f;
        if (haveLive) measure (juce::jmax (0.05f, liveLevel01 * 0.8f), liveMag);

        const float barW = r.getWidth() / (float) kH;
        g.setFont (8.5f);
        auto heightFor = [&r] (const float (&mag)[kH + 1], int h) -> float
        {
            // dB below the fundamental over a 60 dB window -- linear magnitude
            // flattens everything past the 2nd into invisibility.
            const float ref = juce::jmax (1.0e-6f, mag[1]);
            const float db  = 20.0f * std::log10 (juce::jmax (1.0e-6f, mag[h] / ref));
            return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * (r.getHeight() - 12.0f);
        };

        for (int h = 1; h <= kH; ++h)
        {
            const float x = r.getX() + barW * (float) (h - 1);

            const float gh = heightFor (ghostMag, h);
            g.setColour (VC::TextDim.withAlpha (0.22f));
            g.fillRect (juce::Rectangle<float> (x + barW * 0.2f, r.getBottom() - 12.0f - gh,
                                                barW * 0.6f, gh));
            if (haveLive)
            {
                const float lh = heightFor (liveMag, h);
                g.setColour (h == 1 ? VC::TextDim.withAlpha (0.6f)
                                    : juce::Colour (0xffff9100).withAlpha (0.9f));
                g.fillRect (juce::Rectangle<float> (x + barW * 0.28f, r.getBottom() - 12.0f - lh,
                                                    barW * 0.44f, lh));
            }
            g.setColour (VC::TextDim);
            g.drawText (juce::String (h),
                        juce::Rectangle<float> (x, r.getBottom() - 11.0f, barW, 10.0f),
                        juce::Justification::centred, false);
        }
    }
}

// ═══════════════════════════════════════════════════════════ EffectVisualWindow

EffectVisualWindow::EffectVisualWindow (BaySickDAWProcessor& proc,
                                        int channelId,
                                        juce::String slotUuid,
                                        std::function<juce::String(int)> resolveChannelName)
    : mProcessor (proc),
      mChannelId (channelId),
      mUuid (std::move (slotUuid)),
      mResolveName (std::move (resolveChannelName))
{
    mStrip = std::make_unique<EffectVisualStrip>();

    // The strip asks the MODEL for its feed every time it needs one, so a DSP
    // destroyed under an open window can never be read or written through a
    // stale pointer.  Captures only this window (which owns the strip), never
    // a DSP.
    mStrip->setFeedResolver ([this]() -> EffectVisualFeed*
    {
        auto* rack = EffectsPage::rackForChannelId (mProcessor.mVibeGraph, mChannelId);
        if (rack == nullptr) return nullptr;

        for (int s = 0; s < EffectRack::kNumSlots; ++s)
            if (rack->getSlotUuid (s) == mUuid)
                if (auto* d = rack->getSlotEffect (s))
                    return &d->visualFeed();

        return nullptr;
    });

    addAndMakeVisible (*mStrip);
    rebind();
}

EffectVisualWindow::~EffectVisualWindow()
{
    // Clear the resolver FIRST: it captures `this`, and the strip may still
    // release its watcher while being destroyed.  Cleared, the release sees no
    // live feed and drops the count instead of calling into a half-destroyed
    // window.  (The strip is destroyed below with the rest of our members.)
    if (mStrip) mStrip->setFeedResolver (nullptr);
}

juce::String EffectVisualWindow::windowTitle() const { return mTitle; }

DSPBase* EffectVisualWindow::resolveDsp() const
{
    auto* rack = EffectsPage::rackForChannelId (mProcessor.mVibeGraph, mChannelId);
    if (rack == nullptr) return nullptr;

    for (int s = 0; s < EffectRack::kNumSlots; ++s)
        if (rack->getSlotUuid (s) == mUuid)
            return rack->getSlotEffect (s);

    return nullptr;
}

void EffectVisualWindow::configureTitleStrip (PageMenuBar& bar)
{
    mBar = &bar;

    bar.setMenuBuilder ([this] (juce::Component* anchor)
    {
        juce::PopupMenu m;

        // Wording states what the item DOES rather than what the state is: this
        // menu has one entry, so a ticked "Locked to Effect Window" would read as
        // a label with nothing to be consistent against.
        const bool locked = isTetherLocked && isTetherLocked();
        m.addItem (locked ? "Unlock from Effect Window" : "Lock to Effect Window",
                   [this] { if (onToggleTetherLock) onToggleTetherLock(); });

        m.showMenuAsync (juce::PopupMenu::Options()
                             .withTargetComponent (anchor != nullptr ? anchor
                                                                     : (juce::Component*) mBar));
    });
}

void EffectVisualWindow::resized()
{
    if (mStrip) mStrip->setBounds (getLocalBounds().reduced (6));
}

void EffectVisualWindow::parentHierarchyChanged()
{
    if (getPeer() != nullptr) { if (! isTimerRunning()) startTimerHz (4); }
    else                        stopTimer();
}

void EffectVisualWindow::rebind()
{
    auto* rack = EffectsPage::rackForChannelId (mProcessor.mVibeGraph, mChannelId);

    DSPBase*   dsp  = nullptr;
    EffectType type = EffectType::None;
    int        slot = -1;

    if (rack != nullptr)
    {
        for (int s = 0; s < EffectRack::kNumSlots; ++s)
        {
            if (rack->getSlotUuid (s) != mUuid) continue;
            slot = s;
            type = rack->getSlotType (s);
            dsp  = rack->getSlotEffect (s);
            break;
        }
    }

    // Resolved once and then lost = the slot was cleared or the rack died.  Ask
    // the owner to close rather than sitting on a window watching nothing.
    if (mEverResolved && dsp == nullptr)
    {
        if (onRequestClose) onRequestClose();
        return;
    }

    if (dsp != nullptr) mEverResolved = true;

    const juce::String fxName = (rack != nullptr && slot >= 0)
                                  ? SlotComponent::slotDisplayName (rack, slot)
                                  : juce::String();

    if (dsp != mBoundDsp)
    {
        mBoundDsp  = dsp;   // address only, for change detection
        mBoundType = type;
        mStrip->setCaption (fxName);

        // Per-effect drawing lives HERE rather than inside the strip: the strip
        // owns timing, gating and chrome; what a column MEANS is the effect's
        // business.  T18 adds the remaining nine branches.
        //
        // Limiter (T13 / BLU-110): output level as the envelope, gain reduction
        // hanging from the top in cyan, ceiling as the orange line it is
        // measured against.  Colours are the approved #00FFF2 / #FF9100 pair.
        // T18 caption upgrade: the plan's manual rule says the GR trace's
        // meaning is stated AT the component; a bare effect name did not.
        if (mBoundType == EffectType::Limiter)
        {
            mStrip->setCaption (fxName + " - cyan dips = volume being pulled down; orange = ceiling");
            mStrip->setPaintFn ([] (juce::Graphics& g, juce::Rectangle<float> area,
                                    EffectVisualFeed* feed)
            {
                EffectVisualDraw::scrollingHistory (g, area, feed,
                                                    VC::TextDim,                    // level envelope
                                                    juce::Colour (0xff00fff2),      // GR
                                                    true,
                                                    juce::Colour (0xffff9100));     // ceiling
            });
        }
        // ── T18: Chorus / Flanger / Phaser ───────────────────────────────────
        // One shape for all three: they are the same idea (an LFO sweeping a
        // filter or a delay) wearing different clothes, so teaching them with
        // three different pictures would be teaching a difference that is not
        // there.  Left is the LFO, right is what it does to the sound.
        //
        // Captures `this` and resolves the DSP per paint -- never a cached
        // pointer, same rule as the feed resolver above.
        else if (mBoundType == EffectType::Chorus
              || mBoundType == EffectType::Flanger
              || mBoundType == EffectType::Phaser)
        {
            // Audio-first (Jeff, 2026-08-06): main = the sound itself, ghost in
            // vs solid out -- the swoosh is visible in the output envelope.
            // Side = the LFO doing the sweeping and the comb it drags through.
            mStrip->setCaption (fxName + " - grey outline = in, solid = out | LFO + response");
            mStrip->setPaintFn ([this] (juce::Graphics& g, juce::Rectangle<float> area,
                                        EffectVisualFeed* feed)
            {
                auto* dsp = resolveDsp();
                if (dsp == nullptr) return;

                auto right = area.removeFromRight (area.getWidth() * 0.38f);
                auto left  = right.removeFromTop  (right.getHeight() * 0.5f);

                T18Draw::audioInOut (g, area, feed, juce::Colour (0xff00fff2));
                T18Draw::label (g, area.reduced (3.0f), "Audio");

                float phase = 0.0f, depth = 1.0f, notches = 4.0f, depthN = 0.9f;
                int   wave  = 0;
                std::function<float(int, float)> shapeFn;

                if (auto* c = dynamic_cast<ChorusDSP*> (dsp))
                {
                    phase   = c->lfoPhase (0);
                    wave    = c->lfoParams[0].wave;
                    shapeFn = [] (int w, float t) { return ChorusDSP::lfoShapeForDisplay (w, t); };
                    depth   = juce::jlimit (0.05f, 1.0f, c->depth / 20.0f);
                    // Comb spacing is set by the base delay: a short delay puts
                    // the nulls far apart, a long one packs them in.
                    notches = juce::jlimit (1.0f, 24.0f, c->delayMs * 0.8f);
                    depthN  = juce::jlimit (0.0f, 1.0f, c->wet);
                }
                else if (auto* f = dynamic_cast<FlangerDSP*> (dsp))
                {
                    phase   = f->lfoPhase();
                    // Shape morphs sine -> triangle; the scope shows which.
                    wave    = f->mShape > 0.5f ? 1 : 0;
                    depth   = juce::jlimit (0.05f, 1.0f, f->mDepth / 10.0f);
                    notches = juce::jlimit (1.0f, 24.0f, juce::jmax (0.5f, f->mDelay + f->mDepth) * 1.6f);
                    // Feedback is what turns a gentle comb into the metallic
                    // sweep, so it drives how deep the nulls cut.
                    depthN  = juce::jlimit (0.0f, 1.0f, std::abs (f->mFeedback) * 0.5f + f->mWet * 0.5f);
                }
                else if (auto* p = dynamic_cast<PhaserDSP*> (dsp))
                {
                    phase   = p->lfoPhase();
                    wave    = p->mLFOWaveIdx;
                    depth   = juce::jlimit (0.05f, 1.0f, p->mDepth);
                    // A phaser's notch count is its stage count, not a delay --
                    // two allpass stages make one notch.
                    notches = juce::jlimit (1.0f, 12.0f, (float) p->mNumStages * 0.5f);
                    depthN  = juce::jlimit (0.0f, 1.0f, std::abs (p->mFeedback) * 0.4f + p->mWet * 0.6f);
                }

                T18Draw::lfoScope      (g, left,  phase, wave, depth, shapeFn);
                T18Draw::combResponse  (g, right, notches, depthN, phase);
                T18Draw::label (g, left .reduced (3.0f), "LFO");
                T18Draw::label (g, right.reduced (3.0f), "Notches");
            });
        }
        // ── T18: Delay ───────────────────────────────────────────────────────
        // The feedback transfer curve arrived here from the panel at T20, so all
        // of the Delay's drawing is in one place: repeats on the left, the curve
        // that shapes them on the right.
        else if (mBoundType == EffectType::Delay)
        {
            // Audio-first (Jeff, 2026-08-06): main = the ACTUAL echoes -- the
            // DSP publishes the wet path separately from the mix, so every
            // repeat is a visible bump even while the dry plays.  Beat-grid
            // verticals ride over the audio so a synced delay reads as bumps
            // landing on the lines.
            mStrip->setCaption (fxName + " - solid = the echoes, grey = what you played | drive curve");
            mStrip->setPaintFn ([this] (juce::Graphics& g, juce::Rectangle<float> area,
                                        EffectVisualFeed* feed)
            {
                auto* d = dynamic_cast<DelayDSP*> (resolveDsp());
                if (d == nullptr) return;

                auto left  = area.removeFromLeft (area.getWidth() * 0.68f);
                auto right = area;

                T18Draw::audioInOut (g, left, feed, juce::Colour (0xff00fff2));

                // Beat grid over the audio.  The strip scrolls one column per
                // block, so a beat's width in columns = beat time / block time
                // -- derived from the DSP's own BPM so grid and echoes cannot
                // disagree.
                {
                    const float bpm     = juce::jmax (20.0f, d->hostBpmForDisplay());
                    const float beatMs  = 60000.0f / bpm;
                    const float blockMs = d->lastBlockMsForDisplay();
                    if (blockMs > 0.01f)
                    {
                        auto rr = left.reduced (2.0f);
                        const float colsPerBeat = beatMs / blockMs;
                        if (colsPerBeat > 4.0f)
                        {
                            g.setColour (VC::Panel.brighter (0.16f));
                            for (float x = rr.getRight(); x > rr.getX(); x -= colsPerBeat)
                                g.drawLine (x, rr.getY(), x, rr.getBottom(), 0.6f);
                        }
                    }
                }
                T18Draw::label (g, left.reduced (3.0f), "Echoes vs beat");
                if (d->getFeedbackLevel() > 1.0f)
                {
                    g.setColour (juce::Colour (0xffff9100));
                    g.setFont (9.0f);
                    g.drawText ("BUILDING", left.reduced (4.0f),
                                juce::Justification::topRight, false);
                }

                // The curve that left the panel at T20 (CL-299): input vertical,
                // output horizontal, matching the reference orientation.
                auto rr = right.reduced (2.0f);
                T18Draw::grid (g, rr, 4);
                juce::Path curve;
                constexpr int kPts = 96;
                for (int i = 0; i < kPts; ++i)
                {
                    const float in  = -1.0f + 2.0f * (float) i / (float) (kPts - 1);
                    const float out = juce::jlimit (-1.2f, 1.2f, d->shapeFeedbackForDisplay (in));
                    const float px  = rr.getCentreX() + out * rr.getWidth()  * 0.5f / 1.2f;
                    const float py  = rr.getCentreY() - in  * rr.getHeight() * 0.5f;
                    if (i == 0) curve.startNewSubPath (px, py);
                    else        curve.lineTo (px, py);
                }
                g.setColour (juce::Colour (0xff00fff2));
                g.strokePath (curve, juce::PathStrokeType (1.4f));
                T18Draw::label (g, rr.reduced (1.0f), "Drive");
            });
        }
        // ── T18: Reverb (audio-first, Jeff 2026-08-06) ───────────────────────
        // Main = the sound: the grey outline stops when you stop playing and
        // the solid shape keeps going -- THAT hang-over is the reverb.  Side =
        // the decay map explaining what the tail is made of.
        else if (mBoundType == EffectType::Reverb)
        {
            mStrip->setCaption (fxName + " - solid keeps going after the grey stops: that is the tail");
            mStrip->setPaintFn ([this] (juce::Graphics& g, juce::Rectangle<float> area,
                                        EffectVisualFeed* feed)
            {
                auto* r = dynamic_cast<ReverbDSP*> (resolveDsp());
                if (r == nullptr) return;

                auto side = area.removeFromRight (area.getWidth() * 0.38f);
                T18Draw::audioInOut (g, area, feed, juce::Colour (0xff00fff2));
                T18Draw::label (g, area.reduced (3.0f), "Audio");

                T18Draw::reverbEnvelope (g, side, r->getPreDelayMs(), r->getDecay(),
                                         r->getER(), r->getDiffusion());
            });
        }
        // ── T18: Compressor (audio-first, Jeff 2026-08-06) ───────────────────
        // Main = the input the compressor is reacting to, with the threshold
        // drawn ON it and the gain reduction hanging from the top -- which is
        // where Attack and Release finally SHOW: attack is how fast the cyan
        // digs in after the audio crosses the orange lines, release is how long
        // it hangs on after the audio falls back.  (They never bend the knee --
        // that curve is the level map; they are the speed of moving along it.)
        // Side = the knee curve with the live operating dot.
        else if (mBoundType == EffectType::Compressor)
        {
            mStrip->setCaption (fxName + " - cyan from top = volume pulled down; orange = threshold");
            mStrip->setPaintFn ([this] (juce::Graphics& g, juce::Rectangle<float> area,
                                        EffectVisualFeed* feed)
            {
                auto* c = dynamic_cast<CompressorDSP*> (resolveDsp());
                if (c == nullptr) return;

                auto side = area.removeFromRight (area.getWidth() * 0.34f);

                // Input waveform (hi/lo IS the input for this effect's columns).
                auto rr = area.reduced (2.0f);
                T18Draw::grid (g, rr, 8);
                if (feed != nullptr && rr.getWidth() >= 2.0f)
                {
                    const int      cols  = juce::jmin ((int) rr.getWidth(), EffectVisualFeed::kSize);
                    const uint32_t write = feed->writeIndex();
                    const float    midY  = rr.getCentreY();
                    const float    halfH = rr.getHeight() * 0.5f;

                    for (int i = 0; i < cols; ++i)
                    {
                        const auto& col = feed->at (write - (uint32_t) (cols - i));
                        const float x = rr.getX() + (float) i;
                        const float e = juce::jlimit (0.0f, 1.0f, col.hi);
                        if (e > 0.002f)
                        {
                            g.setColour (VC::TextDim.withAlpha (0.8f));
                            g.drawLine (x, midY - e * halfH * 0.94f,
                                        x, midY + e * halfH * 0.94f, 1.0f);
                        }
                        // GR hanging from the top, exactly like the limiter.
                        const float gr = juce::jlimit (0.0f, 1.0f, col.a);
                        if (gr > 0.005f)
                        {
                            g.setColour (juce::Colour (0xff00fff2));
                            g.drawLine (x, rr.getY(), x, rr.getY() + gr * rr.getHeight() * 0.5f, 1.0f);
                        }
                    }

                    // Threshold: +/- lines on the same linear scale as the
                    // waveform, from the newest column so a knob turn moves it.
                    const float thr = juce::jlimit (0.0f, 1.0f, feed->at (write - 1).b);
                    if (thr > 0.001f)
                    {
                        g.setColour (juce::Colour (0xffff9100).withAlpha (0.7f));
                        g.drawLine (rr.getX(), midY - thr * halfH * 0.94f,
                                    rr.getRight(), midY - thr * halfH * 0.94f, 0.8f);
                        g.drawLine (rr.getX(), midY + thr * halfH * 0.94f,
                                    rr.getRight(), midY + thr * halfH * 0.94f, 0.8f);
                    }
                }
                T18Draw::label (g, rr.reduced (1.0f), "Input + reduction");

                T18Draw::compCurve (g, side, c->threshold, c->ratio, c->kneeDb,
                                    -c->getGainReductionDb());
                T18Draw::label (g, side.reduced (3.0f), "Knee");
            });
        }
        // ── T18: Transient Shaper (audio-first, Jeff 2026-08-06) ─────────────
        // Main = the real audio, ghost in vs solid out -- the actual hits
        // getting sharpened or softened as they pass.  Side = the idealized
        // one-hit preview, which still earns its place because it shows what
        // the settings WILL do while nothing is playing.
        else if (mBoundType == EffectType::TransientShaper)
        {
            mStrip->setCaption (fxName + " - grey outline = in, solid = shaped out | one-hit preview");
            mStrip->setPaintFn ([this] (juce::Graphics& g, juce::Rectangle<float> area,
                                        EffectVisualFeed* feed)
            {
                auto* t = dynamic_cast<TransientShaperDSP*> (resolveDsp());
                if (t == nullptr) return;

                auto side = area.removeFromRight (area.getWidth() * 0.34f);
                T18Draw::audioInOut (g, area, feed, juce::Colour (0xff00fff2));
                T18Draw::label (g, area.reduced (3.0f), "Audio");

                T18Draw::transientEnvelope (g, side, t->mAttack, t->mRelease,
                                            t->mAttackShape, t->mReleaseShape);
                T18Draw::label (g, side.reduced (3.0f), "Preview");
            });
        }
        // ── T18: Saturation / Tape (one DSP -- Tape is an alias type) ────────
        else if (mBoundType == EffectType::Saturation || mBoundType == EffectType::Tape)
        {
            // Audio-first (Jeff, 2026-08-06): main = the signal thickening as it
            // passes; the bars are measured AT the level the audio is actually
            // hitting the shaper, so they rise and fall with the sound.
            mStrip->setCaption (fxName + " - grey = in, solid = out | bright bars = harmonics being added NOW");
            mStrip->setPaintFn ([this] (juce::Graphics& g, juce::Rectangle<float> area,
                                        EffectVisualFeed* feed)
            {
                auto* s = dynamic_cast<SaturationDSP*> (resolveDsp());
                if (s == nullptr) return;

                auto audio = area.removeFromLeft (area.getWidth() * 0.45f);
                auto left  = area.removeFromLeft (area.getWidth() * 0.55f);
                auto right = area;

                T18Draw::audioInOut (g, audio, feed, juce::Colour (0xff00fff2));
                T18Draw::label (g, audio.reduced (3.0f), "Audio");

                T18Draw::harmonicBars (g, left, [s] (float x) { return s->shapeForDisplay (x); },
                                       T18Draw::recentInputLevel (feed));
                T18Draw::label (g, left.reduced (3.0f), "Harmonics");

                auto rr = right.reduced (2.0f);
                T18Draw::grid (g, rr, 4);
                juce::Path curve;
                constexpr int kPts = 96;
                for (int i = 0; i < kPts; ++i)
                {
                    const float in  = -1.0f + 2.0f * (float) i / (float) (kPts - 1);
                    const float out = juce::jlimit (-1.2f, 1.2f, s->shapeForDisplay (in));
                    const float px  = rr.getX() + (in + 1.0f) * 0.5f * rr.getWidth();
                    const float py  = rr.getCentreY() - out * rr.getHeight() * 0.5f / 1.2f;
                    if (i == 0) curve.startNewSubPath (px, py);
                    else        curve.lineTo (px, py);
                }
                g.setColour (juce::Colour (0xff00fff2));
                g.strokePath (curve, juce::PathStrokeType (1.4f));
                T18Draw::label (g, rr.reduced (1.0f), "Curve");
            });
        }
        else
        {
            mStrip->setPaintFn (nullptr);
        }
    }

    const juce::String strip = mResolveName ? mResolveName (mChannelId) : juce::String();
    const juce::String what  = fxName.isNotEmpty() ? fxName : juce::String ("Effect");
    const juce::String t = (strip.isEmpty() ? what : strip + " - " + what) + " Visual";
    if (t != mTitle)
    {
        mTitle = t;
        if (onTitleChanged) onTitleChanged (mTitle);
    }
}

void EffectVisualWindow::timerCallback() { rebind(); }
