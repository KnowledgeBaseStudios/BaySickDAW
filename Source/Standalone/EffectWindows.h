#pragma once
#include <JuceHeader.h>
#include "../EffectRack.h"
#include "../DSP/EQ8MsDSP.h"
#include "SlotComponent.h"
#include "SharedUI.h"
#include "UndoActions.h"

// Forward-declared rather than including the hosting header: only the bridge
// toggle needs the type, and the definition is available in the .cpp.
namespace Hosting { class HostedPluginInstance; }

class VibeSynthProcessor;

// ── The Effects shell's satellite windows — QA-ModelShell TS5 (2026-07-29) ────
// The Effects surface is no longer one page that stacks everything.  It is a
// small rack window (EffectsPage) plus these: one window per effect panel, and
// one window per EQ.  Jeff's spec 2026-07-29 -- "a user can choose what they
// are editing at a time instead of everything all at once".
//
// WHY THESE HOLD (channelId, uuid) AND RESOLVE PER TICK RATHER THAN HOLDING
// POINTERS.  A window outlives the rack view's channel selection by design
// (locked: windows stay open when the strip picker changes), and racks live
// inside InsertNodes that die with their tab.  So a captured EffectRack* or
// DSPBase* here would be the exact dangling-target bug the model-side rewrite
// spent TS1-TS3 removing.  Every use re-resolves through
// EffectsPage::rackForChannelId, and a resolve that fails means the target is
// genuinely gone -- which is also how these windows know to close themselves.
// ─────────────────────────────────────────────────────────────────────────────

// ── EffectSlotWindow ─────────────────────────────────────────────────────────
// Content of one per-effect window.  Hosts a SlotComponent in PanelOnly
// presentation, so the panel, its meters, its undo wiring and its menus are the
// shipped ones; what changes is where the chrome lives (the hosting window's
// title strip) and that no header strip is drawn.
class EffectSlotWindow : public juce::Component,
                         private juce::Timer
{
public:
    EffectSlotWindow (VibeSynthProcessor& proc,
                      int channelId,
                      juce::String slotUuid,
                      std::function<juce::String(int)> resolveChannelName,
                      UndoContext undo);
    ~EffectSlotWindow() override;

    // Installs the bypass LED + the Basic / Mode / SC / Presets menu on the
    // hosting window's title strip (locked call 3a: all four in the menu).
    void configureTitleStrip (PageMenuBar& bar);

    // "<Strip> - <Effect>", e.g. "Layer 1 - Compressor".  Recomputed on the
    // poll so a strip rename reaches the window title.
    juce::String windowTitle() const;

    // The slot this window is showing is gone (cleared, rack destroyed, project
    // closed).  The owner destroys the window in response, so nothing may touch
    // this object after it fires.
    std::function<void()> onRequestClose;
    // Title text changed (strip renamed, effect swapped by a preset load).
    std::function<void(const juce::String&)> onTitleChanged;

    void resized() override;
    // Peer-keyed poll, matching every other repeating UI cost in the shell: a
    // window that is not on screen must not keep polling.
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;
    void buildPanel();
    void refreshChrome();
    // Slot index for our uuid, or -1.  Index is NEVER cached: removal packs the
    // slots up and reorder swaps them, both of which move an effect between
    // indices while its uuid travels with it.
    int  resolveSlot (EffectRack*& outRack) const;

    VibeSynthProcessor&              mProc;
    const int                        mChannelId;
    const juce::String               mUuid;
    std::function<juce::String(int)> mResolveChannelName;
    UndoContext                      mUndo;

    // Null unless this slot holds a hosted plugin.  Drives the bridge toggle on
    // the title-strip menu (BLU-302).
    Hosting::HostedPluginInstance*   hostedPluginForSlot() const;

    std::unique_ptr<SlotComponent>   mSlot;
    BypassLedButton                  mLed;      // injected into the title strip
    // SafePointer, NOT a raw pointer.  WorkspaceWindow declares mContent BEFORE
    // mPageMenu, and members destruct in REVERSE declaration order -- so the
    // title strip's menu bar is already gone by the time this window (the
    // content) is destroyed.  A raw pointer here read freed memory in
    // ~EffectSlotWindow and took the app down when a plugin window was closed.
    // A SafePointer simply reads null, which is the correct outcome: the bar is
    // being destroyed anyway, so there is nothing left to unhook from it.
    juce::Component::SafePointer<PageMenuBar> mBar;

    // What the mounted panel was built for.  A Mode switch changes the DSP's
    // variant without changing its EffectType, and the EffectParamMap key is
    // (type, variant) -- so the panel AND the automation registration have to
    // follow the variant, not just the type.
    EffectRack*                      mBuiltRack    { nullptr };
    EffectType                       mBuiltType    { EffectType::None };
    int                              mBuiltVariant { -1 };
    juce::String                     mTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSlotWindow)
};

// ── EffectEqWindow ───────────────────────────────────────────────────────────
// Content of one EQ window.  Fixed to Pre or Post for its whole life (Jeff:
// "each window is just the pre or the post, not a swappable window for both");
// the title strip carries a two-tab strip whose other entry OPENS the other
// window rather than swapping this one's contents.
class EffectEqWindow : public juce::Component,
                       private juce::Timer
{
public:
    EffectEqWindow (VibeSynthProcessor& proc,
                    int channelId,
                    bool isPre,
                    std::function<juce::String(int)> resolveChannelName);
    ~EffectEqWindow() override;

    void configureTitleStrip (PageMenuBar& bar);
    juce::String windowTitle() const;

    bool isPre() const noexcept { return mIsPre; }

    // Asks the owner to open (or bring forward) the sibling EQ window for the
    // same strip.
    std::function<void(bool wantPre)> onOpenOtherEq;
    std::function<void()>             onRequestClose;
    std::function<void(const juce::String&)> onTitleChanged;

    void resized() override;
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;
    void bindToChannel();
    // Resolved per call, never cached across ticks -- the node can be rebuilt
    // under the window (strip respawn, graph rebuild) and a stale pointer here
    // would mean editing an EQ nothing is listening to.
    EQ8MsDSP* resolveEq() const;

    VibeSynthProcessor&              mProc;
    const int                        mChannelId;
    const bool                       mIsPre;
    std::function<juce::String(int)> mResolveChannelName;

    std::unique_ptr<ParametricEQDisplay> mDisplay;
    // Display-only fallback so the curve still draws before the graph has built
    // this channel's node (the same role EffectsPage's owned DSPs played).
    EQ8MsDSP                         mFallbackEq;
    // What the display is currently bound to, so the poll can notice a rebuild.
    EQ8MsDSP*                        mBoundEq { nullptr };
    // Distinguishes "channel not built yet" (fallback draws) from "channel
    // DIED" (close the window).
    bool                             mEverResolved { false };
    // SafePointer for the same destruction-order reason as EffectSlotWindow's.
    juce::Component::SafePointer<PageMenuBar> mBar;
    bool                             mShowMid { true };
    juce::String                     mTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectEqWindow)
};
