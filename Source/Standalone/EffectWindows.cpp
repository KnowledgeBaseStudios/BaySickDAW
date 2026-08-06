#include "EffectWindows.h"
#include "EffectsPage.h"
#include "EffectEditorPanels.h"
#include "../PluginProcessor.h"
#include "../DSP/EffectParamMap.h"
#include "../Hosting/HostedPluginEffect.h"   // QA-ModelShell TS6: window fit + bridge toggle
#include "WorkspaceWindow.h"

// ═════════════════════════════════════════════════════════════════ EffectSlotWindow

EffectSlotWindow::EffectSlotWindow (VibeSynthProcessor& proc,
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
        hosted->onNaturalSizeChanged = [this, hosted] (int w, int h)
        {
            auto* win = findParentComponentOfClass<WorkspaceWindow>();
            if (win == nullptr) return;

            win->sizeToContent (w, h);

            // QA-Layout T12: the surface scales, so the window may go below the
            // plugin's natural size -- but only as far as the UI stays usable.
            // A bridged surface cannot scale at all, so its floor IS its natural
            // size; shrinking further would only clip it.
            const float floorScale = hosted->canScaleSurface()
                                       ? Hosting::HostedPluginEditor::kMinUsableScale
                                       : 1.0f;
            win->setResizeFloor ((int) ((float) w * floorScale),
                                 (int) ((float) h * floorScale));
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

EffectEqWindow::EffectEqWindow (VibeSynthProcessor& proc,
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

// ═══════════════════════════════════════════════════════════ EffectVisualWindow

EffectVisualWindow::EffectVisualWindow (VibeSynthProcessor& proc,
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
        if (mBoundType == EffectType::Limiter)
        {
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
