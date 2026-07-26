#include "MixerPage.h"
#include <set>   // D.3: setStripOrder uses std::set for dedup

// ─────────────────────────────────────────────────────────────────────────────
// Direct Routing label - vertical-text panel between Master and FX Bus group,
// shown only when strips are routed directly to Master. Paint-only; never
// intercepts clicks so cables can still drag through.
// ─────────────────────────────────────────────────────────────────────────────
struct DirectRoutingLabel : public juce::Component
{
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff14141a));
        g.fillRect(b);
        g.setColour(VC::Accent.withAlpha(0.45f));
        g.drawRect(b, 1.f);

        // Vertical "DIRECT ROUTING" text (rotated 90° counter-clockwise)
        g.setColour(VC::TextDim);
        g.setFont(juce::Font(10.f, juce::Font::bold));
        juce::AffineTransform tr;
        tr = tr.rotated(-juce::MathConstants<float>::halfPi,
                        b.getCentreX(), b.getCentreY());
        g.addTransform(tr);
        g.drawText("DIRECT ROUTING",
                   b.withSizeKeepingCentre(b.getHeight() - 6.f, 14.f),
                   juce::Justification::centred, false);
    }
};

// Tab accent colors mirrored on the mixer so bus colors match the ribbon tab
// colors - helps the user find "where to do things".
static constexpr juce::uint32 kMixerTabPurple = 0xff7b2fbe;  // Master strip
static constexpr juce::uint32 kEffectsTabPink = 0xffce3f8e;  // FX Bus + aux strips

// QA-Eg: send slot fan-out offsets - first send anchored at the socket (0),
// additional sends fan out symmetrically so a single-send strip's cable lands
// EXACTLY at the socket dot.  File-scope so both CableOverlay::paint and
// CableOverlay::hitTestCablesAll apply the same offset (paint draws sends at
// src.x + offset; hit-test must match or sends 2/3/4 hit zones miss).
static constexpr int kSendOffsets[4] = { 0, 8, -8, 16 };

// QA-Eg fix-up (2026-05-24): Master cable cutout rectangle dimensions.  Tuned
// against the live mixer build via a transient "Tune Master Cutout"
// calibration UI (since removed); values baked in here as compile-time
// constants.  The cutout is a rectangle centered on Master's destination-
// socket position with W * H px size + XOff / YOff px shift, used by
// CableOverlay::paint Phase B + hitTestCablesAll to clip the visible
// return-cable bundle that lands at Master's input.  See
// `Documents/BaySickDAW/mixer_master_cutout.txt` (if you still have the log)
// for the tuning provenance.
static constexpr float kMasterCutoutW    = 5.0f;
static constexpr float kMasterCutoutH    = 14.0f;
static constexpr float kMasterCutoutXOff = 0.5f;
static constexpr float kMasterCutoutYOff = 5.0f;

// Accent-color resolver used by layoutScrollContent - maps (channelId, current
// main-out destination) to the strip's top-bar accent color.
static juce::Colour pickStripColor(int chId, int destChannelId)
{
    using namespace MixerChannelIds;
    // Aux: always Effects-tab pink
    if (chId >= kAuxBase && chId < kAuxBase + 16) return juce::Colour(kEffectsTabPink);
    // Colored bus groups - track the main-out destination
    if (destChannelId == kLayersBus) return VC::LayerCol[0];
    if (destChannelId == kBassBus)   return VC::BassCol[0];
    if (destChannelId == kDrumsBus)  return VC::DrumsCol;
    if (destChannelId == kClipsBus)  return VC::Warm;
    if (destChannelId == kFxBus)     return juce::Colour(kEffectsTabPink);
    // 2026-04-30: Vox + Inst destination buses got teal + navy mirrors of
    // the matching ribbon tabs.  Was missing - Vox/Inst insert strips
    // routed to their natural bus fell through to VC::Accent (grey-blue),
    // so the top accent stripe was effectively invisible vs the bus's
    // bright neon divider.  These two lines + the chId fallbacks below
    // unify the appearance so insert strip top stripe matches the bus
    // group neon divider.
    if (destChannelId == kVoxBus || destChannelId == kVoxBus2)   return juce::Colour(0xFF0FAFA5);
    if (destChannelId == kInstBus || destChannelId == kInstBus2
                                  || destChannelId == kInstBus3) return juce::Colour(0xFF1C3A8A);
    // J-5 (2026-05-03): RustyDrums Bus strips share the Drums-red accent so
    // the top stripe matches the bus group neon divider.
    if (destChannelId == kRustyDrumsBus) return VC::DrumsCol;
    // Direct Routing / aux chain: fall back to the strip's natural color
    if (chId >= kLayerBase && chId < kLayerBase + 16) return VC::LayerCol[0];
    if (chId >= kBassBase  && chId < kBassBase  + 16) return VC::BassCol[0];
    if (chId >= kDrumBase  && chId < kDrumBase  + 16) return VC::DrumsCol;
    if (chId >= kAudioBase && chId < kAudioBase + 50) return VC::Warm;
    if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)  return juce::Colour(0xFF0FAFA5);
    if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips) return juce::Colour(0xFF1C3A8A);
    if (chId >= kRustyBase && chId < kRustyBase + kMaxRustyStrips) return VC::DrumsCol;
    return VC::Accent;
}

// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::ScrollContent::paint(juce::Graphics& g)
{
    g.fillAll(VC::Bg);
}

void MixerPage::ScrollContent::paintOverChildren(juce::Graphics& g)
{
    for (const auto& line : mNeonLines)
    {
        const juce::Colour c = line.color;
        const float x  = (float)line.x;
        const float y0 = (float)line.yStart;
        const float y1 = (float)line.yEnd;

        if (line.bright)
        {
            // Bus → first-member: 3-layer glow + solid 2 px core
            g.setColour(c.withAlpha(0.12f)); g.drawLine(x, y0, x, y1, 8.f);
            g.setColour(c.withAlpha(0.35f)); g.drawLine(x, y0, x, y1, 4.f);
            g.setColour(c.withAlpha(1.00f)); g.drawLine(x, y0, x, y1, 2.f);
        }
        else
        {
            // Member-to-member: subtle glow + thin 1 px core
            g.setColour(c.withAlpha(0.08f)); g.drawLine(x, y0, x, y1, 5.f);
            g.setColour(c.withAlpha(0.40f)); g.drawLine(x, y0, x, y1, 1.f);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 5F-4b B3: CableOverlay - green bezier cables between strip sockets
// ─────────────────────────────────────────────────────────────────────────────
MixerPage::CableOverlay::CableOverlay(MixerPage& o) : owner(o)
{
    // QA-Eg: setBufferedToImage was REMOVED here.  The cable overlay now
    // repaints every vblank (telemetry-driven cable animation), so caching
    // the bitmap adds overhead AND caused the unison-flicker pattern where
    // every cable strobed in lockstep (all cables share the same cached
    // bitmap; per-frame invalidation thrashed the cache).  Paint directly
    // each frame instead.
    // hitTest() gates click-through: only intercepts near sockets or while dragging.
    setInterceptsMouseClicks(true, false);
}

bool MixerPage::CableOverlay::hitTest(int x, int y)
{
    if (mDragging) return true;
    if (mPendingSendSrcId >= 0) return true;   // B5: intercept everything in send-placement mode
    if (mPendingScSrcId   >= 0) return true;   // C.4 Phase 1: same for SC-placement mode

    auto pt = juce::Point<float>((float) x, (float) y);

    // B6: right-click near a cable should be intercepted
    if (hitTestCable(pt).srcId >= 0) return true;

    return findSocketNear(pt, 14.f, true) >= 0;
}

// QA-Eg: deep dual-stub cable path - 200 px base drop plus 0.4 * horizontal
// distance multiplier.  Bezier control points are at (start.x, start.y +
// tension) and (dest.x, dest.y + tension) so the cable's middle section
// descends well below the visible page area (CableOverlay clips to page
// bounds), producing a patch-bay aesthetic where each end drops down with
// a slight lean toward the other.  Solves the cable-cut-off-behind-Master
// problem naturally without conditional per-cable math.
juce::Path MixerPage::CableOverlay::getMixerCablePath(float startX, float startY,
                                                      float destX,  float destY) const
{
    constexpr float baseStubLength = 200.0f;
    const float distanceMultiplier = std::abs(destX - startX) * 0.4f;
    const float tension = baseStubLength + distanceMultiplier;

    juce::Path path;
    path.startNewSubPath(startX, startY);
    path.cubicTo(startX, startY + tension,
                 destX,  destY  + tension,
                 destX,  destY);
    return path;
}

void MixerPage::CableOverlay::paint(juce::Graphics& g)
{
    // C.4 Phase 1 (2026-04-30): cable color palette per Jeff's spec -
    //   Main cable (locked main-out): green, kept as-is
    //   Send cable: pink (was green-with-alpha)
    //   Sidechain cable: white
    constexpr juce::uint32 kCableMain = 0xff33ff88;   // green
    constexpr juce::uint32 kCableSend = 0xffff33b0;   // hot pink (perceptual parity with the green main)
    constexpr juce::uint32 kCableSc   = 0xffeeeeee;   // white

    // QA-Eg telemetry helper: map source-strip peak dBFS to (alpha, color).
    // Base alpha 0.55 so silent cables stay clearly visible.  dB-normalized
    // mapping (-60..0 dB linear interpolation) so perceived brightness tracks
    // perceived loudness instead of compressing into the top 10 dB.  Warning
    // color lerps toward red when the linear peak crosses 0.95 (~ -0.45 dB).
    auto cableTelemetry = [this](int srcChannelId, juce::Colour baseColor)
        -> juce::Colour
    {
        float peakDb = -60.0f;
        if (auto* srcStrip = owner.findStripByChannelId(srcChannelId))
            peakDb = srcStrip->getCurrentPeakDb();
        const float peakNorm = juce::jlimit(0.0f, 1.0f, (peakDb + 60.0f) / 60.0f);
        const float alpha    = 0.55f + 0.45f * peakNorm;
        const float peakLin  = juce::jlimit(0.0f, 1.0f,
            juce::Decibels::decibelsToGain(peakDb, -60.0f));
        juce::Colour col = baseColor;
        if (peakLin >= 0.95f)
        {
            const float warn = juce::jlimit(0.0f, 1.0f, (peakLin - 0.95f) / 0.05f);
            col = baseColor.interpolatedWith(juce::Colours::red, warn);
        }
        return col.withAlpha(alpha);
    };

    const auto mainStroke = juce::PathStrokeType(2.5f,
        juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded);
    // Sends use a slightly thicker stroke so they read clearly when stacked
    // alongside the main-out cable from the same source.
    const auto sendStroke = juce::PathStrokeType(3.0f,
        juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded);

    const auto& edges   = owner.mProcessor.mVibeGraph.getRoutingGraph().edges();
    const auto& scEdges = owner.mProcessor.mVibeGraph.getRoutingGraph().scEdges();

    // QA-Eg fix-up: Master strip is fixed-position + full-page-height.  Other
    // strips scroll past behind it.  Pre-fix, the cable overlay painted every
    // cable on top of everything — so cables visibly bled across Master's body
    // (sends scrolling behind Master kept showing their cable floating over
    // Master; main-out returns to Master showed correctly but ALL other cable
    // bodies crossing Master also showed wrong).  Fix: clip-exclude Master's
    // bounds when rendering cables, then re-render each Master-terminating
    // cable through a thin carve-out shaped like the cable's own stroked path
    // — so each return cable shows a precise stub inside Master visibly
    // connecting to its socket, while other cables stay hidden behind Master.
    juce::Rectangle<int> masterBounds;
    if (auto* m = owner.mMasterStrip.get())
        masterBounds = getLocalArea(m, m->getLocalBounds());

    const int kMasterCh = MixerChannelIds::kMaster;

    // Phase A: render every cable with Master excluded from the clip.  Cable
    // portions outside Master render normally; portions inside Master are
    // erased entirely (these are the parts we want hidden behind Master).
    {
        juce::Graphics::ScopedSaveState sss(g);
        if (! masterBounds.isEmpty()) g.excludeClipRegion(masterBounds);

        // Pass A1: main-out cables (paint underneath sends).
        for (const auto& e : edges)
        {
            if (! e.isMainOut) continue;
            if (mDragging && e.srcId == mDragSrcId) continue;

            auto src = owner.getSocketPosition(e.srcId);
            auto dst = owner.getSocketPosition(e.dstId);
            if (src.x < 0 || dst.x < 0) continue;

            g.setColour(cableTelemetry(e.srcId, juce::Colour(kCableMain)));
            g.strokePath(getMixerCablePath(src.x, src.y, dst.x, dst.y), mainStroke);
        }

        // Pass A2: send cables (paint on top of mains).
        std::map<int, int> sendSlotIndex;
        for (const auto& e : edges)
        {
            if (e.isMainOut) continue;

            auto src = owner.getSocketPosition(e.srcId);
            auto dst = owner.getSocketPosition(e.dstId);
            if (src.x < 0 || dst.x < 0) continue;

            const int idx = sendSlotIndex[e.srcId]++;
            src.x += (float) kSendOffsets[idx % 4];

            g.setColour(cableTelemetry(e.srcId, juce::Colour(kCableSend)));
            g.strokePath(getMixerCablePath(src.x, src.y, dst.x, dst.y), sendStroke);
        }

        // Pass A3: SC cables (white) - on top of mains + sends.
        for (const auto& sce : scEdges)
        {
            auto src = owner.getSocketPosition(sce.srcId);
            auto dst = owner.getSocketPosition(sce.dstId);
            if (src.x < 0 || dst.x < 0) continue;

            g.setColour(cableTelemetry(sce.srcId, juce::Colour(kCableSc)));
            g.strokePath(getMixerCablePath(src.x, src.y, dst.x, dst.y), mainStroke);
        }
    }

    // Phase B: render cables terminating at Master with the clip narrowed to
    // a small rectangle centered on Master's destination-socket position.
    // The rectangle dimensions (width / height + xOff / yOff) live on
    // MixerPage as tunable members; the "Tune Master Cutout" calibration mode
    // exposes drag/resize handles on the editor rectangle so the size can be
    // dialed in live, then "Save Cutout Layout" writes the values to a log
    // file for me to bake into source defaults.  Path-derived carve-out was
    // tried first but failed when a strip scrolled fully behind Master --
    // its cable's entire path then lived inside Master and the carve-out
    // (which IS the path) exposed it.  A fixed rectangle at the destination
    // socket is scroll-safe because it pins to the socket regardless of
    // where the source has scrolled.
    juce::Rectangle<int> cutoutRect;
    auto masterSocket = owner.getSocketPosition(kMasterCh);

    if (! masterBounds.isEmpty() && masterSocket.x >= 0)
    {
        const float w = juce::jmax(1.0f, kMasterCutoutW);
        const float h = juce::jmax(1.0f, kMasterCutoutH);
        const float cx = masterSocket.x + kMasterCutoutXOff;
        const float cy = masterSocket.y + kMasterCutoutYOff;
        cutoutRect = juce::Rectangle<int>(
            (int) std::floor(cx - w * 0.5f),
            (int) std::floor(cy - h * 0.5f),
            (int) std::ceil(w),
            (int) std::ceil(h));

        auto renderMasterStub = [&] (juce::Point<float> src, juce::Point<float> dst,
                                     juce::Colour col,
                                     const juce::PathStrokeType& stroke)
        {
            // QA-Eg fix-up: only the exact sentinel (-1, -1) returned by
            // getSocketPosition for missing / hidden strips disqualifies a
            // cable.  Real-but-very-negative coordinates from a source strip
            // scrolled off the viewport's left edge are NOT sentinels --
            // those cables still need to render so the cutout shows their
            // full color-depth bundle at Master's socket regardless of where
            // the source has scrolled to.
            const bool srcSentinel = (src.x == -1.f && src.y == -1.f);
            const bool dstSentinel = (dst.x == -1.f && dst.y == -1.f);
            if (srcSentinel || dstSentinel) return;
            juce::Graphics::ScopedSaveState sss(g);
            g.reduceClipRegion(cutoutRect);
            g.setColour(col);
            g.strokePath(getMixerCablePath(src.x, src.y, dst.x, dst.y), stroke);
        };

        // Pass B1: mains terminating at Master.
        for (const auto& e : edges)
        {
            if (! e.isMainOut) continue;
            if (e.dstId != kMasterCh) continue;
            if (mDragging && e.srcId == mDragSrcId) continue;

            auto src = owner.getSocketPosition(e.srcId);
            auto dst = owner.getSocketPosition(e.dstId);
            renderMasterStub(src, dst,
                             cableTelemetry(e.srcId, juce::Colour(kCableMain)),
                             mainStroke);
        }

        // Pass B2: sends terminating at Master.  Counter walks every send in
        // lockstep with Phase A's pass so idx matches the paint-time offset.
        // QA-Eg fix-up: pre-check removed -- renderMasterStub does the
        // sentinel filter itself, and real-but-very-negative src coordinates
        // (source strip scrolled off-screen) still need to render through the
        // cutout.
        std::map<int, int> sendSlotIndexB;
        for (const auto& e : edges)
        {
            if (e.isMainOut) continue;
            const int idx = sendSlotIndexB[e.srcId]++;
            if (e.dstId != kMasterCh) continue;

            auto src = owner.getSocketPosition(e.srcId);
            auto dst = owner.getSocketPosition(e.dstId);

            src.x += (float) kSendOffsets[idx % 4];

            renderMasterStub(src, dst,
                             cableTelemetry(e.srcId, juce::Colour(kCableSend)),
                             sendStroke);
        }

        // Pass B3: SC cables terminating at Master (rare).
        for (const auto& sce : scEdges)
        {
            if (sce.dstId != kMasterCh) continue;

            auto src = owner.getSocketPosition(sce.srcId);
            auto dst = owner.getSocketPosition(sce.dstId);
            renderMasterStub(src, dst,
                             cableTelemetry(sce.srcId, juce::Colour(kCableSc)),
                             mainStroke);
        }
    }


    // Ghost cable while dragging the main-out (no telemetry: no committed source)
    if (mDragging)
    {
        g.setColour(juce::Colour(kCableMain).withAlpha(0.45f));
        g.strokePath(getMixerCablePath(mDragSrcSocket.x, mDragSrcSocket.y,
                                       mDragMousePos.x,  mDragMousePos.y),
                     mainStroke);
    }

    // B5: ghost cable in send-placement mode (follows cursor) - pink.
    if (mPendingSendSrcId >= 0)
    {
        auto srcSock = owner.getSocketPosition(mPendingSendSrcId);
        if (srcSock.x >= 0)
        {
            g.setColour(juce::Colour(kCableSend).withAlpha(0.40f));
            g.strokePath(getMixerCablePath(srcSock.x, srcSock.y,
                                           mDragMousePos.x, mDragMousePos.y),
                         juce::PathStrokeType(2.0f));
        }
    }

    // C.4 Phase 1: ghost cable in SC-placement mode - white.
    if (mPendingScSrcId >= 0)
    {
        auto srcSock = owner.getSocketPosition(mPendingScSrcId);
        if (srcSock.x >= 0)
        {
            g.setColour(juce::Colour(kCableSc).withAlpha(0.45f));
            g.strokePath(getMixerCablePath(srcSock.x, srcSock.y,
                                           mDragMousePos.x, mDragMousePos.y),
                         juce::PathStrokeType(2.0f));
        }
    }

    // Red flash on rejected strip
    if (mFlashStripId >= 0 && mFlashCountdown > 0)
    {
        if (auto* strip = owner.findStripByChannelId(mFlashStripId))
        {
            auto sb = strip->getBoundsInParent();
            juce::Rectangle<float> flashRect;

            if (strip->getParentComponent() == &owner)
            {
                flashRect = sb.toFloat();
            }
            else
            {
                float px = owner.mViewport->getX() + sb.getX() - owner.mViewport->getViewPositionX();
                float py = owner.mViewport->getY() + sb.getY() - owner.mViewport->getViewPositionY();
                flashRect = { px, py, (float)sb.getWidth(), (float)sb.getHeight() };
            }

            g.setColour(juce::Colour(0xffff2020).withAlpha(0.35f));
            g.fillRoundedRectangle(flashRect, 3.f);
        }
    }
}

void MixerPage::CableOverlay::mouseDown(const juce::MouseEvent& e)
{
    // B6: right-click -> show cable property popup.  QA-Eg: when multiple
    // cables sit near the click (e.g., a send + sidechain + main all from
    // the same source going to the same area), pop a chooser menu first so
    // the user can pick which cable's properties to edit.
    if (e.mods.isRightButtonDown())
    {
        auto hits = hitTestCablesAll(e.position);
        if (hits.empty()) return;
        const auto screenPt = e.getScreenPosition().toFloat();
        if (hits.size() == 1)
        {
            // QA-Eg: skip popup for single-hit mains (no editable props).
            if (! hits.front().isMainOut)
                showCablePopup(screenPt, hits.front());
            return;
        }

        juce::PopupMenu chooser;
        for (size_t i = 0; i < hits.size(); ++i)
        {
            const auto& h = hits[i];
            auto* srcStrip = owner.findStripByChannelId(h.srcId);
            auto* dstStrip = owner.findStripByChannelId(h.dstId);
            const juce::String srcName = srcStrip ? srcStrip->getName() : juce::String("?");
            const juce::String dstName = dstStrip ? dstStrip->getName() : juce::String("?");
            juce::String label;
            if (h.isSidechain)
                label = "Sidechain: " + srcName + " -> " + dstName;
            else if (h.isMainOut)
                label = "Main: " + srcName + " -> " + dstName;
            else
                label = "Send " + juce::String(h.sendSlot + 1) + ": "
                      + srcName + " -> " + dstName;
            // QA-Eg: main entries are grayed-out (no popup behind them); they
            // appear in the chooser for visibility only.
            const bool isActive = ! h.isMainOut;
            chooser.addItem((int) i + 1, label, isActive);
        }
        chooser.showMenuAsync(juce::PopupMenu::Options{},
            [this, hits, screenPt](int r)
            {
                if (r > 0 && r <= (int) hits.size()
                    && ! hits[(size_t)(r - 1)].isMainOut)
                    showCablePopup(screenPt, hits[(size_t)(r - 1)]);
            });
        return;
    }

    // B5: send-placement mode - click commits the send
    if (mPendingSendSrcId >= 0)
    {
        int dstId = findStripUnder(e.position);

        if (dstId < 0 || dstId == mPendingSendSrcId)
        {
            cancelSendPlacement();
            return;
        }

        // Routing rule check for sends
        using namespace MixerChannelIds;
        // ALL sends must land on an Aux strip. Sidechain routing is a separate
        // feature (deferred post-5F-9). FX Bus is reachable only via aux-send.
        if (!isValidBusSendTarget(dstId))
        {
            mFlashStripId = dstId; mFlashCountdown = 6; startTimerHz(30);
            cancelSendPlacement();
            return;
        }

        // Cycle check
        if (owner.mProcessor.mVibeGraph.getRoutingGraph().wouldCreateCycle(mPendingSendSrcId, dstId))
        {
            mFlashStripId = dstId; mFlashCountdown = 6; startTimerHz(30);
            cancelSendPlacement();
            return;
        }

        // Find available send slot (0..3)
        const juce::String prefix = prefixFromChannelId(mPendingSendSrcId);
        int slot = findAvailableSendSlot(prefix);
        if (slot < 0)
        {
            // All 4 send slots full - flash the source strip
            mFlashStripId = mPendingSendSrcId; mFlashCountdown = 6; startTimerHz(30);
            cancelSendPlacement();
            return;
        }

        // Commit: write _sendN_to APVTS param
        const juce::String sp = prefix + "_send" + juce::String(slot);
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                owner.mProcessor.apvts.getParameter(sp + "_to")))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)dstId));

        cancelSendPlacement();
        return;
    }

    // C.4 Phase 1 (2026-04-30): sidechain-placement mode - click commits SC
    // on the TARGET strip's _sc_recv{N}_from (target-side encoding so per-
    // module pickers see stable line indices regardless of cable order).
    if (mPendingScSrcId >= 0)
    {
        int dstId = findStripUnder(e.position);

        if (dstId < 0 || dstId == mPendingScSrcId)
        {
            cancelSidechainPlacement();
            return;
        }

        // Cycle check (covers SC + send + main edges combined).
        if (owner.mProcessor.mVibeGraph.getRoutingGraph().wouldCreateCycle(mPendingScSrcId, dstId))
        {
            mFlashStripId = dstId; mFlashCountdown = 6; startTimerHz(30);
            cancelSidechainPlacement();
            return;
        }

        // Find available SC receive slot on the TARGET (0..3).
        const juce::String targetPrefix =
            MixerChannelIds::prefixFromChannelId(dstId);
        int slot = findAvailableScRecvSlot(targetPrefix);
        if (slot < 0)
        {
            // Target's 4 SC receive lines are full - flash the target.
            mFlashStripId = dstId; mFlashCountdown = 6; startTimerHz(30);
            cancelSidechainPlacement();
            return;
        }

        // Commit: write target's _sc_recv{N}_from with source's channel id.
        const juce::String sp = targetPrefix + "_sc_recv" + juce::String(slot);
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                owner.mProcessor.apvts.getParameter(sp + "_from")))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float) mPendingScSrcId));

        cancelSidechainPlacement();
        return;
    }

    // B4: main-out drag - click near a non-locked socket
    int chId = findSocketNear(e.position, 14.f, true);
    if (chId < 0) return;

    mDragging      = true;
    mDragSrcId     = chId;
    mDragSrcSocket = owner.getSocketPosition(chId);
    mDragMousePos  = e.position;
    repaint();
}

void MixerPage::CableOverlay::mouseDrag(const juce::MouseEvent& e)
{
    if (!mDragging) return;
    mDragMousePos = e.position;
    repaint();
}

void MixerPage::CableOverlay::mouseUp(const juce::MouseEvent& e)
{
    if (!mDragging) { return; }
    mDragging = false;

    int dstId = findStripUnder(e.position);

    if (dstId < 0 || dstId == mDragSrcId)
    {
        // Dropped on empty space or self - cancel, cable snaps back
        repaint();
        return;
    }

    // Routing rule check
    if (!isRouteAllowed(mDragSrcId, dstId))
    {
        mFlashStripId   = dstId;
        mFlashCountdown = 6;   // ~200ms at 30fps
        startTimerHz(30);
        repaint();
        return;
    }

    // Cycle check
    if (owner.mProcessor.mVibeGraph.getRoutingGraph().wouldCreateCycle(mDragSrcId, dstId))
    {
        mFlashStripId   = dstId;
        mFlashCountdown = 6;
        startTimerHz(30);
        repaint();
        return;
    }

    // Commit: write _sendTo APVTS param for the source strip
    const juce::String prefix = MixerChannelIds::prefixFromChannelId(mDragSrcId);
    if (prefix.isNotEmpty())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                owner.mProcessor.apvts.getParameter(prefix + "_sendTo")))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)dstId));
    }

    repaint();
}

void MixerPage::CableOverlay::timerCallback()
{
    if (mFlashCountdown > 0)
    {
        --mFlashCountdown;
        repaint();
    }
    else
    {
        mFlashStripId = -1;
        stopTimer();
        repaint();
    }
}

int MixerPage::CableOverlay::findSocketNear(juce::Point<float> pt, float radius,
                                              bool skipLocked) const
{
    // Check Master + 5 buses + all insert maps + aux
    auto check = [&](int chId) -> bool
    {
        if (skipLocked && MixerChannelIds::isMainOutLocked(chId))
            return false;
        auto sock = owner.getSocketPosition(chId);
        return sock.x >= 0 && sock.getDistanceFrom(pt) < radius;
    };

    using namespace MixerChannelIds;
    // Don't offer master/bus sockets for dragging (locked main-out)
    if (!skipLocked)
    {
        if (check(kMaster))    return kMaster;
        if (check(kLayersBus)) return kLayersBus;
        if (check(kBassBus))   return kBassBus;
        if (check(kDrumsBus))  return kDrumsBus;
        if (check(kFxBus))     return kFxBus;
        if (check(kClipsBus))  return kClipsBus;
        // G-6 (2026-04-29): Vox + Inst buses (primary + secondary) were
        // missing here pre-G-6.  Without them, dragging a cable into a Vox/
        // Inst bus did nothing.  Now fully supported as drop sockets.
        if (check(kVoxBus))    return kVoxBus;
        if (check(kInstBus))   return kInstBus;
        if (owner.isVoxBus2Active()  && check(kVoxBus2))   return kVoxBus2;
        if (owner.isInstBus2Active() && check(kInstBus2))  return kInstBus2;
        if (owner.isInstBus3Active() && check(kInstBus3))  return kInstBus3;
        // J-5 (2026-05-03): RustyDrums Bus is a valid drop socket whenever
        // the singleton has spawned its strips (mRustyDrumsBusActive flag).
        if (owner.mRustyDrumsBusActive && check(kRustyDrumsBus)) return kRustyDrumsBus;
    }

    for (auto& [tabId, strip] : owner.mLayerStrips)
        if (check(layerInsert(tabId))) return layerInsert(tabId);
    for (auto& [tabId, strip] : owner.mBassStrips)
        if (check(bassInsert(tabId))) return bassInsert(tabId);
    for (auto& [slot, strip] : owner.mDrumStrips)
        if (check(drumInsert(slot))) return drumInsert(slot);
    for (auto& [row, strip] : owner.mAudioStrips)
        if (check(audioInsert(row))) return audioInsert(row);
    for (auto& [idx, strip] : owner.mAuxStrips)
        if (check(auxStrip(idx))) return auxStrip(idx);
    // R1 (2026-04-23)
    for (auto& [idx, strip] : owner.mVoxStrips)
        if (check(voxInsert(idx))) return voxInsert(idx);
    for (auto& [idx, strip] : owner.mInstStrips)
        if (check(instInsert(idx))) return instInsert(idx);
    // J-5 (2026-05-03)
    for (auto& [idx, strip] : owner.mRustyStrips)
        if (check(rustyInsert(idx))) return rustyInsert(idx);

    return -1;
}

int MixerPage::CableOverlay::findStripUnder(juce::Point<float> pt) const
{
    // Check all strips - is pt within their page-coords bounds?
    auto checkBounds = [&](int chId) -> bool
    {
        auto* strip = owner.findStripByChannelId(chId);
        if (!strip || !strip->isVisible()) return false;

        auto sb = strip->getBoundsInParent();
        juce::Rectangle<float> rect;

        if (strip->getParentComponent() == &owner)
        {
            rect = sb.toFloat();
        }
        else
        {
            float px = owner.mViewport->getX() + sb.getX() - owner.mViewport->getViewPositionX();
            float py = owner.mViewport->getY() + sb.getY() - owner.mViewport->getViewPositionY();
            rect = { px, py, (float)sb.getWidth(), (float)sb.getHeight() };
        }

        return rect.contains(pt);
    };

    using namespace MixerChannelIds;
    if (checkBounds(kMaster))    return kMaster;
    if (checkBounds(kLayersBus)) return kLayersBus;
    if (checkBounds(kBassBus))   return kBassBus;
    if (checkBounds(kDrumsBus))  return kDrumsBus;
    if (checkBounds(kFxBus))     return kFxBus;
    if (checkBounds(kClipsBus))  return kClipsBus;
    // G-6 (2026-04-29): Vox + Inst buses (primary + secondary).  Pre-G-6
    // these weren't in this list which is why cable-drag-to-bus didn't work.
    if (checkBounds(kVoxBus))    return kVoxBus;
    if (checkBounds(kInstBus))   return kInstBus;
    if (owner.isVoxBus2Active()  && checkBounds(kVoxBus2))   return kVoxBus2;
    if (owner.isInstBus2Active() && checkBounds(kInstBus2))  return kInstBus2;
    if (owner.isInstBus3Active() && checkBounds(kInstBus3))  return kInstBus3;
    if (owner.mRustyDrumsBusActive && checkBounds(kRustyDrumsBus)) return kRustyDrumsBus;

    for (auto& [tabId, s] : owner.mLayerStrips)
        if (checkBounds(layerInsert(tabId))) return layerInsert(tabId);
    for (auto& [tabId, s] : owner.mBassStrips)
        if (checkBounds(bassInsert(tabId))) return bassInsert(tabId);
    for (auto& [slot, s] : owner.mDrumStrips)
        if (checkBounds(drumInsert(slot))) return drumInsert(slot);
    for (auto& [row, s] : owner.mAudioStrips)
        if (checkBounds(audioInsert(row))) return audioInsert(row);
    for (auto& [idx, s] : owner.mAuxStrips)
        if (checkBounds(auxStrip(idx))) return auxStrip(idx);
    // R1 (2026-04-23)
    for (auto& [idx, s] : owner.mVoxStrips)
        if (checkBounds(voxInsert(idx))) return voxInsert(idx);
    for (auto& [idx, s] : owner.mInstStrips)
        if (checkBounds(instInsert(idx))) return instInsert(idx);
    // J-5 (2026-05-03)
    for (auto& [idx, s] : owner.mRustyStrips)
        if (checkBounds(rustyInsert(idx))) return rustyInsert(idx);

    return -1;
}

bool MixerPage::CableOverlay::isRouteAllowed(int srcId, int dstId) const
{
    using namespace MixerChannelIds;

    // Self-route / terminal output
    if (srcId == dstId)        return false;
    if (dstId == kOutput)      return false;

    // Bus/Master main-out is locked (drag never starts); defensive check
    if (isMainOutLocked(srcId)) return false;

    const bool srcIsLayer  = (srcId >= kLayerBase && srcId < kLayerBase + 16);
    const bool srcIsBass   = (srcId >= kBassBase  && srcId < kBassBase  + 16);
    const bool srcIsDrum   = (srcId >= kDrumBase  && srcId < kDrumBase  + 16);
    const bool srcIsAudio  = (srcId >= kAudioBase && srcId < kAudioBase + 50);
    const bool srcIsAux    = (srcId >= kAuxBase   && srcId < kAuxBase   + 16);
    const bool srcIsVox    = (srcId >= kVoxBase   && srcId < kVoxBase   + kMaxVoxStrips);
    const bool srcIsInst   = (srcId >= kInstBase  && srcId < kInstBase  + kMaxInstStrips);
    const bool srcIsRusty  = (srcId >= kRustyBase && srcId < kRustyBase + kMaxRustyStrips);

    const bool dstIsMaster = (dstId == kMaster);
    const bool dstIsAux    = (dstId >= kAuxBase && dstId < kAuxBase + 16);

    // Layer insert: Layers Bus · Bass Bus · Master
    if (srcIsLayer)
        return dstIsMaster || dstId == kLayersBus || dstId == kBassBus;

    // Bass insert: Bass Bus · Layers Bus · Master
    if (srcIsBass)
        return dstIsMaster || dstId == kBassBus || dstId == kLayersBus;

    // Drum insert: Drums Bus · Master
    if (srcIsDrum)
        return dstIsMaster || dstId == kDrumsBus;

    // J-5 (2026-05-03): Rusty insert main-out is LOCKED to kRustyDrumsBus
    // (enforced via isMainOutLocked).  This rule covers send cables only -
    // sends are restricted to aux strips per spec, no inter-bus routing.
    if (srcIsRusty)
        return dstIsAux;

    // Audio insert: any bus EXCEPT FX · Master (FX reachable only via aux-send).
    // R1 (2026-04-23): added Vox + Inst bus destinations.
    // G-6 (2026-04-29): secondary Vox/Inst buses also valid destinations.
    if (srcIsAudio)
        return dstIsMaster || dstId == kLayersBus || dstId == kBassBus
            || dstId == kDrumsBus || dstId == kClipsBus
            || dstId == kVoxBus  || dstId == kInstBus
            || dstId == kVoxBus2 || dstId == kInstBus2 || dstId == kInstBus3;

    // Aux strip: Master · FX Bus · other Aux
    if (srcIsAux)
        return dstIsMaster || dstId == kFxBus || dstIsAux;

    // R1 (2026-04-23): Vox / Inst strips - main-out limited to Master, Clips
    // Bus, and their own bus family.  Other instrument buses are explicitly
    // excluded - live-input goes to live-input destinations.
    // G-6 (2026-04-29): Vox can ALSO route to kVoxBus2; Inst to kInstBus2/3.
    // Activation state checked at the source so cables to inactive buses
    // don't open invisible routing - owner is referenced via the const ref
    // in the surrounding CableOverlay class.
    if (srcIsVox)  return dstIsMaster || dstId == kClipsBus
                       || dstId == kVoxBus
                       || (dstId == kVoxBus2 && owner.isVoxBus2Active());
    if (srcIsInst) return dstIsMaster || dstId == kClipsBus
                       || dstId == kInstBus
                       || (dstId == kInstBus2 && owner.isInstBus2Active())
                       || (dstId == kInstBus3 && owner.isInstBus3Active());

    // Unknown strip type - be safe
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// 5F-4b B5: send-placement mode
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::CableOverlay::mouseMove(const juce::MouseEvent& e)
{
    if (mPendingSendSrcId >= 0 || mPendingScSrcId >= 0)
    {
        mDragMousePos = e.position;
        repaint();
    }
}

bool MixerPage::CableOverlay::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && mPendingScSrcId >= 0)
    {
        cancelSidechainPlacement();
        return true;
    }
    if (k == juce::KeyPress::escapeKey && mPendingSendSrcId >= 0)
    {
        cancelSendPlacement();
        return true;
    }
    return false;
}

void MixerPage::CableOverlay::startSendPlacement(int srcChannelId)
{
    // If no aux strip exists yet, auto-create one so the user has a valid
    // landing target. Sends can only land on aux strips.
    if (owner.mAuxStrips.empty())
    {
        owner.addAuxChannel();
        owner.layoutScrollContent();
    }

    mPendingSendSrcId = srcChannelId;
    mPendingScSrcId   = -1;   // C.4 Phase 1: mutually exclusive with SC mode
    mDragMousePos     = owner.getSocketPosition(srcChannelId);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    grabKeyboardFocus();   // so Escape works
    repaint();
}

void MixerPage::CableOverlay::cancelSendPlacement()
{
    mPendingSendSrcId = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

int MixerPage::CableOverlay::findAvailableSendSlot(const juce::String& prefix) const
{
    for (int s = 0; s < 4; ++s)
    {
        const juce::String paramId = prefix + "_send" + juce::String(s) + "_to";
        if (auto* p = owner.mProcessor.apvts.getRawParameterValue(paramId))
            if ((int) p->load() < 0)
                return s;
    }
    return -1;   // all 4 slots occupied
}

// C.4 Phase 1 (2026-04-30): start/end SC-placement mode + target-side slot
// finder.  Mirrors send placement but writes to TARGET's _sc_recv{N}_from
// instead of SOURCE's _send{N}_to.
void MixerPage::CableOverlay::startSidechainPlacement(int srcChannelId)
{
    mPendingScSrcId   = srcChannelId;
    mPendingSendSrcId = -1;            // mutually exclusive
    mDragMousePos     = owner.getSocketPosition(srcChannelId);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    grabKeyboardFocus();
    repaint();
}
void MixerPage::CableOverlay::cancelSidechainPlacement()
{
    mPendingScSrcId = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}
int MixerPage::CableOverlay::findAvailableScRecvSlot(const juce::String& targetPrefix) const
{
    if (targetPrefix.isEmpty()) return -1;
    for (int s = 0; s < 4; ++s)
    {
        const juce::String paramId = targetPrefix + "_sc_recv" + juce::String(s) + "_from";
        if (auto* p = owner.mProcessor.apvts.getRawParameterValue(paramId))
            if ((int) p->load() < 0)
                return s;
    }
    return -1;
}

// C.4 Phase 1 (2026-04-30): per-strip "+" Add-Cable button popup.  Pops a
// 2-item menu (Send / Sidechain) and routes the user's pick into the
// CableOverlay's corresponding placement mode.  Wired from every strip's
// onAddSendRequested lambda so the user gets a consistent "+ -> pick type
// -> click target" flow.
void MixerPage::onAddCableRequestedFor(int srcChannelId)
{
    if (mCableOverlay == nullptr) return;

    juce::PopupMenu m;
    m.addItem(1, "Send...");
    m.addItem(2, "Sidechain...");

    m.showMenuAsync(juce::PopupMenu::Options{},
        [this, srcChannelId](int r)
        {
            if (mCableOverlay == nullptr) return;
            if (r == 1) mCableOverlay->startSendPlacement      (srcChannelId);
            else if (r == 2) mCableOverlay->startSidechainPlacement(srcChannelId);
        });
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4b B6: cable hit-testing + right-click popup
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<MixerPage::CableOverlay::CableHit>
MixerPage::CableOverlay::hitTestCablesAll(juce::Point<float> pt) const
{
    std::vector<CableHit> hits;
    const auto& edges   = owner.mProcessor.mVibeGraph.getRoutingGraph().edges();
    const auto& scEdges = owner.mProcessor.mVibeGraph.getRoutingGraph().scEdges();
    const int kMasterCh = MixerChannelIds::kMaster;

    // QA-Eg fix-up: compute the Master cutout rect (mirrors paint's Phase B
    // computation).  When the click point is inside the cutout, every cable
    // terminating at Master is added to hits regardless of source-strip
    // scroll position -- the visible bundle at Master's socket should always
    // list every routing-to-Master in the right-click chooser, scroll-
    // independent.
    juce::Rectangle<float> masterCutoutRect;
    auto masterSocketHT = owner.getSocketPosition(kMasterCh);
    if (! (masterSocketHT.x == -1.f && masterSocketHT.y == -1.f))
    {
        const float w = juce::jmax(1.0f, kMasterCutoutW);
        const float h = juce::jmax(1.0f, kMasterCutoutH);
        const float cx = masterSocketHT.x + kMasterCutoutXOff;
        const float cy = masterSocketHT.y + kMasterCutoutYOff;
        masterCutoutRect = juce::Rectangle<float>(cx - w * 0.5f, cy - h * 0.5f, w, h);
    }
    const bool inMasterCutout = ! masterCutoutRect.isEmpty()
                              && masterCutoutRect.contains(pt);

    // QA-Eg: hit-test path now matches the rendered path (getMixerCablePath
    // with 200 px tension).  Previously this used a shallow inline bezier
    // that no longer matched what was drawn, so click positions on the
    // visible cables often missed the hit zone entirely.
    auto cableHits = [&pt, this](juce::Point<float> src, juce::Point<float> dst) -> bool
    {
        juce::Path bezier = getMixerCablePath(src.x, src.y, dst.x, dst.y);
        juce::Path hitZone;
        juce::PathStrokeType(10.f).createStrokedPath(hitZone, bezier);
        return hitZone.contains(pt);
    };

    // De-dupe helper for the Master-cutout pass below (avoids adding a cable
    // twice when the bezier-hit path AND the cutout both register it).
    auto alreadyHas = [&hits] (int srcId, int dstId, bool isMainOut, bool isSidechain)
    {
        for (const auto& h : hits)
            if (h.srcId == srcId && h.dstId == dstId
             && h.isMainOut == isMainOut && h.isSidechain == isSidechain)
                return true;
        return false;
    };

    // C.4 Phase 1 (2026-04-30): SC cables collected first so they appear at
    // the TOP of the chooser when sends + SC overlap (matches paint order).
    for (const auto& sce : scEdges)
    {
        auto src = owner.getSocketPosition(sce.srcId);
        auto dst = owner.getSocketPosition(sce.dstId);
        if (src.x < 0 || dst.x < 0) continue;
        if (cableHits(src, dst))
        {
            CableHit hit;
            hit.srcId       = sce.srcId;
            hit.dstId       = sce.dstId;
            hit.scRecvSlot  = sce.dstSlot;
            hit.isSidechain = true;
            hits.push_back(hit);
        }
    }

    // QA-Eg fix-up: split mains pass + sends pass to mirror the paint loop
    // structure.  Sends pass tracks a per-srcId counter to apply the same
    // kSendOffsets fan-out paint applies to src.x -- so the send-cable hit
    // zone tracks the rendered cable's offset (sends 2/3/4 are drawn at
    // src.x + 8 / -8 / +16; pre-fix the hit test used un-offset src.x and
    // missed those cables by up to 16 px).  Sends checked before mains so
    // overlapping cables hit in paint z-order (sends drawn on top of mains).

    // Pass 1: send cables (apply kSendOffsets per-srcId to mirror paint).
    std::map<int, int> sendSlotIndex;
    for (const auto& e : edges)
    {
        if (e.isMainOut) continue;
        auto src = owner.getSocketPosition(e.srcId);
        auto dst = owner.getSocketPosition(e.dstId);
        if (src.x < 0 || dst.x < 0) continue;

        const int idx = sendSlotIndex[e.srcId]++;
        src.x += (float) kSendOffsets[idx % 4];

        if (cableHits(src, dst))
        {
            CableHit hit;
            hit.srcId     = e.srcId;
            hit.dstId     = e.dstId;
            hit.isMainOut = false;
            // Reverse-lookup: which send slot (0..3) has this dstId?
            const juce::String prefix = MixerChannelIds::prefixFromChannelId(e.srcId);
            for (int s = 0; s < 4; ++s)
            {
                const auto paramId = prefix + "_send" + juce::String(s) + "_to";
                if (auto* p = owner.mProcessor.apvts.getRawParameterValue(paramId))
                    if ((int) p->load() == e.dstId) { hit.sendSlot = s; break; }
            }
            hits.push_back(hit);
        }
    }

    // Pass 2: main-out cables (no offset, no slot reverse-lookup needed).
    for (const auto& e : edges)
    {
        if (! e.isMainOut) continue;
        auto src = owner.getSocketPosition(e.srcId);
        auto dst = owner.getSocketPosition(e.dstId);
        if (src.x < 0 || dst.x < 0) continue;
        if (cableHits(src, dst))
        {
            CableHit hit;
            hit.srcId     = e.srcId;
            hit.dstId     = e.dstId;
            hit.isMainOut = true;
            hits.push_back(hit);
        }
    }

    // QA-Eg fix-up: Master cutout pass.  When the click point is inside the
    // cutout rectangle at Master's socket, every cable terminating at Master
    // is added to hits regardless of source-strip scroll position.  De-duped
    // against the bezier-hit passes above so a Master cable whose source is
    // currently on-screen (and would have been added there) doesn't appear
    // twice in the chooser.
    if (inMasterCutout)
    {
        for (const auto& e : edges)
        {
            if (e.dstId != kMasterCh) continue;

            // Skip if source strip doesn't exist at all (stale routing edge).
            auto src = owner.getSocketPosition(e.srcId);
            if (src.x == -1.f && src.y == -1.f) continue;

            if (e.isMainOut)
            {
                if (alreadyHas(e.srcId, e.dstId, /*isMainOut*/true, /*isSidechain*/false))
                    continue;
                CableHit hit;
                hit.srcId     = e.srcId;
                hit.dstId     = e.dstId;
                hit.isMainOut = true;
                hits.push_back(hit);
            }
            else
            {
                if (alreadyHas(e.srcId, e.dstId, /*isMainOut*/false, /*isSidechain*/false))
                    continue;
                const juce::String prefix = MixerChannelIds::prefixFromChannelId(e.srcId);
                int sendSlot = -1;
                for (int s = 0; s < 4; ++s)
                {
                    const auto paramId = prefix + "_send" + juce::String(s) + "_to";
                    if (auto* p = owner.mProcessor.apvts.getRawParameterValue(paramId))
                        if ((int) p->load() == e.dstId) { sendSlot = s; break; }
                }
                CableHit hit;
                hit.srcId     = e.srcId;
                hit.dstId     = e.dstId;
                hit.isMainOut = false;
                hit.sendSlot  = sendSlot;
                hits.push_back(hit);
            }
        }

        for (const auto& sce : scEdges)
        {
            if (sce.dstId != kMasterCh) continue;
            auto src = owner.getSocketPosition(sce.srcId);
            if (src.x == -1.f && src.y == -1.f) continue;
            if (alreadyHas(sce.srcId, sce.dstId, /*isMainOut*/false, /*isSidechain*/true))
                continue;
            CableHit hit;
            hit.srcId       = sce.srcId;
            hit.dstId       = sce.dstId;
            hit.scRecvSlot  = sce.dstSlot;
            hit.isSidechain = true;
            hits.push_back(hit);
        }
    }

    return hits;
}

MixerPage::CableOverlay::CableHit
MixerPage::CableOverlay::hitTestCable(juce::Point<float> pt) const
{
    auto all = hitTestCablesAll(pt);
    return all.empty() ? CableHit{} : all.front();
}

// ── CableSendPopup ───────────────────────────────────────────────────────────
// Inline component shown in a CallOutBox when the user right-clicks a send cable.
namespace {
class CableSendPopup : public juce::Component
{
public:
    CableSendPopup(juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& prefix, int sendSlot,
                   const juce::String& destName,
                   std::function<void()> onDeleteCb)
    {
        // Destination header
        mDestLabel.setText(juce::String("Send ") + juce::String(juce::CharPointer_UTF8("\xe2\x86\x92")) + " " + destName,
                           juce::dontSendNotification);
        mDestLabel.setFont(juce::Font(11.f, juce::Font::bold));
        mDestLabel.setColour(juce::Label::textColourId, juce::Colour(0xff33ff88));
        addAndMakeVisible(mDestLabel);

        // Amount slider
        mAmountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        mAmountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
        mAmountSlider.setRange(-60.0, 6.0, 0.1);
        mAmountSlider.setTextValueSuffix(" dB");
        addAndMakeVisible(mAmountSlider);

        const juce::String sp = prefix + "_send" + juce::String(sendSlot);
        mAmountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, sp + "_amount", mAmountSlider);

        // Pre/post toggle
        mPrePostBtn.setButtonText("Pre-Fader");
        mPrePostBtn.setClickingTogglesState(true);
        addAndMakeVisible(mPrePostBtn);
        mPrePostAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, sp + "_prepost", mPrePostBtn);

        // Delete button
        mDeleteBtn.setButtonText("Delete Send");
        mDeleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff442222));
        mDeleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff6666));
        mDeleteBtn.onClick = [this, onDeleteCb]
        {
            if (onDeleteCb) onDeleteCb();
            if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                cb->dismiss();
        };
        addAndMakeVisible(mDeleteBtn);

        setSize(210, 110);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(6);
        mDestLabel   .setBounds(b.removeFromTop(16));  b.removeFromTop(3);
        mAmountSlider.setBounds(b.removeFromTop(22));  b.removeFromTop(3);
        mPrePostBtn  .setBounds(b.removeFromTop(20));  b.removeFromTop(3);
        mDeleteBtn   .setBounds(b.removeFromTop(22));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1e2024));
    }

private:
    juce::Label        mDestLabel;
    VibeSlider         mAmountSlider;
    juce::ToggleButton mPrePostBtn;
    juce::TextButton   mDeleteBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAmountAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mPrePostAtt;
};

// QA-Eg: CableMainOutPopup class removed - main cables have no editable
// properties; the popup was just a glorified tooltip.  Main cables now
// appear as grayed-out entries in the right-click chooser (visibility
// only) and have no popup behavior on single-hit right-click either.
}  // anon namespace

void MixerPage::CableOverlay::showCablePopup(juce::Point<float> screenPt,
                                               const CableHit& hit)
{
    using namespace MixerChannelIds;

    auto getStripName = [&](int chId) -> juce::String
    {
        if (auto* s = owner.findStripByChannelId(chId))
            return s->getName();
        return "Ch " + juce::String(chId);
    };

    std::unique_ptr<juce::Component> content;

    if (hit.isSidechain)
    {
        // C.4 Phase 1 (2026-04-30): SC cable popup - info-only label + Delete.
        // No amount slider (SC tap is unity-gain post-everything per Q4=A) or
        // pre/post toggle.  Delete writes -1 back to TARGET's _sc_recv{N}_from.
        const juce::String targetPrefix = prefixFromChannelId(hit.dstId);
        auto deleteAction = [this, targetPrefix, slot = hit.scRecvSlot]
        {
            if (targetPrefix.isEmpty()) return;
            const juce::String sp = targetPrefix + "_sc_recv" + juce::String(slot);
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                    owner.mProcessor.apvts.getParameter(sp + "_from")))
                p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(-1.f));
            // C.4 Phase 1 (2026-04-30): force overlay repaint so the cable
            // disappears immediately.  RoutingGraph rebuilds from APVTS each
            // audio block, but nothing else triggers the UI to redraw after
            // the param write -- without this the cable lingers visually
            // until something else (mouse move, etc.) repaints the overlay.
            repaint();
        };

        // ScCablePopup: a small CallOutBox-shown widget with the routing info
        // label + a Delete button.  Defined locally because the SC popup has
        // a dedicated Delete affordance and no shared state with the cable
        // overlay; keeping it inline avoids growing the header surface.
        struct ScCablePopup : public juce::Component {
            juce::Label    info;
            juce::TextButton delBtn;
            std::function<void()> onDelete;
            ScCablePopup(juce::String src, juce::String dst, int line,
                          std::function<void()> del)
                : onDelete(std::move(del))
            {
                info.setText("Sidechain: " + src + " -> " + dst
                              + "  (line " + juce::String(line) + ")",
                              juce::dontSendNotification);
                info.setColour(juce::Label::textColourId, juce::Colours::white);
                info.setJustificationType(juce::Justification::centred);
                addAndMakeVisible(info);

                delBtn.setButtonText("Delete");
                delBtn.setColour(juce::TextButton::buttonColourId,
                                  juce::Colour(0xff4a3030));
                delBtn.onClick = [this] {
                    if (onDelete) onDelete();
                    if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                        cb->dismiss();
                };
                addAndMakeVisible(delBtn);

                setSize(220, 60);
            }
            void resized() override
            {
                info.setBounds(0, 4, getWidth(), 22);
                delBtn.setBounds(getWidth()/2 - 40, 30, 80, 24);
            }
            void paint(juce::Graphics& g) override
            {
                g.fillAll(juce::Colour(0xff1e2024));
            }
        };

        content = std::make_unique<ScCablePopup>(
            getStripName(hit.srcId), getStripName(hit.dstId),
            hit.scRecvSlot, std::move(deleteAction));
    }
    else if (hit.isMainOut)
    {
        // QA-Eg: main cables have no popup.  User drags the cable to move/
        // remove the routing.  Chooser shows them grayed-out for visibility;
        // single-hit right-click on a main does nothing.
        return;
    }
    else if (hit.sendSlot >= 0)
    {
        const juce::String prefix = prefixFromChannelId(hit.srcId);
        auto deleteAction = [this, prefix, slot = hit.sendSlot]
        {
            const juce::String sp = prefix + "_send" + juce::String(slot);
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                    owner.mProcessor.apvts.getParameter(sp + "_to")))
                p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(-1.f));
        };

        content = std::make_unique<CableSendPopup>(
            owner.mProcessor.apvts, prefix, hit.sendSlot,
            getStripName(hit.dstId), std::move(deleteAction));
    }
    else
    {
        return;   // shouldn't happen
    }

    auto& cb = juce::CallOutBox::launchAsynchronously(
        std::move(content),
        juce::Rectangle<int>((int) screenPt.x - 4, (int) screenPt.y - 4, 8, 8),
        nullptr);
    juce::ignoreUnused(cb);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5F-4b B3: strip lookup + socket position helpers
// ─────────────────────────────────────────────────────────────────────────────
// QA-Eg fix-up (perf-audit H2): single-lookup cache replaces the prior linear
// scan over 12 bus pointers + 8 std::map containers.  rebuildStripCache walks
// all current strips once; subsequent calls are O(1) hash lookups until the
// next strip add / remove invalidates the cache.
MixerTrackStrip* MixerPage::findStripByChannelId(int channelId) const
{
    if (mStripCacheDirty)
    {
        rebuildStripCache();
        mStripCacheDirty = false;
    }
    auto it = mStripByChannelId.find(channelId);
    return it != mStripByChannelId.end() ? it->second : nullptr;
}

void MixerPage::rebuildStripCache() const
{
    using namespace MixerChannelIds;

    mStripByChannelId.clear();

    auto reg = [this](int id, MixerTrackStrip* s)
    {
        if (s) mStripByChannelId[id] = s;
    };

    // Bus strips (12 fixed-id slots; secondary Vox/Inst + RustyDrums may be null).
    reg(kMaster,        mMasterStrip       .get());
    reg(kLayersBus,     mLayersBusStrip    .get());
    reg(kBassBus,       mBassBusStrip      .get());
    reg(kDrumsBus,      mDrumsBusStrip     .get());
    reg(kFxBus,         mFXBusStrip        .get());
    reg(kClipsBus,      mAudioClipsBusStrip.get());
    reg(kVoxBus,        mVoxBusStrip       .get());
    reg(kInstBus,       mInstBusStrip      .get());
    reg(kVoxBus2,       mVoxBus2Strip      .get());
    reg(kInstBus2,      mInstBus2Strip     .get());
    reg(kInstBus3,      mInstBus3Strip     .get());
    reg(kRustyDrumsBus, mRustyDrumsBusStrip.get());

    // Dynamic instrument channels (8 std::map containers, keyed by index).
    for (auto& [tabId, strip] : mLayerStrips) reg(kLayerBase + tabId, strip.get());
    for (auto& [tabId, strip] : mBassStrips)  reg(kBassBase  + tabId, strip.get());
    for (auto& [slot,  strip] : mDrumStrips)  reg(kDrumBase  + slot,  strip.get());
    for (auto& [row,   strip] : mAudioStrips) reg(kAudioBase + row,   strip.get());
    for (auto& [idx,   strip] : mAuxStrips)   reg(kAuxBase   + idx,   strip.get());
    for (auto& [idx,   strip] : mVoxStrips)   reg(kVoxBase   + idx,   strip.get());
    for (auto& [idx,   strip] : mInstStrips)  reg(kInstBase  + idx,   strip.get());
    for (auto& [idx,   strip] : mRustyStrips) reg(kRustyBase + idx,   strip.get());
}

void MixerPage::reRegisterStripAutomation()
{
    auto reReg = [](MixerTrackStrip* s) { if (s) s->reRegisterAutomation(); };
    reReg (mMasterStrip.get());
    reReg (mLayersBusStrip.get());
    reReg (mBassBusStrip.get());
    reReg (mDrumsBusStrip.get());
    reReg (mFXBusStrip.get());
    reReg (mAudioClipsBusStrip.get());
    reReg (mVoxBusStrip.get());
    reReg (mInstBusStrip.get());
    reReg (mVoxBus2Strip.get());
    reReg (mInstBus2Strip.get());
    reReg (mInstBus3Strip.get());
    reReg (mRustyDrumsBusStrip.get());
}

juce::Point<float> MixerPage::getSocketPosition(int channelId) const
{
    auto* strip = findStripByChannelId(channelId);
    if (strip == nullptr || ! strip->isVisible())
        return { -1.f, -1.f };

    auto sb = strip->getBoundsInParent();
    auto sc = strip->getSocketCentre();   // local coords within strip

    // Master strip is a direct child of MixerPage.
    if (strip->getParentComponent() == const_cast<MixerPage*>(this))
        return { sb.getX() + (float) sc.x, sb.getY() + (float) sc.y };

    // All other strips live inside mScrollContent within mViewport.
    const float px = mViewport->getX() + sb.getX() + (float) sc.x
                   - mViewport->getViewPositionX();
    const float py = mViewport->getY() + sb.getY() + (float) sc.y
                   - mViewport->getViewPositionY();

    return { px, py };
}

// ─────────────────────────────────────────────────────────────────────────────
MixerPage::MixerPage(VibeSynthProcessor& processor, PatternManager& pm)
    : mProcessor(processor), mPM(pm)
{
    // 5F-4a: ensure master + 5 bus strip APVTS params exist before we bind.
    mProcessor.ensureMixerBusAndMasterParams();

    mMasterStrip = std::make_unique<MixerTrackStrip>("Master",
                       MixerTrackStrip::StripType::Master, juce::Colour(kMixerTabPurple));
    mMasterStrip->setAutomationPrefix("mixer_master");
    mMasterStrip->setApvts(mProcessor.apvts, "mixer_master");
    wireMasterCallbacks();
    addAndMakeVisible(*mMasterStrip);

    mScrollContent = std::make_unique<ScrollContent>();
    mViewport = std::make_unique<juce::Viewport>();
    mViewport->setViewedComponent(mScrollContent.get(), false);
    // Hide BOTH of the viewport's internal scrollbars - we place our own
    // permanent horizontal scrollbar at the top of the page so the cable
    // overlay can't cover it. Allow horizontal scrolling even without the
    // viewport's own bar (we drive it via setViewPosition).
    mViewport->setScrollBarsShown(false, false,
                                  /*allowVScrollWithoutBar*/ false,
                                  /*allowHScrollWithoutBar*/ true);
    addAndMakeVisible(*mViewport);

    mHScrollBar = std::make_unique<juce::ScrollBar>(/*isVertical*/ false);
    mHScrollBar->setAutoHide(false);              // permanent
    mHScrollBar->addListener(this);
    mHScrollBar->setRangeLimits(0.0, 1.0, juce::dontSendNotification);
    mHScrollBar->setCurrentRange(0.0, 1.0, juce::dontSendNotification);
    addAndMakeVisible(*mHScrollBar);

    auto& mx = mPM.getMixer();
    mLayersBusStrip     = std::make_unique<MixerTrackStrip>("Layers Bus",
                              MixerTrackStrip::StripType::Bus, VC::LayerCol[0]);
    mBassBusStrip       = std::make_unique<MixerTrackStrip>("Bass Bus",
                              MixerTrackStrip::StripType::Bus, VC::BassCol[0]);
    mDrumsBusStrip      = std::make_unique<MixerTrackStrip>("Drums Bus",
                              MixerTrackStrip::StripType::Bus, VC::DrumsCol);
    mFXBusStrip         = std::make_unique<MixerTrackStrip>("FX Bus",
                              MixerTrackStrip::StripType::Bus, juce::Colour(kEffectsTabPink));
    mAudioClipsBusStrip = std::make_unique<MixerTrackStrip>("Clips Bus",
                              MixerTrackStrip::StripType::Bus, VC::Warm);
    // R3.5 (2026-04-23): Vox + Inst BUS strips - teal + navy accents.
    mVoxBusStrip  = std::make_unique<MixerTrackStrip>("Vox Bus",
                              MixerTrackStrip::StripType::Bus, juce::Colour(0xFF0FAFA5));
    mInstBusStrip = std::make_unique<MixerTrackStrip>("Inst Bus",
                              MixerTrackStrip::StripType::Bus, juce::Colour(0xFF1C3A8A));
    // J-5 (2026-05-03): BaySickRustyDrums dedicated bus.  Drums-red accent
    // matches the existing Drums Bus so the user reads them as the same family.
    mRustyDrumsBusStrip = std::make_unique<MixerTrackStrip>("RustyDrums Bus",
                              MixerTrackStrip::StripType::Bus, VC::DrumsCol);

    mLayersBusStrip    ->setAutomationPrefix("mixer_layers");
    mBassBusStrip      ->setAutomationPrefix("mixer_bass");
    mDrumsBusStrip     ->setAutomationPrefix("mixer_drums");
    mFXBusStrip        ->setAutomationPrefix("mixer_fx");
    mAudioClipsBusStrip->setAutomationPrefix("mixer_clipsbus");
    mVoxBusStrip       ->setAutomationPrefix("mixer_voxbus");
    mInstBusStrip      ->setAutomationPrefix("mixer_instbus");
    mRustyDrumsBusStrip->setAutomationPrefix("mixer_rustybus");

    // 5F-4a: bind each bus strip's new controls (polarity/width/bypass) to APVTS
    mLayersBusStrip    ->setApvts(mProcessor.apvts, "mixer_layers");
    mBassBusStrip      ->setApvts(mProcessor.apvts, "mixer_bass");
    mDrumsBusStrip     ->setApvts(mProcessor.apvts, "mixer_drums");
    mFXBusStrip        ->setApvts(mProcessor.apvts, "mixer_fx");
    mAudioClipsBusStrip->setApvts(mProcessor.apvts, "mixer_clipsbus");
    mVoxBusStrip       ->setApvts(mProcessor.apvts, "mixer_voxbus");
    mInstBusStrip      ->setApvts(mProcessor.apvts, "mixer_instbus");
    mRustyDrumsBusStrip->setApvts(mProcessor.apvts, "mixer_rustybus");

    wireBusCallbacks(mLayersBusStrip.get(),     mx.layersLevel,          mx.layersPan,        mx.layersMute,         mx.layersSolo);
    wireBusCallbacks(mBassBusStrip.get(),        mx.bassLevel,            mx.bassPan,          mx.bassMute,           mx.bassSolo);
    wireBusCallbacks(mDrumsBusStrip.get(),       mx.drumsLevel,           mx.drumsPan,         mx.drumsMute,          mx.drumsSolo);
    wireBusCallbacks(mAudioClipsBusStrip.get(),  mx.audioClipsBusLevel,   mx.audioClipsBusPan, mx.audioClipsBusMute,  mx.audioClipsBusSolo);

    // FX Bus doesn't participate in the legacy MixerState struct (no dedicated
    // level/pan/mute/solo members), so wireBusCallbacks isn't used. Still wire
    // onFXClicked so its FX Rack button navigates to the Effects Page.
    mFXBusStrip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    // R3.5: Vox + Inst bus FX rack buttons navigate to Effects page like other buses.
    mVoxBusStrip ->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    mInstBusStrip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    mRustyDrumsBusStrip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };

    // G-6 (2026-04-29): Vox + Inst BUS strips are user-renameable.  System
    // buses (Layers/Bass/Drums/FX/Clips/Master) keep fixed names since they
    // map to fixed channel identities.  Names persist via UIState (see
    // serialize/deserialize in StandaloneEditor).
    mVoxBusStrip ->setRenameable (true);
    mInstBusStrip->setRenameable (true);
    mVoxBusStrip->onNameChanged = [this](const juce::String&) {
        if (getWidth() > 0) layoutScrollContent();
        if (onAudioStripRenamed) onAudioStripRenamed();
    };
    mInstBusStrip->onNameChanged = [this](const juce::String&) {
        if (getWidth() > 0) layoutScrollContent();
        if (onAudioStripRenamed) onAudioStripRenamed();
    };

    mScrollContent->addAndMakeVisible(*mLayersBusStrip);
    mScrollContent->addAndMakeVisible(*mBassBusStrip);
    mScrollContent->addAndMakeVisible(*mDrumsBusStrip);
    mScrollContent->addAndMakeVisible(*mFXBusStrip);
    mScrollContent->addAndMakeVisible(*mAudioClipsBusStrip);
    mScrollContent->addAndMakeVisible(*mVoxBusStrip);
    mScrollContent->addAndMakeVisible(*mInstBusStrip);
    // J-5: bus strip added to children but visibility flag (mRustyDrumsBusActive)
    // is what gates whether layoutScrollContent positions it on-screen.
    mScrollContent->addChildComponent(*mRustyDrumsBusStrip);
    mRustyDrumsBusStrip->setVisible(false);

    // Direct Routing label (hidden until any strip main-outs to Master)
    mDirectRoutingLabel = std::make_unique<DirectRoutingLabel>();
    mDirectRoutingLabel->setInterceptsMouseClicks(false, false);
    mScrollContent->addChildComponent(*mDirectRoutingLabel);

    // 5F-4b B3: cable overlay sits on top of everything
    mCableOverlay = std::make_unique<CableOverlay>(*this);
    addAndMakeVisible(*mCableOverlay);

    // 5F-4b B5: wire "+" add-send button on fixed strips
    auto wireSendBtn = [this](MixerTrackStrip* s)
    {
        s->onAddSendRequested = [this](int chId)
        {
            onAddCableRequestedFor(chId);
        };
    };
    wireSendBtn(mMasterStrip.get());
    wireSendBtn(mLayersBusStrip.get());
    wireSendBtn(mBassBusStrip.get());
    wireSendBtn(mDrumsBusStrip.get());
    wireSendBtn(mFXBusStrip.get());
    wireSendBtn(mAudioClipsBusStrip.get());
    wireSendBtn(mVoxBusStrip.get());
    wireSendBtn(mInstBusStrip.get());
    wireSendBtn(mRustyDrumsBusStrip.get());

    // 5F-4b B3: set channel IDs on fixed strips for cable rendering
    mMasterStrip      ->setChannelId(MixerChannelIds::kMaster);
    mLayersBusStrip   ->setChannelId(MixerChannelIds::kLayersBus);
    mBassBusStrip     ->setChannelId(MixerChannelIds::kBassBus);
    mDrumsBusStrip    ->setChannelId(MixerChannelIds::kDrumsBus);
    mFXBusStrip       ->setChannelId(MixerChannelIds::kFxBus);
    mAudioClipsBusStrip->setChannelId(MixerChannelIds::kClipsBus);
    mVoxBusStrip      ->setChannelId(MixerChannelIds::kVoxBus);
    mInstBusStrip     ->setChannelId(MixerChannelIds::kInstBus);
    mRustyDrumsBusStrip->setChannelId(MixerChannelIds::kRustyDrumsBus);

    // 5F-4b B2: "Add Aux Strip" button (renamed from "Add Mixer Strip" during
    // R1 2026-04-23 when Vox + Inst were added alongside).  Owned here,
    // reparented into the PageMenuBar when the Mixer page becomes visible.
    mAddAuxBtn = std::make_unique<juce::TextButton>("Add Aux Strip");
    mAddAuxBtn->setTooltip("Add a new Aux / Group strip to the mixer");
    mAddAuxBtn->setColour(juce::TextButton::buttonColourId, VC::Surface);
    mAddAuxBtn->setColour(juce::TextButton::textColourOffId, VC::Text);
    mAddAuxBtn->onClick = [this] { addAuxChannel(); };

    // R1 (2026-04-23): Add Vox Strip + Add Inst Strip buttons.  Same reparent
    // pattern as Add Aux - StandaloneEditor moves them into the PageMenuBar
    // while the Mixer page is visible.
    mAddVoxBtn = std::make_unique<juce::TextButton>("Add Vox Strip");
    mAddVoxBtn->setTooltip("Add a live-input vocal strip (up to 6)");
    mAddVoxBtn->setColour(juce::TextButton::buttonColourId, VC::Surface);
    mAddVoxBtn->setColour(juce::TextButton::textColourOffId, VC::Text);
    mAddVoxBtn->onClick = [this] { addVoxChannel(); };

    mAddInstBtn = std::make_unique<juce::TextButton>("Add Inst Strip");
    mAddInstBtn->setTooltip("Add a live-input instrument strip (up to 20)");
    mAddInstBtn->setColour(juce::TextButton::buttonColourId, VC::Surface);
    mAddInstBtn->setColour(juce::TextButton::textColourOffId, VC::Text);
    mAddInstBtn->onClick = [this] { addInstChannel(); };

    // G-6 (2026-04-29): Add Vox Bus + Add Inst Bus buttons.  Each opens a
    // secondary bus on demand (Vox: 1 extra max → kVoxBus2; Inst: 2 extra
    // max → kInstBus2 / kInstBus3).  Greyed out at cap.
    mAddVoxBusBtn = std::make_unique<juce::TextButton>("Add Vox Bus");
    mAddVoxBusBtn->setTooltip("Add a second Vox bus (e.g. lead vs backup vocals).  Max 1 extra.");
    mAddVoxBusBtn->setColour(juce::TextButton::buttonColourId, VC::Surface);
    mAddVoxBusBtn->setColour(juce::TextButton::textColourOffId, VC::Text);
    mAddVoxBusBtn->onClick = [this] { activateVoxBus2(); };

    mAddInstBusBtn = std::make_unique<juce::TextButton>("Add Inst Bus");
    mAddInstBusBtn->setTooltip("Add a secondary Inst bus (e.g. guitars vs bass).  Max 2 extra.");
    mAddInstBusBtn->setColour(juce::TextButton::buttonColourId, VC::Surface);
    mAddInstBusBtn->setColour(juce::TextButton::textColourOffId, VC::Text);
    mAddInstBusBtn->onClick = [this]
    {
        if      (! mInstBus2Active) activateInstBus2();
        else if (! mInstBus3Active) activateInstBus3();
    };

    // 5F-4b B7: restore any aux strips that were in the saved project.
    // VibeGraph already has their InsertNodes (registered by restoreAuxStripsFromState
    // in setStateInformation). Create the matching UI strips.
    for (int idx : mProcessor.mVibeGraph.getAuxIndices())
        addAuxChannelAtIndex(idx);

    syncFromModel();
    syncApvtsFromMixerState();   // 5F-4a Batch 6
    startTimerHz(30);

    // 2026-05-02: vblank-locked meter feed.  Each monitor refresh, exchange
    // every per-strip peak atomic with -inf and push the running max to the
    // matching DBFSMeter.  Mirrors FL Studio's vsync-locked metering.
    mVBlank = std::make_unique<juce::VBlankAttachment> (this, [this] { onVBlank(); });
}

MixerPage::~MixerPage()
{
    mVBlank.reset();
    stopTimer();
}

// ─────────────────────────────────────────────────────────────────────────────
// Lazy channel creation
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::addLayerChannel(int pageIndex, const juce::String& name)
{
    // pageIndex is authoritative - matches registerLayerEngine's InsertNode
    // key + APVTS prefix (mixer_layer_{pageIndex}). Caller translates ribbonTabId
    // to pageIndex before calling.
    if (mLayerStrips.count(pageIndex) > 0) return;

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::LayerChannel, VC::LayerCol[0]);
    const juce::String prefix = "mixer_layer_" + juce::String(pageIndex);
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::layerInsert(pageIndex));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };

    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onNameChanged = [this, pageIndex](const juce::String& newName) {
        if (onChannelRenamed) onChannelRenamed(pageIndex, newName);
    };

    mScrollContent->addAndMakeVisible(*strip);
    mLayerStrips[pageIndex] = std::move(strip);
    mLayerTabOrder.push_back(pageIndex);

    if (getWidth() > 0) resized();
}

void MixerPage::addBassChannel(int pageIndex, const juce::String& name)
{
    // pageIndex is authoritative - see addLayerChannel().
    if (mBassStrips.count(pageIndex) > 0) return;

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::BassChannel, VC::BassCol[0]);
    const juce::String prefix = "mixer_bass_" + juce::String(pageIndex);
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::bassInsert(pageIndex));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };

    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onNameChanged = [this, pageIndex](const juce::String& newName) {
        if (onChannelRenamed) onChannelRenamed(pageIndex, newName);
    };

    mScrollContent->addAndMakeVisible(*strip);
    mBassStrips[pageIndex] = std::move(strip);
    mBassTabOrder.push_back(pageIndex);

    if (getWidth() > 0) resized();
}

void MixerPage::addDrumChannel(int slot, const juce::String& name)
{
    if (name.isEmpty()) return;

    if (mDrumStrips.count(slot) > 0)
    {
        // Update name when user reassigns a different sound to the same slot
        mDrumStrips[slot]->setTrackName(name);
        return;
    }

    auto strip = std::make_unique<MixerTrackStrip>(
        name,
        MixerTrackStrip::StripType::DrumChannel, VC::DrumsCol);

    // 5F-4a: bind drum insert strip's APVTS (polarity/width/bypass/arm).
    const juce::String drumPrefix = "mixer_drum_" + juce::String(slot);
    strip->setAutomationPrefix(drumPrefix);
    strip->setApvts(mProcessor.apvts, drumPrefix);
    strip->setChannelId(MixerChannelIds::drumInsert(slot));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };

    if (slot < MAX_DRUM_ROWS)
    {
        const int capturedSlot = slot;
        strip->onFaderDragStarted = [this] { mMixerStateBefore = mPM.getMixer(); };
        strip->onFaderDragEnded   = [this]
        {
            if (!mUndoCtx.isValid()) return;
            MixerState after = mPM.getMixer();
            mUndoCtx.perform(new MixerStateAction("Drum Level",
                mMixerStateBefore, after,
                [this](const MixerState& s) { applyMixerSnapshot(s); }), "Drum Level");
        };
        strip->onFaderChanged = [this, capturedSlot](float db) {
            mPM.getMixer().drumSlotLevel[capturedSlot] =
                juce::Decibels::decibelsToGain(db, -60.0f);
        };
        strip->onPanDragStarted = [this] { mMixerStateBefore = mPM.getMixer(); };
        strip->onPanDragEnded   = [this]
        {
            if (!mUndoCtx.isValid()) return;
            MixerState after = mPM.getMixer();
            mUndoCtx.perform(new MixerStateAction("Drum Pan",
                mMixerStateBefore, after,
                [this](const MixerState& s) { applyMixerSnapshot(s); }), "Drum Pan");
        };
        strip->onPanChanged = [this, capturedSlot](float pan) {
            mPM.getMixer().drumSlotPan[capturedSlot] = pan;
        };
        strip->setFaderDb(juce::Decibels::gainToDecibels(
            mPM.getMixer().drumSlotLevel[slot], -60.0f));
        strip->setPan(mPM.getMixer().drumSlotPan[slot]);
    }
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };

    mScrollContent->addAndMakeVisible(*strip);
    mDrumStrips[slot] = std::move(strip);
    mDrumSlotOrder.push_back(slot);

    if (getWidth() > 0) resized();
}

// 5F-4b B2+B7: Aux/Group strip factory. Receive-only, no source engine.
void MixerPage::addAuxChannel()
{
    addAuxChannelAtIndex(mNextAuxIdx);

    // QA-Ef #5 (2026-05-22): flag the project dirty so the user gets the unsaved
    // marker / save prompt after adding an aux.  createAndAddParameter for the
    // strip's APVTS params doesn't fire a value-change, so the normal APVTS
    // dirty hook never sees this edit.  onAnyStateChange is wired to
    // ProjectManager::markDirty by StandaloneEditor and markDirty itself
    // respects mIgnoreDirty -- so the same callback no-ops harmlessly during
    // a load (which calls addAuxChannelAtIndex directly, never this user-
    // initiated entry).
    if (mProcessor.onAnyStateChange)
        mProcessor.onAnyStateChange();
}

// R1 (2026-04-23): Vox + Inst strip creators.  Mirror of addAuxChannel /
// addAuxChannelAtIndex.  Capped at kMaxVoxStrips / kMaxInstStrips per the
// spec lock.  ensureVoxInsert / ensureInstInsert on the processor side
// registers APVTS params + InsertNode; this side just creates the UI strip.
void MixerPage::addVoxChannel()  { addVoxChannelAtIndex  (mNextVoxIdx);  }
void MixerPage::addInstChannel() { addInstChannelAtIndex (mNextInstIdx); }

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): secondary Vox/Inst bus activation.  Lazy-creates the
// strip + flags the bus active.  Idempotent - already-active activate is
// a no-op (returns false).  Audio infrastructure (DSP node + APVTS params)
// is always allocated; this just controls UI presence + route picker
// filtering.  Bus rename works the same as primary bus strip rename.
// ─────────────────────────────────────────────────────────────────────────────
bool MixerPage::activateVoxBus2()
{
    if (mVoxBus2Active) return false;
    if (! mVoxBus2Strip)
    {
        mVoxBus2Strip = std::make_unique<MixerTrackStrip>("Vox Bus 2",
            MixerTrackStrip::StripType::Bus, juce::Colour(0xFF0FAFA5));   // teal
        mVoxBus2Strip->setAutomationPrefix("mixer_voxbus2");
        mVoxBus2Strip->setApvts(mProcessor.apvts, "mixer_voxbus2");
        mVoxBus2Strip->setChannelId(MixerChannelIds::kVoxBus2);
        mVoxBus2Strip->onAddSendRequested = [this](int chId) {
            onAddCableRequestedFor(chId);
        };
        mVoxBus2Strip->onFXClicked = [this](const juce::String& id) {
            if (onEffectsTabRequested) onEffectsTabRequested(id);
        };
        mVoxBus2Strip->onNameChanged = [this](const juce::String&) {
            if (getWidth() > 0) layoutScrollContent();
            if (onAudioStripRenamed) onAudioStripRenamed();
        };
        mVoxBus2Strip->setRenameable (true);   // G-6: Vox/Inst buses are user-renameable
        // G-7: right-click → Delete with auto-reroute attached strips → kVoxBus.
        {
            juce::Component::SafePointer<MixerPage> safeThis (this);
            mVoxBus2Strip->onContextMenuRequested = [safeThis] (juce::Point<int> screenPos)
            {
                if (! safeThis) return;
                juce::PopupMenu m;
                m.addItem (1, "Delete Vox Bus");
                m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                                    juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1)),
                    [safeThis] (int r)
                    {
                        if (! safeThis || r != 1) return;
                        auto* aw = new juce::AlertWindow (
                            "Delete Vox Bus",
                            "Deleting this bus removes its Effects Rack.\n"
                            "All attached strips will be moved to the main Vox Bus.",
                            juce::AlertWindow::WarningIcon);
                        aw->addButton ("Delete", 1);
                        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                        aw->enterModalState (true, juce::ModalCallbackFunction::create (
                            [safeThis, aw] (int result)
                            {
                                std::unique_ptr<juce::AlertWindow> own (aw);
                                if (result != 1 || ! safeThis) return;
                                safeThis->deleteSecondaryBus (MixerChannelIds::kVoxBus2);
                            }), false);
                    });
            };
        }
        mScrollContent->addAndMakeVisible(*mVoxBus2Strip);
    }
    else
    {
        mVoxBus2Strip->setVisible(true);
    }
    mVoxBus2Active = true;
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();   // refresh Effects dropdown
    if (mAddVoxBusBtn) mAddVoxBusBtn->setEnabled(false);   // capped at 1 extra
    return true;
}

bool MixerPage::activateInstBus2()
{
    if (mInstBus2Active) return false;
    if (! mInstBus2Strip)
    {
        mInstBus2Strip = std::make_unique<MixerTrackStrip>("Inst Bus 2",
            MixerTrackStrip::StripType::Bus, juce::Colour(0xFF1C3A8A));   // navy
        mInstBus2Strip->setAutomationPrefix("mixer_instbus2");
        mInstBus2Strip->setApvts(mProcessor.apvts, "mixer_instbus2");
        mInstBus2Strip->setChannelId(MixerChannelIds::kInstBus2);
        mInstBus2Strip->onAddSendRequested = [this](int chId) {
            onAddCableRequestedFor(chId);
        };
        mInstBus2Strip->onFXClicked = [this](const juce::String& id) {
            if (onEffectsTabRequested) onEffectsTabRequested(id);
        };
        mInstBus2Strip->onNameChanged = [this](const juce::String&) {
            if (getWidth() > 0) layoutScrollContent();
            if (onAudioStripRenamed) onAudioStripRenamed();
        };
        mInstBus2Strip->setRenameable (true);   // G-6
        {
            juce::Component::SafePointer<MixerPage> safeThis (this);
            mInstBus2Strip->onContextMenuRequested = [safeThis] (juce::Point<int> screenPos)
            {
                if (! safeThis) return;
                juce::PopupMenu m;
                m.addItem (1, "Delete Inst Bus");
                m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                                    juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1)),
                    [safeThis] (int r)
                    {
                        if (! safeThis || r != 1) return;
                        auto* aw = new juce::AlertWindow (
                            "Delete Inst Bus",
                            "Deleting this bus removes its Effects Rack.\n"
                            "All attached strips will be moved to the main Inst Bus.",
                            juce::AlertWindow::WarningIcon);
                        aw->addButton ("Delete", 1);
                        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                        aw->enterModalState (true, juce::ModalCallbackFunction::create (
                            [safeThis, aw] (int result)
                            {
                                std::unique_ptr<juce::AlertWindow> own (aw);
                                if (result != 1 || ! safeThis) return;
                                safeThis->deleteSecondaryBus (MixerChannelIds::kInstBus2);
                            }), false);
                    });
            };
        }
        mScrollContent->addAndMakeVisible(*mInstBus2Strip);
    }
    else
    {
        mInstBus2Strip->setVisible(true);
    }
    mInstBus2Active = true;
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
    return true;
}

bool MixerPage::activateInstBus3()
{
    if (mInstBus3Active) return false;
    if (! mInstBus3Strip)
    {
        mInstBus3Strip = std::make_unique<MixerTrackStrip>("Inst Bus 3",
            MixerTrackStrip::StripType::Bus, juce::Colour(0xFF1C3A8A));   // navy
        mInstBus3Strip->setAutomationPrefix("mixer_instbus3");
        mInstBus3Strip->setApvts(mProcessor.apvts, "mixer_instbus3");
        mInstBus3Strip->setChannelId(MixerChannelIds::kInstBus3);
        mInstBus3Strip->onAddSendRequested = [this](int chId) {
            onAddCableRequestedFor(chId);
        };
        mInstBus3Strip->onFXClicked = [this](const juce::String& id) {
            if (onEffectsTabRequested) onEffectsTabRequested(id);
        };
        mInstBus3Strip->onNameChanged = [this](const juce::String&) {
            if (getWidth() > 0) layoutScrollContent();
            if (onAudioStripRenamed) onAudioStripRenamed();
        };
        mInstBus3Strip->setRenameable (true);   // G-6
        {
            juce::Component::SafePointer<MixerPage> safeThis (this);
            mInstBus3Strip->onContextMenuRequested = [safeThis] (juce::Point<int> screenPos)
            {
                if (! safeThis) return;
                juce::PopupMenu m;
                m.addItem (1, "Delete Inst Bus");
                m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                                    juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1)),
                    [safeThis] (int r)
                    {
                        if (! safeThis || r != 1) return;
                        auto* aw = new juce::AlertWindow (
                            "Delete Inst Bus",
                            "Deleting this bus removes its Effects Rack.\n"
                            "All attached strips will be moved to the main Inst Bus.",
                            juce::AlertWindow::WarningIcon);
                        aw->addButton ("Delete", 1);
                        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                        aw->enterModalState (true, juce::ModalCallbackFunction::create (
                            [safeThis, aw] (int result)
                            {
                                std::unique_ptr<juce::AlertWindow> own (aw);
                                if (result != 1 || ! safeThis) return;
                                safeThis->deleteSecondaryBus (MixerChannelIds::kInstBus3);
                            }), false);
                    });
            };
        }
        mScrollContent->addAndMakeVisible(*mInstBus3Strip);
    }
    else
    {
        mInstBus3Strip->setVisible(true);
    }
    mInstBus3Active = true;
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
    if (mAddInstBusBtn) mAddInstBusBtn->setEnabled(false);   // capped at 2 extra
    return true;
}

void MixerPage::addVoxChannelAtIndex(int idx)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxVoxStrips) return;
    if (mVoxStrips.count(idx) > 0) return;

    const juce::String prefix = "mixer_vox_" + juce::String(idx);
    const juce::String name   = "Vox " + juce::String(idx + 1);
    mProcessor.ensureVoxInsert(idx, name);

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::Vox, juce::Colour(0xFF0FAFA5));   // teal
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::voxInsert(idx));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onNameChanged = [this](const juce::String&) {
        if (getWidth() > 0) layoutScrollContent();
        if (onAudioStripRenamed) onAudioStripRenamed();
    };
    // R2 (2026-04-23): Arm LED click -> ASIO input picker.
    strip->onArmRequested = [this](int chId) { showInputChannelPicker(chId); };

    mScrollContent->addAndMakeVisible(*strip);
    mVoxStrips[idx] = std::move(strip);
    mVoxOrder.push_back(idx);
    mNextVoxIdx = juce::jmax(mNextVoxIdx, idx + 1);

    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
    // G-4 (2026-04-28): notify StandaloneEditor so the matching Vox ribbon
    // page is spawned alongside the strip.  spawnVoxTabIfMissing is idempotent
    // so firing this on project-load restore paths is safe.
    if (onVoxStripAdded) onVoxStripAdded(idx);

    // QA-Fb G2-dirty: createAndAddParameter for the strip's APVTS params never
    // fires a value-change, so the APVTS dirty hook can't see a page-add
    // (mirror of addAuxChannel / QA-Ef #5).  Fired here in the AtIndex body --
    // unlike aux, user gestures reach this directly (onAddTabRequest, tab
    // duplicate, Add-From-Export) -- and markDirty no-ops while
    // ProjectManager::mIgnoreDirty is up, so deserializeUIState's restore-time
    // calls stay clean (onDeserializeUIState fires inside the openProject
    // ignore-dirty window).
    if (mProcessor.onAnyStateChange)
        mProcessor.onAnyStateChange();
}

// R2 (2026-04-23): shared ASIO input-channel picker for Vox + Inst Arm-LED
// clicks.  Reads channel names from the AudioDeviceManager via the
// B2 + B1 (2026-05-04): smart channel grouping for the input picker.
//
// B2 heuristic - pair adjacent channels if their names suggest a stereo pair:
//   * suffix "L"/"R" or " L"/" R" or " (L)"/" (R)" - case-insensitive
//   * common base prefix (everything except the suffix) is identical
// B1 fallback - known device profiles when names are generic ("IN 1", "IN 2", ...)
//   * Tascam Model 24: pairs at 13/14, 15/16, 17/18, 19/20, 21/22 (line inputs).
//     23/24 is the master mix bus and is left as two mono entries - not a true
//     stereo line-input pair.
//   * Tascam Model 16 / Model 12: similar smaller layouts.
// Anything not paired stays mono.  Pure code-side; no UI option to override.
namespace
{
struct ChannelGroup
{
    int          startIdx;     // 0-based first channel in the group
    bool         isPair;       // false = single mono channel
    juce::String label;        // display text for the picker item
};

struct DevicePairProfile
{
    const char*       deviceNameContains;
    int               requiredChannelCount;   // -1 = any count
    std::vector<int>  pairFirstIndices;       // 0-based starts; pairs are (i, i+1)
};

static const DevicePairProfile kDeviceProfiles[] =
{
    // Tascam ships a single "Model Mixer ASIO" driver across the entire
    // Model series - Model 12, 16, 24, 2400 all use the same driver name.
    // Channel count is the only thing that disambiguates them at runtime.
    //
    // Model 24 = 24 input channels: 12 mono mic preamps + 5 line-input
    // stereo pairs (13/14..21/22) + 2 mono master-mix returns (23/24).
    { "Model Mixer", 24, { 12, 14, 16, 18, 20 } },
};

// Returns true if `name` ends with one of the L-channel suffixes; if so
// `outBase` is the name minus the suffix.
static bool stripLeftSuffix (const juce::String& name, juce::String& outBase)
{
    static const char* kLs[] = { " L", "_L", "-L", "(L)", " (L)" };
    for (auto* s : kLs)
    {
        if (name.endsWithIgnoreCase (s))
        {
            outBase = name.dropLastCharacters ((int) std::strlen (s)).trimEnd();
            return true;
        }
    }
    if (name.endsWithIgnoreCase ("L") && ! name.endsWithIgnoreCase ("AL")
        && ! name.endsWithIgnoreCase ("EL") && ! name.endsWithIgnoreCase ("IL")
        && ! name.endsWithIgnoreCase ("OL") && ! name.endsWithIgnoreCase ("UL"))
    {
        outBase = name.dropLastCharacters (1).trimEnd();
        return true;
    }
    return false;
}

static bool stripRightSuffix (const juce::String& name, juce::String& outBase)
{
    static const char* kRs[] = { " R", "_R", "-R", "(R)", " (R)" };
    for (auto* s : kRs)
    {
        if (name.endsWithIgnoreCase (s))
        {
            outBase = name.dropLastCharacters ((int) std::strlen (s)).trimEnd();
            return true;
        }
    }
    if (name.endsWithIgnoreCase ("R") && ! name.endsWithIgnoreCase ("AR")
        && ! name.endsWithIgnoreCase ("ER") && ! name.endsWithIgnoreCase ("IR")
        && ! name.endsWithIgnoreCase ("OR") && ! name.endsWithIgnoreCase ("UR"))
    {
        outBase = name.dropLastCharacters (1).trimEnd();
        return true;
    }
    return false;
}

// Build groups for the picker.  Tries B2 first; if zero pairs were detected
// AND the device name matches a B1 profile, applies that profile instead.
// Final list is sorted by channel index so groups appear in natural order.
static std::vector<ChannelGroup> computeChannelGroups (const juce::StringArray& names,
                                                        const juce::String& deviceName)
{
    std::vector<ChannelGroup> groups;
    std::vector<bool> consumed ((size_t) names.size(), false);

    // B2 - adjacent L/R name pairing.
    for (int i = 0; i + 1 < names.size(); ++i)
    {
        if (consumed[(size_t) i]) continue;
        juce::String lBase, rBase;
        const bool lOk = stripLeftSuffix  (names[i].trim(),     lBase);
        const bool rOk = stripRightSuffix (names[i + 1].trim(), rBase);
        if (lOk && rOk && lBase.equalsIgnoreCase (rBase) && lBase.isNotEmpty())
        {
            ChannelGroup g;
            g.startIdx = i;
            g.isPair   = true;
            g.label    = juce::String (i + 1) + "/" + juce::String (i + 2)
                          + ":  " + lBase + " (stereo)";
            groups.push_back (std::move (g));
            consumed[(size_t) i]     = true;
            consumed[(size_t) i + 1] = true;
            ++i;
        }
    }

    const bool b2FoundPairs = std::any_of (groups.begin(), groups.end(),
                                            [] (const ChannelGroup& g) { return g.isPair; });

    // B1 - device-profile fallback when B2 found nothing.  Profile match
    // requires the device-name substring AND (when set) the channel count
    // to match - needed because some vendors (e.g. Tascam) ship one ASIO
    // driver for an entire product family, distinguishing models only by
    // the actual channel count the driver enumerates.
    if (! b2FoundPairs && deviceName.isNotEmpty())
    {
        for (const auto& prof : kDeviceProfiles)
        {
            if (! deviceName.containsIgnoreCase (prof.deviceNameContains)) continue;
            if (prof.requiredChannelCount >= 0
                && prof.requiredChannelCount != names.size()) continue;
            for (int idx : prof.pairFirstIndices)
            {
                if (idx < 0 || idx + 1 >= names.size()) continue;
                if (consumed[(size_t) idx] || consumed[(size_t) idx + 1]) continue;
                ChannelGroup g;
                g.startIdx = idx;
                g.isPair   = true;
                g.label    = juce::String (idx + 1) + "/" + juce::String (idx + 2)
                              + ":  " + names[idx].trim()
                              + " + " + names[idx + 1].trim() + " (stereo)";
                groups.push_back (std::move (g));
                consumed[(size_t) idx]     = true;
                consumed[(size_t) idx + 1] = true;
            }
            break;   // first matching profile wins
        }
    }

    // Add remaining mono channels.
    for (int i = 0; i < names.size(); ++i)
    {
        if (consumed[(size_t) i]) continue;
        ChannelGroup g;
        g.startIdx = i;
        g.isPair   = false;
        g.label    = juce::String (i + 1) + ":  " + names[i].trim();
        groups.push_back (std::move (g));
    }

    std::sort (groups.begin(), groups.end(),
               [] (const ChannelGroup& a, const ChannelGroup& b)
               { return a.startIdx < b.startIdx; });
    return groups;
}
} // namespace

// `getInputChannelNames` callback (wired by StandaloneEditor); writes the
// chosen index to APVTS `_inputChannelIdx` + name to `_inputChannelName`;
// sets `_arm` true on selection / false on "Disarm".  When the device has
// zero input channels, shows a single disabled "No channels available" item.
void MixerPage::showInputChannelPicker(int channelId)
{
    using namespace MixerChannelIds;
    const juce::String prefix = prefixFromChannelId(channelId);
    if (prefix.isEmpty()) return;

    juce::PopupMenu menu;
    menu.addSectionHeader (prefix.startsWith("mixer_vox_") ? "Vocal Input" : "Instrument Input");

    juce::StringArray names;
    if (getInputChannelNames) names = getInputChannelNames();

    if (names.isEmpty())
    {
        menu.addItem (1, "No channels available", false, false);
        menu.showMenuAsync (juce::PopupMenu::Options(),
                              juce::ModalCallbackFunction::create ([] (int) {}));
        return;
    }

    // Current selection (for tick + Disarm visibility)
    const int curIdx = (int) (mProcessor.apvts.getRawParameterValue (
                                 prefix + "_inputChannelIdx") != nullptr
                                 ? mProcessor.apvts.getRawParameterValue (
                                      prefix + "_inputChannelIdx")->load()
                                 : -1.f);
    const bool curArmed = (mProcessor.apvts.getRawParameterValue (prefix + "_arm") != nullptr
                              && mProcessor.apvts.getRawParameterValue (prefix + "_arm")->load() > 0.5f);
    const bool curStereo = (mProcessor.apvts.getRawParameterValue (prefix + "_inputChannelStereo") != nullptr
                              && mProcessor.apvts.getRawParameterValue (prefix + "_inputChannelStereo")->load() > 0.5f);

    // B2 + B1: build channel groups (stereo pairs + mono channels) and add
    // them as flat items in channel order.  Item IDs:
    //   100..199 = mono on idx-100
    //   200..299 = stereo pair starting at idx-200
    //   99       = disarm
    const juce::String devName = getInputDeviceName ? getInputDeviceName() : juce::String();
    const auto groups = computeChannelGroups (names, devName);
    for (const auto& g : groups)
    {
        const int itemId = g.isPair ? (200 + g.startIdx) : (100 + g.startIdx);
        // QA-E Task 5 (2026-05-15): tick reflects the current channel
        // selection regardless of arm state.  A user can have a channel
        // assigned for monitoring without arming; the picker should still
        // show which channel is the active assignment.
        const bool ticked = (curIdx == g.startIdx)
                          && (curStereo == g.isPair);
        menu.addItem (juce::PopupMenu::Item (g.label)
                        .setID (itemId)
                        .setTicked (ticked));
    }

    // QA-Fe2: Builder Grid Default (Vox strips only -- Inst records dry-only).
    // Item IDs 300..303; tick shows the locked pick, no tick = auto rule.
    const bool isVoxStrip = prefix.startsWith ("mixer_vox_");
    if (isVoxStrip && onGetGridDefault != nullptr && onSetGridDefault != nullptr)
    {
        const int voxIdx = channelId - kVoxBase;
        const int cur    = onGetGridDefault (voxIdx);
        menu.addSeparator();
        menu.addSectionHeader ("Builder Grid Default");
        static const char* kTakeNames[4] = { "Dry", "Dry Cleaned", "Wet", "Wet Cleaned" };
        for (int t = 0; t < 4; ++t)
            menu.addItem (juce::PopupMenu::Item (kTakeNames[t])
                            .setID (300 + t)
                            .setTicked (cur == t));
    }

    if (curArmed)
    {
        menu.addSeparator();
        menu.addItem (99, "Disarm");
    }

    auto self = juce::Component::SafePointer<MixerPage> (this);
    menu.showMenuAsync (juce::PopupMenu::Options(),
        [self, prefix, channelId, names] (int chosen)
        {
            if (! self || chosen == 0) return;

            // QA-Fe2: grid-default pick -- session lock, no channel change.
            if (chosen >= 300 && chosen < 304)
            {
                if (self->onSetGridDefault)
                    self->onSetGridDefault (channelId - MixerChannelIds::kVoxBase,
                                            chosen - 300);
                return;
            }

            const bool disarm = (chosen == 99);
            // B2 + B1: 100..199 = mono on (chosen - 100); 200..299 = stereo
            // pair starting at (chosen - 200).
            const bool isStereoPick = (chosen >= 200 && chosen < 300);
            const int  newIdx = disarm ? -1
                              : (isStereoPick ? (chosen - 200)
                                              : (chosen - 100));

            if (auto* p = self->mProcessor.apvts.getParameter (prefix + "_inputChannelIdx"))
                p->setValueNotifyingHost (
                    p->getNormalisableRange().convertTo0to1 ((float) newIdx));
            if (auto* p = self->mProcessor.apvts.getParameter (prefix + "_inputChannelStereo"))
                p->setValueNotifyingHost (disarm ? 0.f : (isStereoPick ? 1.f : 0.f));
            // QA-E Task 5 (2026-05-15): picker no longer auto-arms.  Arm state
            // is toggled by left-clicking the Arm LED; the picker (right-click)
            // only changes the channel assignment.  "Disarm" menu item still
            // clears _arm explicitly as an in-picker convenience.
            if (disarm)
            {
                if (auto* p = self->mProcessor.apvts.getParameter (prefix + "_arm"))
                    p->setValueNotifyingHost (0.f);
            }

            // Display name: for stereo pairs, show "13/14: ..." composite.
            juce::String displayName;
            if (! disarm)
            {
                if (isStereoPick && newIdx + 1 < names.size())
                    displayName = juce::String (newIdx + 1) + "/" + juce::String (newIdx + 2)
                                    + ": " + names[newIdx].trim() + " + " + names[newIdx + 1].trim();
                else if (newIdx >= 0 && newIdx < names.size())
                    displayName = names[newIdx];
            }
            self->mProcessor.setInputChannelName (prefix, displayName);
            self->refreshLiveInputStrip (channelId);
        });
}

// R2: keep the strip's Arm-LED tooltip in sync with the persisted state.
// Called after a picker choice + after project load (R3 will hook the load).
void MixerPage::refreshLiveInputStrip (int channelId)
{
    auto* strip = findStripByChannelId (channelId);
    if (! strip) return;
    const juce::String prefix = MixerChannelIds::prefixFromChannelId (channelId);
    const juce::String name   = mProcessor.getInputChannelName (prefix);
    strip->setInputChannelLabel (name);
}

// K-2 (2026-05-05): toggle the noLiveInput flag on an Inst strip - sfizz
// sources (BaySickGuitars / BaySickBasses) hide arm + listen LEDs since the
// engine IS the source.  No-op if the strip at idx doesn't exist.
void MixerPage::setInstStripNoLiveInput (int idx, bool b)
{
    auto it = mInstStrips.find (idx);
    if (it == mInstStrips.end() || ! it->second) return;
    it->second->setNoLiveInput (b);
}

void MixerPage::addInstChannelAtIndex(int idx)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxInstStrips) return;
    if (mInstStrips.count(idx) > 0) return;

    const juce::String prefix = "mixer_inst_" + juce::String(idx);
    const juce::String name   = "Inst " + juce::String(idx + 1);
    mProcessor.ensureInstInsert(idx, name);

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::Inst, juce::Colour(0xFF1C3A8A));  // navy blue
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::instInsert(idx));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onNameChanged = [this](const juce::String&) {
        if (getWidth() > 0) layoutScrollContent();
        if (onAudioStripRenamed) onAudioStripRenamed();
    };
    // I-16 G-9 polish (2026-05-03): missing-from-inception copy-paste oversight
    // -- Vox strips wire onArmRequested -> showInputChannelPicker so clicking
    // the arm LED opens the ASIO input picker.  Inst strips never did, so
    // arming was silent.  Match the Vox path here.
    strip->onArmRequested = [this](int chId) { showInputChannelPicker(chId); };

    mScrollContent->addAndMakeVisible(*strip);
    mInstStrips[idx] = std::move(strip);
    mInstOrder.push_back(idx);
    mNextInstIdx = juce::jmax(mNextInstIdx, idx + 1);

    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
    // G-4 (2026-04-28): notify StandaloneEditor so the matching Inst ribbon
    // page is spawned alongside the strip.
    if (onInstStripAdded) onInstStripAdded(idx);

    // QA-Fb G2-dirty: same page-add dirty fire as addVoxChannelAtIndex above
    // (APVTS dirty hook can't see createAndAddParameter; load path suppressed
    // by ProjectManager::mIgnoreDirty).
    if (mProcessor.onAnyStateChange)
        mProcessor.onAnyStateChange();
}

// ─────────────────────────────────────────────────────────────────────────────
// J-5 (2026-05-03): BaySickRustyDrums strip lifecycle on the Mixer page.
// PluginProcessor::loadBaySickRustyDrumsKit calls these in a batch (one per
// discovered channel); destroyBaySickRustyDrums calls clearAllRustyChannels.
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::addRustyChannelAtIndex (int idx, const juce::String& name)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxRustyStrips) return;
    if (mRustyStrips.count (idx) > 0) return;

    const juce::String prefix = "mixer_rusty_" + juce::String (idx);
    // Backstop: APVTS params + InsertNode are normally created by
    // PluginProcessor::ensureRustyInsert before this is called, but call
    // again here so the function is robust if invoked stand-alone.
    mProcessor.ensureRustyInsert (idx, name);

    // J-5 (2026-05-03): use DrumChannel (no arm/monitor buttons - these
    // strips are sfizz-driven, not live-input).  Drums-red accent matches
    // the existing Drums Bus family + the new RustyDrums Bus.
    auto strip = std::make_unique<MixerTrackStrip> (
        name,
        MixerTrackStrip::StripType::DrumChannel,
        VC::DrumsCol);
    strip->setAutomationPrefix (prefix);
    strip->setApvts (mProcessor.apvts, prefix);
    strip->setChannelId (MixerChannelIds::rustyInsert (idx));
    strip->onAddSendRequested = [this](int chId) { onAddCableRequestedFor (chId); };
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested (id);
    };
    strip->onNameChanged = [this](const juce::String&) {
        if (getWidth() > 0) layoutScrollContent();
        if (onAudioStripRenamed) onAudioStripRenamed();
    };

    mScrollContent->addAndMakeVisible (*strip);
    mRustyStrips[idx] = std::move (strip);
    mRustyOrder.push_back (idx);

    // First strip activates the bus visibility.
    if (! mRustyDrumsBusActive && mRustyDrumsBusStrip)
    {
        mRustyDrumsBusActive = true;
        mRustyDrumsBusStrip->setVisible (true);
    }

    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::removeRustyChannelAtIndex (int idx)
{
    auto it = mRustyStrips.find (idx);
    if (it == mRustyStrips.end()) return;

    if (it->second)
        mScrollContent->removeChildComponent (it->second.get());
    mStripCacheDirty = true;   // perf-audit H2: erase invalidates the strip cache.
    mRustyStrips.erase (it);
    mRustyOrder.erase (std::remove (mRustyOrder.begin(), mRustyOrder.end(), idx),
                       mRustyOrder.end());

    if (getWidth() > 0) layoutScrollContent();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::clearAllRustyChannels()
{
    mStripCacheDirty = true;   // perf-audit H2: bulk clear invalidates the strip cache.
    for (auto& [idx, strip] : mRustyStrips)
        if (strip) mScrollContent->removeChildComponent (strip.get());
    mRustyStrips.clear();
    mRustyOrder.clear();

    // Empty Rusty group → hide the bus strip too.
    if (mRustyDrumsBusActive && mRustyDrumsBusStrip)
    {
        mRustyDrumsBusActive = false;
        mRustyDrumsBusStrip->setVisible (false);
    }

    if (getWidth() > 0) layoutScrollContent();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::clearDynamicStrips()
{
    mStripCacheDirty = true;   // perf-audit H2: bulk clear invalidates the strip cache.
    mLayerStrips.clear();    mLayerTabOrder.clear();
    mBassStrips .clear();    mBassTabOrder .clear();
    mDrumStrips .clear();    mDrumSlotOrder.clear();
    mAudioStrips.clear();    mAudioRowOrder.clear();
    mAuxStrips  .clear();    mAuxOrder     .clear();    mNextAuxIdx  = 0;
    mVoxStrips  .clear();    mVoxOrder     .clear();    mNextVoxIdx  = 0;
    mInstStrips .clear();    mInstOrder    .clear();    mNextInstIdx = 0;
    mRustyStrips.clear();    mRustyOrder   .clear();
    if (mRustyDrumsBusActive && mRustyDrumsBusStrip)
    {
        mRustyDrumsBusActive = false;
        mRustyDrumsBusStrip->setVisible(false);
    }
    mLastSendToCache.clear();
    if (getWidth() > 0) layoutScrollContent();
    if (mCableOverlay) mCableOverlay->repaint();
}

void MixerPage::setAuxStripName (int idx, const juce::String& name)
{
    auto it = mAuxStrips.find (idx);
    if (it == mAuxStrips.end() || ! it->second) return;
    it->second->setTrackName (name);
    if (getWidth() > 0) layoutScrollContent();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::setVoxStripName (int idx, const juce::String& name)
{
    auto it = mVoxStrips.find (idx);
    if (it == mVoxStrips.end() || ! it->second) return;
    it->second->setTrackName (name);
    if (getWidth() > 0) layoutScrollContent();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::setInstStripName (int idx, const juce::String& name)
{
    auto it = mInstStrips.find (idx);
    if (it == mInstStrips.end() || ! it->second) return;
    it->second->setTrackName (name);
    if (getWidth() > 0) layoutScrollContent();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

std::vector<int> MixerPage::getVoxStripIndices() const  { return mVoxOrder; }
std::vector<int> MixerPage::getInstStripIndices() const { return mInstOrder; }

// D.3: re-order strips per a saved project's display order.
void MixerPage::setStripOrder (OrderKind kind, const std::vector<int>& indices)
{
    auto reorder = [&](std::vector<int>& orderVec, auto& stripMap)
    {
        // Build the new order: only indices that exist in stripMap, in the
        // saved sequence.  Then append any registered indices missing from
        // the saved order so no strip is lost.
        std::vector<int> reordered;
        reordered.reserve (stripMap.size());
        std::set<int> seen;
        for (int idx : indices)
        {
            if (stripMap.count (idx) > 0 && seen.insert (idx).second)
                reordered.push_back (idx);
        }
        for (const auto& kv : stripMap)
        {
            if (seen.count (kv.first) == 0)
                reordered.push_back (kv.first);
        }
        orderVec = std::move (reordered);
    };

    switch (kind)
    {
        case OrderKind::Aux:   reorder (mAuxOrder,      mAuxStrips);   break;
        case OrderKind::Vox:   reorder (mVoxOrder,      mVoxStrips);   break;
        case OrderKind::Inst:  reorder (mInstOrder,     mInstStrips);  break;
        case OrderKind::Audio: reorder (mAudioRowOrder, mAudioStrips); break;
    }

    if (getWidth() > 0)
    {
        layoutScrollContent();
        if (mCableOverlay) mCableOverlay->repaint();
    }
}

juce::String MixerPage::getVoxStripName (int idx) const
{
    auto it = mVoxStrips.find (idx);
    return (it != mVoxStrips.end() && it->second) ? it->second->getName()
                                                   : juce::String ("Vox " + juce::String (idx + 1));
}

juce::String MixerPage::getInstStripName (int idx) const
{
    auto it = mInstStrips.find (idx);
    return (it != mInstStrips.end() && it->second) ? it->second->getName()
                                                   : juce::String ("Inst " + juce::String (idx + 1));
}

void MixerPage::addAuxChannelAtIndex(int idx)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxAuxStrips) return;
    if (mAuxStrips.count(idx) > 0) return;   // already exists

    const juce::String prefix = "mixer_aux_" + juce::String(idx);
    const juce::String name   = "Aux " + juce::String(idx + 1);

    // Register the VibeGraph InsertNode + APVTS params (lazy / idempotent).
    mProcessor.ensureAuxInsert(idx, name);

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::Aux, juce::Colour(kEffectsTabPink));
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::auxStrip(idx));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    // Aux strips support inline rename. Name lives on the strip; width may
    // change so re-layout. Also fire onAudioStripRenamed so the Effects page
    // rebuilds its dropdown (reusing the existing callback since its only job
    // is "any mixer strip name changed - refresh dropdowns").
    strip->onNameChanged = [this](const juce::String&) {
        if (getWidth() > 0) layoutScrollContent();
        if (onAudioStripRenamed) onAudioStripRenamed();
    };

    // G-7 (2026-04-29): right-click on the aux strip pops a context menu
    // with a Delete option.  Confirmation prompt fires before the strip is
    // removed; we sweep any strip whose _sendTo / _sendN_to targeted this
    // aux's channel id and reset those to inactive (-1) so audio doesn't
    // get routed to a phantom channel after the aux is gone.
    juce::Component::SafePointer<MixerPage> safeThis (this);
    const int auxChannelId = MixerChannelIds::auxStrip (idx);
    strip->onContextMenuRequested = [safeThis, idx, auxChannelId] (juce::Point<int> screenPos)
    {
        if (! safeThis) return;
        juce::PopupMenu m;
        m.addItem (1, "Delete Aux Strip");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                            juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1)),
            [safeThis, idx, auxChannelId] (int r)
            {
                if (! safeThis || r != 1) return;
                auto* aw = new juce::AlertWindow (
                    "Delete Aux Strip",
                    "Delete this aux strip?  Its Effects Rack will be cleared.\n"
                    "Any sends routed to this aux will be reset to inactive.",
                    juce::AlertWindow::WarningIcon);
                aw->addButton ("Delete", 1);
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create (
                    [safeThis, idx, auxChannelId, aw] (int result)
                    {
                        std::unique_ptr<juce::AlertWindow> own (aw);
                        if (result != 1 || ! safeThis) return;
                        safeThis->deleteAuxStrip (idx, auxChannelId);
                    }), false);
            });
    };

    mScrollContent->addAndMakeVisible(*strip);
    mAuxStrips[idx] = std::move(strip);
    mAuxOrder.push_back(idx);

    // Keep mNextAuxIdx ahead of all used indices
    mNextAuxIdx = juce::jmax(mNextAuxIdx, idx + 1);

    if (getWidth() > 0) resized();

    // Notify Effects page so its dropdown lists the new aux immediately.
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::removeAuxChannel(int idx)
{
    mStripCacheDirty = true;   // perf-audit H2: erase invalidates the strip cache.
    mAuxStrips.erase(idx);
    mAuxOrder.erase(std::remove(mAuxOrder.begin(), mAuxOrder.end(), idx),
                    mAuxOrder.end());
    // Note: InsertNode + APVTS params intentionally preserved so re-creating
    // an aux at the same idx restores its prior settings.
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

// 2026-05-05: orphaned-strip cleanup.  When a tab close fires we destroy the
// page + engine, but pre-this-fix the matching mixer strip was never removed
// from mInstStrips / mVoxStrips / mClipsStrips.  The next addInstChannelAtIndex
// (et al) would then bail at `if (count(idx) > 0) return;` and the user
// couldn't re-add a tab at that slot.  These helpers drop the strip widget +
// the order entry; the underlying VibeGraph InsertNode + APVTS params stay
// alive so re-adding the same idx restores prior settings (matches the Aux
// remove convention above).
void MixerPage::removeInstChannel(int idx)
{
    mStripCacheDirty = true;   // perf-audit H2: erase invalidates the strip cache.
    mInstStrips.erase(idx);
    mInstOrder.erase(std::remove(mInstOrder.begin(), mInstOrder.end(), idx),
                     mInstOrder.end());
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::removeVoxChannel(int idx)
{
    mStripCacheDirty = true;   // perf-audit H2: erase invalidates the strip cache.
    mVoxStrips.erase(idx);
    mVoxOrder.erase(std::remove(mVoxOrder.begin(), mVoxOrder.end(), idx),
                    mVoxOrder.end());
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::removeClipChannel(int idx)
{
    // QA-EffectsReview side-fix (2026-06-06): Clip strips ARE order-tracked in
    // mAudioRowOrder (addAudioChannel push_backs the row; the layout + cable
    // scan iterate it).  The earlier "no separate order vector" note was wrong.
    // Drop BOTH the strip widget AND the order entry -- else the stale row leaves
    // a blank slot in the packed layout (and a re-add duplicates the index).
    // Mirrors removeInstChannel / removeVoxChannel.  The InsertNode + APVTS params
    // stay alive so re-adding the same idx restores prior settings (Aux/Inst/Vox
    // convention).
    mStripCacheDirty = true;   // perf-audit H2: erase invalidates the strip cache.
    mAudioStrips.erase(idx);
    mAudioRowOrder.erase(std::remove(mAudioRowOrder.begin(), mAudioRowOrder.end(), idx),
                         mAudioRowOrder.end());
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

// MIX-05's real cause (QA-L scout): the engine-page strips had NO removal at
// all -- a closed Layer/Bass/Drum tab left its strip in the live mixer, the
// packed layout overlapped after re-adds, and the add-side count(idx) guard
// blocked index reuse.  Widget + order entry only; InsertNode + APVTS params
// persist (house convention above).  All removers fire onAudioStripRenamed so
// the Effects-page dropdown rebuilds on every close/delete path (MIX-07 --
// only deleteSecondaryBus fired it before).
void MixerPage::removeLayerChannel(int pageIndex)
{
    mStripCacheDirty = true;
    mLayerStrips.erase(pageIndex);
    mLayerTabOrder.erase(std::remove(mLayerTabOrder.begin(), mLayerTabOrder.end(), pageIndex),
                         mLayerTabOrder.end());
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::removeBassChannel(int pageIndex)
{
    mStripCacheDirty = true;
    mBassStrips.erase(pageIndex);
    mBassTabOrder.erase(std::remove(mBassTabOrder.begin(), mBassTabOrder.end(), pageIndex),
                        mBassTabOrder.end());
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::removeDrumChannel(int slot)
{
    mStripCacheDirty = true;
    mDrumStrips.erase(slot);
    mDrumSlotOrder.erase(std::remove(mDrumSlotOrder.begin(), mDrumSlotOrder.end(), slot),
                         mDrumSlotOrder.end());
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

// G-7 (2026-04-29): full aux delete via right-click → Delete prompt.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    // Maps a strip APVTS prefix (e.g. "mixer_layer_3") back to its channel id
    // so callers can resolve "what's this param's strip's natural parent?"
    int channelIdFromMixerPrefix (const juce::String& prefix)
    {
        using namespace MixerChannelIds;
        if (prefix == "mixer_master")    return kMaster;
        if (prefix == "mixer_layers")    return kLayersBus;
        if (prefix == "mixer_bass")      return kBassBus;
        if (prefix == "mixer_drums")     return kDrumsBus;
        if (prefix == "mixer_fx")        return kFxBus;
        if (prefix == "mixer_clipsbus")  return kClipsBus;
        if (prefix == "mixer_voxbus")    return kVoxBus;
        if (prefix == "mixer_instbus")   return kInstBus;
        if (prefix == "mixer_voxbus2")   return kVoxBus2;
        if (prefix == "mixer_instbus2")  return kInstBus2;
        if (prefix == "mixer_instbus3")  return kInstBus3;
        if (prefix.startsWith ("mixer_layer_")) return kLayerBase + prefix.substring (12).getIntValue();
        if (prefix.startsWith ("mixer_bass_"))  return kBassBase  + prefix.substring (11).getIntValue();
        if (prefix.startsWith ("mixer_drum_"))  return kDrumBase  + prefix.substring (11).getIntValue();
        if (prefix.startsWith ("mixer_audio_")) return kAudioBase + prefix.substring (12).getIntValue();
        if (prefix.startsWith ("mixer_aux_"))   return kAuxBase   + prefix.substring (10).getIntValue();
        if (prefix.startsWith ("mixer_vox_"))   return kVoxBase   + prefix.substring (10).getIntValue();
        if (prefix.startsWith ("mixer_inst_"))  return kInstBase  + prefix.substring (11).getIntValue();
        return -1;
    }

    // Setter: writes natural value to a RangedAudioParameter via setValueNotifyingHost.
    void writeParamNatural (juce::RangedAudioParameter* rp, float natural)
    {
        const float normalized = rp->getNormalisableRange().convertTo0to1 (natural);
        rp->setValueNotifyingHost (normalized);
    }

    // Predicate: is this parameter id a primary _sendTo or one of the 4
    // additional send destinations (_sendN_to)?
    //
    // Called once per registered parameter by every send sweep, so the four
    // secondary suffixes are built once at first use rather than rebuilt (and
    // freed) on each call -- with lazily-registered per-strip params the walk
    // covers thousands of ids.
    const juce::String& sendSuffix (int n)
    {
        static const juce::String suffixes[4] = { "_send0_to", "_send1_to",
                                                  "_send2_to", "_send3_to" };
        return suffixes[n];
    }

    bool isSendDestId (const juce::String& id, bool& isPrimary)
    {
        if (id.endsWith ("_sendTo")) { isPrimary = true; return true; }
        for (int n = 0; n < 4; ++n)
            if (id.endsWith (sendSuffix (n))) { isPrimary = false; return true; }
        return false;
    }

    // QA-ProjectSave docket 18 (2026-07-26): which bus strips are permanent
    // regardless of what routes to them.  Master is terminal and the FX bus is
    // the default aux parent + a standing send target; the secondary Vox/Inst
    // buses and the RustyDrums bus carry their own explicit activation flags,
    // and a just-created "Add Vox Bus" has nothing routed to it yet -- route-
    // counting those would make the strip the user just asked for not appear.
    bool isAlwaysVisibleBus (int chId)
    {
        using namespace MixerChannelIds;
        return chId == kMaster   || chId == kFxBus
            || chId == kVoxBus2  || chId == kInstBus2
            || chId == kInstBus3 || chId == kRustyDrumsBus;
    }

    // Walks every parameter whose id is a send-destination on a strip prefix.
    // For each match where current value equals targetChId, calls onMatch
    // (which decides what to reset to).  onMatch returns the new natural value.
    void sweepSendsTargeting (juce::AudioProcessor& processor,
                              int targetChId,
                              std::function<float (juce::RangedAudioParameter*,
                                                    int /*stripChId*/,
                                                    bool /*isPrimary*/)> onMatch)
    {
        for (auto* p : processor.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (rp == nullptr) continue;
            const juce::String id = rp->getParameterID();
            bool isPrimary = false;
            if (! isSendDestId (id, isPrimary)) continue;

            const float current = rp->convertFrom0to1 (rp->getValue());
            if ((int) current != targetChId) continue;

            // Extract strip prefix (everything before the trailing "_send..." tag).
            juce::String stripPrefix;
            if (isPrimary)
                stripPrefix = id.dropLastCharacters (juce::String ("_sendTo").length());
            else
            {
                // _sendN_to → strip prefix is everything up to "_sendN_to"
                for (int n = 0; n < 4; ++n)
                {
                    const juce::String suffix = "_send" + juce::String (n) + "_to";
                    if (id.endsWith (suffix))
                    {
                        stripPrefix = id.dropLastCharacters (suffix.length());
                        break;
                    }
                }
            }
            const int stripChId = channelIdFromMixerPrefix (stripPrefix);
            const float newVal = onMatch (rp, stripChId, isPrimary);
            writeParamNatural (rp, newVal);
        }
    }

}

void MixerPage::deleteAuxStrip (int idx, int auxChannelId)
{
    // Reset every send that targeted this aux.
    //  - Primary _sendTo  → natural parent for that strip's channel id
    //  - Secondary _sendN_to → -1 (inactive)
    sweepSendsTargeting (mProcessor, auxChannelId,
        [] (juce::RangedAudioParameter*, int stripChId, bool isPrimary) -> float
        {
            if (isPrimary)
                return (float) MixerChannelIds::defaultSendTo (stripChId);
            return -1.0f;
        });

    // Remove the UI strip.
    removeAuxChannel (idx);

    // Free the audio-graph InsertNode.  Re-creating an aux at the same idx
    // calls ensureAuxInsert which lazily reallocates.
    mProcessor.mVibeGraph.removeInsertNode (VibeGraph::InsertKind::Aux, idx);

    // Routing graph rebuild on the next block picks up the param changes.
    if (onAudioStripRenamed) onAudioStripRenamed();
    repaint();
}

void MixerPage::deleteSecondaryBus (int channelId)
{
    using namespace MixerChannelIds;
    if (channelId != kVoxBus2 && channelId != kInstBus2 && channelId != kInstBus3)
        return;   // primary buses + master never deletable

    const int parentBus = (channelId == kVoxBus2) ? kVoxBus : kInstBus;

    // Reroute any strip whose primary _sendTo targets this bus → parent bus.
    // Reroute any _sendN_to targeting this bus → -1 (inactive) so we don't
    // double-up the parent bus on multiple sends.
    sweepSendsTargeting (mProcessor, channelId,
        [parentBus] (juce::RangedAudioParameter*, int /*stripChId*/, bool isPrimary) -> float
        {
            return isPrimary ? (float) parentBus : -1.0f;
        });

    // Hide the bus strip.  Audio InsertNode stays allocated (always alloc'd
    // in prepare()) - no audio leaks to it because all senders were just
    // rerouted.  Activation flag flip drops it from layout + the route
    // picker submenu list.
    if (channelId == kVoxBus2)
    {
        if (mVoxBus2Strip) mVoxBus2Strip->setVisible (false);
        mVoxBus2Active = false;
    }
    else if (channelId == kInstBus2)
    {
        if (mInstBus2Strip) mInstBus2Strip->setVisible (false);
        mInstBus2Active = false;
    }
    else if (channelId == kInstBus3)
    {
        if (mInstBus3Strip) mInstBus3Strip->setVisible (false);
        mInstBus3Active = false;
    }

    if (getWidth() > 0) layoutScrollContent();
    if (onAudioStripRenamed) onAudioStripRenamed();
    repaint();
}

void MixerPage::addAudioChannel(int row, const juce::String& name)
{
    if (mAudioStrips.count(row) > 0) return;  // already exists

    auto strip = std::make_unique<MixerTrackStrip>(
        name.isNotEmpty() ? name : "Audio " + juce::String(row + 1),
        MixerTrackStrip::StripType::LayerChannel,
        juce::Colour(0xff5a9fbf));  // audio clip cyan-blue

    const juce::String audioPrefix = "mixer_audio_" + juce::String(row);
    strip->setAutomationPrefix(audioPrefix);
    strip->setApvts(mProcessor.apvts, audioPrefix);
    strip->setChannelId(MixerChannelIds::audioInsert(row));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onFaderChanged = [this, row](float db) {
        if (row >= 0 && row < MixerState::kMaxAudioRows)
            mPM.getMixer().audioRowLevel[row] = juce::Decibels::decibelsToGain(db);
    };
    strip->onMuteChanged = [this, row](bool muted) {
        if (row >= 0 && row < MixerState::kMaxAudioRows)
            mPM.getMixer().audioRowMute[row] = muted;
    };
    strip->onNameChanged = [this](const juce::String&) {
        if (onAudioStripRenamed) onAudioStripRenamed();
    };

    mScrollContent->addAndMakeVisible(*strip);
    mAudioStrips[row] = std::move(strip);
    mAudioRowOrder.push_back(row);

    if (getWidth() > 0) resized();
}

void MixerPage::renameChannel(StripKind kind, int pageIdx, const juce::String& newName)
{
    // C.4 follow-up (2026-04-30): dispatch by kind so a Drum rename at
    // pageIdx=0 doesn't accidentally hit a Bass strip at index 0 (and so on
    // for any Layer/Bass/Drum index collision).  Each strip type's map is
    // keyed by its own per-type page index.
    switch (kind)
    {
        case StripKind::Layer:
            if (auto it = mLayerStrips.find(pageIdx); it != mLayerStrips.end())
                it->second->setTrackName(newName);
            break;
        case StripKind::Bass:
            if (auto it = mBassStrips.find(pageIdx); it != mBassStrips.end())
                it->second->setTrackName(newName);
            break;
        case StripKind::Drum:
            if (auto it = mDrumStrips.find(pageIdx); it != mDrumStrips.end())
                it->second->setTrackName(newName);
            break;
        // QA-ClipDrop Task 3 (SC-H, 2026-06-03): Clips/Audio-row strips are keyed
        // in mAudioStrips by their owning Clips-page row index, so a ribbon tab
        // rename can now sync through to the mixer strip the same way Layer/Bass/
        // Drum do (previously left untouched -- no enum entry existed).
        case StripKind::Audio:
            if (auto it = mAudioStrips.find(pageIdx); it != mAudioStrips.end())
                it->second->setTrackName(newName);
            break;
    }
}

juce::String MixerPage::getAudioStripName(int row) const
{
    auto it = mAudioStrips.find(row);
    if (it != mAudioStrips.end())
        return it->second->getName();
    return "Audio " + juce::String(row + 1);
}

std::vector<int> MixerPage::getAuxStripIndices() const
{
    return mAuxOrder;
}

juce::String MixerPage::getAuxStripName(int idx) const
{
    auto it = mAuxStrips.find(idx);
    if (it != mAuxStrips.end())
        return it->second->getName();
    return "Aux " + juce::String(idx + 1);
}

std::vector<int> MixerPage::getDrumStripIndices() const
{
    std::vector<int> out;
    out.reserve (mDrumStrips.size());
    for (auto& kv : mDrumStrips)
        out.push_back (kv.first);
    return out;   // std::map iterates keys in ascending order - stable
}

juce::String MixerPage::getDrumStripName(int slot) const
{
    auto it = mDrumStrips.find (slot);
    if (it != mDrumStrips.end())
        return it->second->getName();
    return "Drum " + juce::String (slot + 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Callbacks
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::wireMasterCallbacks()
{
    mMasterStrip->onFaderDragStarted = [this] { mMixerStateBefore = mPM.getMixer(); };
    mMasterStrip->onFaderDragEnded   = [this]
    {
        if (!mUndoCtx.isValid()) return;
        MixerState after = mPM.getMixer();
        mUndoCtx.perform(new MixerStateAction("Master Level",
            mMixerStateBefore, after,
            [this](const MixerState& s) { applyMixerSnapshot(s); }), "Master Level");
    };
    mMasterStrip->onFaderChanged = [this](float db)
    {
        mPM.getMixer().masterLevel = juce::Decibels::decibelsToGain(db, -60.0f);
    };
    mMasterStrip->onPanDragStarted = [this] { mMixerStateBefore = mPM.getMixer(); };
    mMasterStrip->onPanDragEnded   = [this]
    {
        if (!mUndoCtx.isValid()) return;
        MixerState after = mPM.getMixer();
        mUndoCtx.perform(new MixerStateAction("Master Pan",
            mMixerStateBefore, after,
            [this](const MixerState& s) { applyMixerSnapshot(s); }), "Master Pan");
    };
    mMasterStrip->onPanChanged = [this](float pan) { mPM.getMixer().masterPan = pan; };
    mMasterStrip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
}

void MixerPage::wireBusCallbacks(MixerTrackStrip* strip,
                                  float& levelRef, float& panRef,
                                  bool& muteRef, bool& soloRef)
{
    strip->onFaderDragStarted = [this] { mMixerStateBefore = mPM.getMixer(); };
    strip->onFaderDragEnded   = [this]
    {
        if (!mUndoCtx.isValid()) return;
        MixerState after = mPM.getMixer();
        mUndoCtx.perform(new MixerStateAction("Fader",
            mMixerStateBefore, after,
            [this](const MixerState& s) { applyMixerSnapshot(s); }), "Fader");
    };
    strip->onFaderChanged = [&levelRef](float db) {
        levelRef = juce::Decibels::decibelsToGain(db, -60.0f);
    };
    strip->onPanDragStarted = [this] { mMixerStateBefore = mPM.getMixer(); };
    strip->onPanDragEnded   = [this]
    {
        if (!mUndoCtx.isValid()) return;
        MixerState after = mPM.getMixer();
        mUndoCtx.perform(new MixerStateAction("Pan",
            mMixerStateBefore, after,
            [this](const MixerState& s) { applyMixerSnapshot(s); }), "Pan");
    };
    strip->onPanChanged   = [&panRef](float pan) { panRef = pan; };
    strip->onMuteChanged  = [this, &muteRef](bool m)
    {
        if (mUndoCtx.isValid()) {
            MixerState before = mPM.getMixer();
            muteRef = m;
            mUndoCtx.perform(new MixerStateAction("Mute",
                before, mPM.getMixer(),
                [this](const MixerState& s) { applyMixerSnapshot(s); }), "Mute");
        } else {
            muteRef = m;
        }
    };
    strip->onSoloChanged  = [this, &soloRef](bool s)
    {
        if (mUndoCtx.isValid()) {
            MixerState before = mPM.getMixer();
            soloRef = s;
            mUndoCtx.perform(new MixerStateAction("Solo",
                before, mPM.getMixer(),
                [this](const MixerState& s2) { applyMixerSnapshot(s2); }), "Solo");
        } else {
            soloRef = s;
        }
    };
    strip->onFXClicked    = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Model sync
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::syncFromModel()
{
    const auto& mx = mPM.getMixer();

    mMasterStrip->setFaderDb(juce::Decibels::gainToDecibels(mx.masterLevel, -60.0f));
    mMasterStrip->setPan(mx.masterPan);

    mLayersBusStrip->setFaderDb(juce::Decibels::gainToDecibels(mx.layersLevel, -60.0f));
    mLayersBusStrip->setPan(mx.layersPan);
    mLayersBusStrip->setMuted(mx.layersMute);
    mLayersBusStrip->setSoloed(mx.layersSolo);

    mBassBusStrip->setFaderDb(juce::Decibels::gainToDecibels(mx.bassLevel, -60.0f));
    mBassBusStrip->setPan(mx.bassPan);
    mBassBusStrip->setMuted(mx.bassMute);
    mBassBusStrip->setSoloed(mx.bassSolo);

    mDrumsBusStrip->setFaderDb(juce::Decibels::gainToDecibels(mx.drumsLevel, -60.0f));
    mDrumsBusStrip->setPan(mx.drumsPan);
    mDrumsBusStrip->setMuted(mx.drumsMute);
    mDrumsBusStrip->setSoloed(mx.drumsSolo);

    mAudioClipsBusStrip->setFaderDb(juce::Decibels::gainToDecibels(mx.audioClipsBusLevel, -60.0f));
    mAudioClipsBusStrip->setPan(mx.audioClipsBusPan);
    mAudioClipsBusStrip->setMuted(mx.audioClipsBusMute);
    mAudioClipsBusStrip->setSoloed(mx.audioClipsBusSolo);

    for (auto& [slot, strip] : mDrumStrips)
    {
        if (slot < MAX_DRUM_ROWS)
        {
            strip->setFaderDb(juce::Decibels::gainToDecibels(mx.drumSlotLevel[slot], -60.0f));
            strip->setPan(mx.drumSlotPan[slot]);
        }
    }
}

// 5F-4a Batch 6: push MixerState into APVTS so InsertNode sees correct values.
// Called once at constructor end (after syncFromModel). Uses setValueNotifyingHost
// so attachments update and the audio thread sees the new values immediately.
void MixerPage::syncApvtsFromMixerState()
{
    const auto& mx = mPM.getMixer();
    auto writeFloat = [&](const juce::String& id, float v)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                mProcessor.apvts.getParameter(id)))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(v));
    };
    auto writeBool = [&](const juce::String& id, bool v)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                mProcessor.apvts.getParameter(id)))
            p->setValueNotifyingHost(v ? 1.f : 0.f);
    };

    // Master
    writeFloat("mixer_master_level", juce::Decibels::gainToDecibels(mx.masterLevel, -60.f));
    writeFloat("mixer_master_pan",   mx.masterPan);

    // Buses
    writeFloat("mixer_layers_level", juce::Decibels::gainToDecibels(mx.layersLevel, -60.f));
    writeFloat("mixer_layers_pan",   mx.layersPan);
    writeBool ("mixer_layers_mute",  mx.layersMute);
    writeBool ("mixer_layers_solo",  mx.layersSolo);
    writeFloat("mixer_bass_level",   juce::Decibels::gainToDecibels(mx.bassLevel, -60.f));
    writeFloat("mixer_bass_pan",     mx.bassPan);
    writeBool ("mixer_bass_mute",    mx.bassMute);
    writeBool ("mixer_bass_solo",    mx.bassSolo);
    writeFloat("mixer_drums_level",  juce::Decibels::gainToDecibels(mx.drumsLevel, -60.f));
    writeFloat("mixer_drums_pan",    mx.drumsPan);
    writeBool ("mixer_drums_mute",   mx.drumsMute);
    writeBool ("mixer_drums_solo",   mx.drumsSolo);
    writeFloat("mixer_clipsbus_level", juce::Decibels::gainToDecibels(mx.audioClipsBusLevel, -60.f));
    writeFloat("mixer_clipsbus_pan",   mx.audioClipsBusPan);
    writeBool ("mixer_clipsbus_mute",  mx.audioClipsBusMute);
    writeBool ("mixer_clipsbus_solo",  mx.audioClipsBusSolo);

    // Drum slot inserts
    for (int slot = 0; slot < MAX_DRUM_ROWS; ++slot)
    {
        const juce::String p = "mixer_drum_" + juce::String(slot);
        writeFloat(p + "_level", juce::Decibels::gainToDecibels(mx.drumSlotLevel[slot], -60.f));
        writeFloat(p + "_pan",   mx.drumSlotPan[slot]);
    }

    // Audio row inserts
    for (int row = 0; row < MixerState::kMaxAudioRows; ++row)
    {
        const juce::String p = "mixer_audio_" + juce::String(row);
        writeFloat(p + "_level", juce::Decibels::gainToDecibels(mx.audioRowLevel[row], -60.f));
        writeBool (p + "_mute",  mx.audioRowMute[row]);
    }
}

void MixerPage::applyMixerSnapshot(const MixerState& state)
{
    mPM.getMixer() = state;
    syncFromModel();
}

void MixerPage::setUndoContext(const UndoContext& ctx)
{
    mUndoCtx = ctx;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::timerCallback()
{
    // 2026-05-02: meter polling moved to onVBlank() (vblank-locked).  This
    // 30 Hz timer now only handles cable-overlay scroll detection + _sendTo
    // change detection + (via base class) flash decay -- none of which need
    // monitor-refresh precision.

    // QA-Eg: scroll-driven repaint kept here even though onVBlank conditionally
    // pumps a repaint when any strip's displayed peak changed (delta-gated
    // telemetry-driven cable animation).  This immediate repaint on scroll
    // avoids one frame of stale-cable visuals during fast scroll bursts.
    const int curViewportX = mViewport ? mViewport->getViewPositionX() : 0;
    if (mCableOverlay && curViewportX != mLastViewportX)
    {
        mLastViewportX = curViewportX;
        mCableOverlay->repaint();
    }

    // Detect _sendTo changes on any strip and re-layout so strips visually
    // move between bus groups when their main-out cable is rerouted.
    {
        using namespace MixerChannelIds;
        auto check = [&](int chId, const juce::String& prefix) -> bool
        {
            if (auto* p = mProcessor.apvts.getRawParameterValue(prefix + "_sendTo"))
            {
                int cur = (int)p->load();
                auto it = mLastSendToCache.find(chId);
                if (it == mLastSendToCache.end() || it->second != cur)
                {
                    mLastSendToCache[chId] = cur;
                    return true;
                }
            }
            return false;
        };

        bool anyChange = false;
        for (int k : mLayerTabOrder)
            anyChange |= check(layerInsert(k), "mixer_layer_" + juce::String(k));
        for (int k : mBassTabOrder)
            anyChange |= check(bassInsert(k), "mixer_bass_" + juce::String(k));
        for (int k : mDrumSlotOrder)
            anyChange |= check(drumInsert(k), "mixer_drum_" + juce::String(k));
        for (int k : mAudioRowOrder)
            anyChange |= check(audioInsert(k), "mixer_audio_" + juce::String(k));
        for (int k : mAuxOrder)
            anyChange |= check(auxStrip(k), "mixer_aux_" + juce::String(k));
        // G-6 (2026-04-29): Vox + Inst inserts were missing from this scan
        // pre-G-6, so cable-rerouting a Vox tab to a different bus didn't
        // trigger a re-layout (strip stayed visually behind the wrong bus).
        for (int k : mVoxOrder)
            anyChange |= check(voxInsert(k), "mixer_vox_" + juce::String(k));
        for (int k : mInstOrder)
            anyChange |= check(instInsert(k), "mixer_inst_" + juce::String(k));
        // J-5 (2026-05-03): Rusty inserts must be in this scan so any send
        // cable change re-runs layout (re-buckets the strip to its new dest).
        for (int k : mRustyOrder)
            anyChange |= check(rustyInsert(k), "mixer_rusty_" + juce::String(k));

        if (anyChange)
        {
            layoutScrollContent();
            if (mCableOverlay) mCableOverlay->repaint();   // strips moved → cable sockets moved
            // Notify listeners (StandaloneEditor wires this to the Effects
            // dropdown) so strips rerouted to another group show up in the
            // correct section.
            if (onSendToChanged) onSendToChanged();
        }
    }
}

// 2026-05-02: vblank-locked meter feed.  Drains every per-strip peak atomic
// (running max accumulated by the audio thread since the last vblank) and
// pushes the value to the matching DBFSMeter, which then runs its own
// vblank ballistics + repaint.  exchange-and-reset semantics: each atomic
// is replaced with -inf so the next audio block starts a fresh max window.
void MixerPage::onVBlank()
{
    constexpr float kNegInf = -std::numeric_limits<float>::infinity();

    // QA-RustyMeter part 2 (2026-05-30): drainStereoBus now also feeds the bus
    // split meter's scrolling RMS top half via drainBusRmsDbStereo(busChId).
    // Master passes kMaster -> returns -inf (Full layout, no RMS) -> harmless
    // setRmsStereo no-op on a Full meter.
    auto drainStereoBus = [this] (MixerTrackStrip* strip, int busChId,
                                   std::atomic<float>& l,
                                   std::atomic<float>& r)
    {
        if (! strip) return;
        const float vL = l.exchange (kNegInf, std::memory_order_relaxed);
        const float vR = r.exchange (kNegInf, std::memory_order_relaxed);
        strip->setStereoLevel (vL, vR);
        const auto [rmL, rmR] = mProcessor.drainBusRmsDbStereo (busChId);
        strip->setRmsStereo (rmL, rmR);
    };

    using namespace MixerChannelIds;   // bus-id constants for drainBusRmsDbStereo

    drainStereoBus (mMasterStrip       .get(), kMaster,        mProcessor.mMasterPeakDbL,        mProcessor.mMasterPeakDbR);
    // QA-RustyMeter Task 3 (2026-05-30): feed the master LUFS box (M/S/I); the
    // box displays the user-selected mode.  Master strip only.
    if (mMasterStrip)
        mMasterStrip->setMasterLufs (mProcessor.getMasterLufs (0),
                                     mProcessor.getMasterLufs (1),
                                     mProcessor.getMasterLufs (2));
    drainStereoBus (mLayersBusStrip    .get(), kLayersBus,     mProcessor.mLayersPeakDbL,        mProcessor.mLayersPeakDbR);
    drainStereoBus (mBassBusStrip      .get(), kBassBus,       mProcessor.mBassPeakDbL,          mProcessor.mBassPeakDbR);
    drainStereoBus (mDrumsBusStrip     .get(), kDrumsBus,      mProcessor.mDrumsPeakDbL,         mProcessor.mDrumsPeakDbR);
    drainStereoBus (mFXBusStrip        .get(), kFxBus,         mProcessor.mFxBusPeakDbL,         mProcessor.mFxBusPeakDbR);
    drainStereoBus (mAudioClipsBusStrip.get(), kClipsBus,      mProcessor.mAudioClipsBusPeakDbL, mProcessor.mAudioClipsBusPeakDbR);
    drainStereoBus (mVoxBusStrip       .get(), kVoxBus,        mProcessor.mVoxBusPeakDbL,        mProcessor.mVoxBusPeakDbR);
    drainStereoBus (mInstBusStrip      .get(), kInstBus,       mProcessor.mInstBusPeakDbL,       mProcessor.mInstBusPeakDbR);
    drainStereoBus (mRustyDrumsBusStrip.get(), kRustyDrumsBus, mProcessor.mRustyDrumsBusPeakDbL, mProcessor.mRustyDrumsBusPeakDbR);   // J-7b
    // QA-Eg: cable overlay reads each strip's cached peak (MixerTrackStrip::
    // getCurrentPeakDb) for telemetry-driven alpha + warning-color animation.
    // Repaint here so cable updates land in the same vblank as the strip
    // meters they're sourced from -- no race between cable read and strip drain.

    // QA-AudioMeters (2026-05-24): unified per-insert drain via the new
    // VibeSynthProcessor::drainInsertPeakDbStereo accessor.  Reads + exchange-
    // resets the m<Kind>InsertPeakDb*L/R[index] mirror that drainMeterAtomicsForUI
    // populates from VibeGraph's per-kind public-member arrays.  Audio kind
    // shares this path (no more inline mAudioRowPeakDb*L/R exchange-reset).
    auto drainStereoInsert = [&] (VibeGraph::InsertKind kind, int idx, MixerTrackStrip* strip)
    {
        if (! strip) return;
        const auto [pkL, pkR] = mProcessor.drainInsertPeakDbStereo (kind, idx);
        strip->setStereoLevel (pkL, pkR);
        // QA-RustyMeter: feed the split meter's scrolling RMS top half.
        const auto [rmL, rmR] = mProcessor.drainInsertRmsDbStereo (kind, idx);
        strip->setRmsStereo (rmL, rmR);
    };

    for (auto& [pageIdx, strip] : mLayerStrips)
        drainStereoInsert (VibeGraph::InsertKind::Layer, pageIdx, strip.get());
    for (auto& [pageIdx, strip] : mBassStrips)
        drainStereoInsert (VibeGraph::InsertKind::Bass, pageIdx, strip.get());
    for (auto& [slot, strip] : mDrumStrips)
        drainStereoInsert (VibeGraph::InsertKind::Drum, slot, strip.get());
    for (auto& [row, strip] : mAudioStrips)
        drainStereoInsert (VibeGraph::InsertKind::Audio, row, strip.get());
    for (auto& [idx, strip] : mAuxStrips)
        drainStereoInsert (VibeGraph::InsertKind::Aux, idx, strip.get());
    for (auto& [idx, strip] : mVoxStrips)
        drainStereoInsert (VibeGraph::InsertKind::Vox, idx, strip.get());
    for (auto& [idx, strip] : mInstStrips)
        drainStereoInsert (VibeGraph::InsertKind::Inst, idx, strip.get());
    // J-5 (2026-05-03): per-Rusty-strip peak meter drain.
    for (auto& [idx, strip] : mRustyStrips)
        drainStereoInsert (VibeGraph::InsertKind::Rusty, idx, strip.get());

    if (mVoxBus2Strip)  drainStereoBus (mVoxBus2Strip .get(), kVoxBus2,  mProcessor.mVoxBus2PeakDbL,  mProcessor.mVoxBus2PeakDbR);
    if (mInstBus2Strip) drainStereoBus (mInstBus2Strip.get(), kInstBus2, mProcessor.mInstBus2PeakDbL, mProcessor.mInstBus2PeakDbR);
    if (mInstBus3Strip) drainStereoBus (mInstBus3Strip.get(), kInstBus3, mProcessor.mInstBus3PeakDbL, mProcessor.mInstBus3PeakDbR);

    // QA-Eg (2026-05-24): cable overlay repaint is delta-gated against a
    // snapshot of every strip's displayed peak dB from the previous vblank.
    // At idle (every meter at floor) the snapshot is stable -> no repaint ->
    // the transparent overlay's clear-then-redraw cycle (the dying-lightbulb
    // flicker source) is skipped entirely.  During playback every strip moves
    // each frame -> repaint every frame -> smooth telemetry animation as
    // before.  Threshold is 0.1 dB -- small enough that any user-visible
    // alpha change in cableTelemetry's -60..0 dB mapping crosses it.
    //
    // QA-Eg fix-up (perf-audit M1): use the mPeakSnapshotScratch member as
    // the per-frame build buffer + std::swap with mLastPeakSnapshot at end so
    // both vectors retain their allocator capacity across frames.  No heap
    // alloc on the UI vblank path after warm-up.
    constexpr float kCableRepaintEpsilonDb = 0.1f;
    mPeakSnapshotScratch.clear();

    auto pushPeak = [&] (const MixerTrackStrip* strip)
    {
        if (strip) mPeakSnapshotScratch.push_back(strip->getCurrentPeakDb());
    };

    pushPeak(mMasterStrip       .get());
    pushPeak(mLayersBusStrip    .get());
    pushPeak(mBassBusStrip      .get());
    pushPeak(mDrumsBusStrip     .get());
    pushPeak(mFXBusStrip        .get());
    pushPeak(mAudioClipsBusStrip.get());
    pushPeak(mVoxBusStrip       .get());
    pushPeak(mInstBusStrip      .get());
    pushPeak(mRustyDrumsBusStrip.get());
    pushPeak(mVoxBus2Strip      .get());
    pushPeak(mInstBus2Strip     .get());
    pushPeak(mInstBus3Strip     .get());

    for (auto& kv : mLayerStrips) pushPeak(kv.second.get());
    for (auto& kv : mBassStrips)  pushPeak(kv.second.get());
    for (auto& kv : mDrumStrips)  pushPeak(kv.second.get());
    for (auto& kv : mAudioStrips) pushPeak(kv.second.get());
    for (auto& kv : mAuxStrips)   pushPeak(kv.second.get());
    for (auto& kv : mVoxStrips)   pushPeak(kv.second.get());
    for (auto& kv : mInstStrips)  pushPeak(kv.second.get());
    for (auto& kv : mRustyStrips) pushPeak(kv.second.get());

    bool cableDirty = (mPeakSnapshotScratch.size() != mLastPeakSnapshot.size());
    if (! cableDirty)
    {
        for (size_t i = 0; i < mPeakSnapshotScratch.size(); ++i)
        {
            if (std::abs(mPeakSnapshotScratch[i] - mLastPeakSnapshot[i]) > kCableRepaintEpsilonDb)
            {
                cableDirty = true;
                break;
            }
        }
    }

    std::swap(mPeakSnapshotScratch, mLastPeakSnapshot);

    if (cableDirty && mCableOverlay)
        mCableOverlay->repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────
// QA-RustyMeter Task 5 (2026-05-30): bus collapse/expand view state.  The
// per-bus _collapsed APVTS bool is the source of truth (it persists with the
// project + restores on load).  isBusCollapsed reads it; onBusCollapseToggled
// (fired by a bus strip's arrow) flips it + relayouts.
bool MixerPage::isBusCollapsed (int channelId) const
{
    const juce::String prefix = MixerChannelIds::prefixFromChannelId (channelId);
    if (prefix.isEmpty()) return false;
    if (auto* v = mProcessor.apvts.getRawParameterValue (prefix + "_collapsed"))
        return v->load() > 0.5f;
    return false;
}

void MixerPage::onBusCollapseToggled (int channelId)
{
    const juce::String prefix = MixerChannelIds::prefixFromChannelId (channelId);
    if (prefix.isEmpty()) return;
    if (auto* p = mProcessor.apvts.getParameter (prefix + "_collapsed"))
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);   // flip
    layoutScrollContent();   // re-reads the param: hide/show members + close the gap
}

void MixerPage::layoutScrollContent()
{
    using namespace MixerChannelIds;

    // QA-Eg fix-up (perf-audit H2): layoutScrollContent is called after every
    // structural change (add, remove, reorder, lazy bus add).  Use the entry
    // point as the broad invalidation hook so the strip cache rebuilds on
    // next lookup.  Erase sites also set the flag explicitly so a paint
    // between erase and the next layout pass doesn't see a dangling pointer.
    mStripCacheDirty = true;

    const int stripH    = juce::jmax(200, mScrollContent->getHeight());
    const int busW      = MixerTrackStrip::widthForType(MixerTrackStrip::StripType::Bus);
    const int layerW    = MixerTrackStrip::widthForType(MixerTrackStrip::StripType::LayerChannel);
    const int bassW     = MixerTrackStrip::widthForType(MixerTrackStrip::StripType::BassChannel);
    const int drumW     = MixerTrackStrip::widthForType(MixerTrackStrip::StripType::DrumChannel);
    const int audioW    = MixerTrackStrip::widthForType(MixerTrackStrip::StripType::LayerChannel);
    const int auxW      = MixerTrackStrip::widthForType(MixerTrackStrip::StripType::Aux);
    const int kGroupSep = 14;   // wider gap between bus groups
    const int kDRLabelW = 28;   // width of Direct Routing vertical-text label

    auto getSendTo = [&](const juce::String& prefix, int fallback) -> int {
        if (auto* p = mProcessor.apvts.getRawParameterValue(prefix + "_sendTo"))
            return (int)p->load();
        return fallback;
    };

    // Bucket each strip by destination channel id.
    struct Member { MixerTrackStrip* strip; int width; int chId; int dest; };
    std::map<int, std::vector<Member>> buckets;

    auto bucketPush = [&](MixerTrackStrip* s, int chId, int w, const juce::String& prefix) {
        int dest = getSendTo(prefix, defaultSendTo(chId));
        buckets[dest].push_back({ s, w, chId, dest });
    };

    for (int k : mLayerTabOrder)
    {
        auto it = mLayerStrips.find(k);
        if (it != mLayerStrips.end())
            bucketPush(it->second.get(), layerInsert(k), layerW,
                       "mixer_layer_" + juce::String(k));
    }
    for (int k : mBassTabOrder)
    {
        auto it = mBassStrips.find(k);
        if (it != mBassStrips.end())
            bucketPush(it->second.get(), bassInsert(k), bassW,
                       "mixer_bass_" + juce::String(k));
    }
    for (int k : mDrumSlotOrder)
    {
        auto it = mDrumStrips.find(k);
        if (it != mDrumStrips.end())
            bucketPush(it->second.get(), drumInsert(k), drumW,
                       "mixer_drum_" + juce::String(k));
    }
    for (int k : mAudioRowOrder)
    {
        auto it = mAudioStrips.find(k);
        if (it != mAudioStrips.end())
            bucketPush(it->second.get(), audioInsert(k), audioW,
                       "mixer_audio_" + juce::String(k));
    }
    for (int k : mAuxOrder)
    {
        auto it = mAuxStrips.find(k);
        if (it != mAuxStrips.end())
            bucketPush(it->second.get(), auxStrip(k), auxW,
                       "mixer_aux_" + juce::String(k));
    }
    // R1 (2026-04-23): Vox + Inst strips share Aux's width since they're the
    // same physical size class.  No dedicated bus strip yet - they'll group
    // under the FX bus for layout purposes in R1.  Real bus-group routing
    // lands in R3 when audio actually flows through the Vox/Inst buses.
    for (int k : mVoxOrder)
    {
        auto it = mVoxStrips.find(k);
        if (it != mVoxStrips.end())
            bucketPush(it->second.get(), voxInsert(k), auxW,
                       "mixer_vox_" + juce::String(k));
    }
    for (int k : mInstOrder)
    {
        auto it = mInstStrips.find(k);
        if (it != mInstStrips.end())
            bucketPush(it->second.get(), instInsert(k), auxW,
                       "mixer_inst_" + juce::String(k));
    }
    // J-5 (2026-05-03): BaySickRustyDrums strips bucket together (default
    // dest = kRustyDrumsBus) so they appear as a single group on the Mixer
    // page next to the RustyDrums Bus strip.  drumW width matches the
    // DrumChannel strip type used for these strips.
    for (int k : mRustyOrder)
    {
        auto it = mRustyStrips.find(k);
        if (it != mRustyStrips.end())
            bucketPush(it->second.get(), rustyInsert(k), drumW,
                       "mixer_rusty_" + juce::String(k));
    }

    int x = kSepW;

    // Neon lines recorded during layout, drawn later by ScrollContent::paintOverChildren
    std::vector<NeonLine> neon;

    // Lay out a single group's members FLUSH (no gap between strips).
    // Paints a bright neon line at busRight→firstMember, dimmed lines between adjacent members.
    // accent = bus's natural color (drives both strip accent + neon color).
    auto layoutGroup = [&](const std::vector<Member>& members, int busRight,
                           juce::Colour accent, int destId)
    {
        for (size_t i = 0; i < members.size(); ++i)
        {
            const auto& m = members[i];
            m.strip->setBounds(x, 0, m.width, stripH);
            m.strip->setAccentColor(pickStripColor(m.chId, destId));

            // Neon line at strip's left edge (covers where bus/prev-strip meets this strip)
            const bool bright = (i == 0);  // first member: bright line flush against bus
            neon.push_back({ x, 0, stripH, accent, bright });

            x += m.width;
        }
    };

    // ── Direct Routing group (strips main-out to Master) ─────────────
    const auto drIt = buckets.find(kMaster);
    const bool hasDR = (drIt != buckets.end()) && !drIt->second.empty();
    if (hasDR && mDirectRoutingLabel)
    {
        mDirectRoutingLabel->setVisible(true);
        mDirectRoutingLabel->setBounds(x, 0, kDRLabelW, stripH);
        x += kDRLabelW;
        // DR strips keep their natural color. Neon is VC::Accent to match the DR label.
        layoutGroup(drIt->second, x, VC::Accent, kMaster);
        x += kGroupSep;
    }
    else if (mDirectRoutingLabel)
    {
        mDirectRoutingLabel->setVisible(false);
    }

    // Per-group helper: lay out the bus strip + its members.
    auto laidOutBus = [&](MixerTrackStrip& busStrip, int busChId, juce::Colour busAccent)
    {
        auto it = buckets.find(busChId);
        const bool hasMembers = (it != buckets.end() && ! it->second.empty());

        // QA-ProjectSave docket 18 (2026-07-26): an empty bus is hidden and
        // consumes no width, so its group gap closes instead of leaving a hole.
        // `hasMembers` is the whole test: buckets are keyed by each EXISTING
        // strip's _sendTo, so re-pointing a main-out at a bus re-buckets that
        // strip INTO the bus's group -- for a bus, "something routes here" and
        // "has members" are the same condition.  (Sends never enter into it;
        // _sendN_to only ever lands on an aux strip.)
        //
        // The flag-gated buses opt out: they are created by explicit user
        // action and must appear immediately, before anything is routed to them.
        if (! hasMembers && ! isAlwaysVisibleBus (busChId))
        {
            busStrip.setVisible (false);
            return;
        }
        busStrip.setVisible (true);
        busStrip.setBounds(x, 0, busW, stripH);
        busStrip.setAccentColor(busAccent);
        // QA-RustyMeter Task 5 (2026-05-30): wire the collapse arrow (idempotent)
        // + apply the persisted collapse state.  Collapsed -> hide this bus's
        // member strips + don't advance x, so the group's gap closes.  Arrow
        // greyed/disabled when the bus has no members.
        busStrip.onCollapseToggled = [this](int chId) { onBusCollapseToggled(chId); };
        x += busW;
        const bool collapsed  = hasMembers && isBusCollapsed(busChId);
        busStrip.setCollapseEnabled(hasMembers);
        busStrip.setCollapsed(collapsed);
        if (hasMembers)
        {
            if (collapsed)
                for (auto& m : it->second) { if (m.strip) m.strip->setVisible(false); }
            else
            {
                for (auto& m : it->second) { if (m.strip) m.strip->setVisible(true); }
                layoutGroup(it->second, x, busAccent, busChId);
            }
        }
        x += kGroupSep;
    };

    // ── FX Bus group ─────────────────────────────────────────────────
    mFXBusStrip->setBounds(x, 0, busW, stripH);
    mFXBusStrip->setAccentColor(juce::Colour(kEffectsTabPink));
    // QA-RustyMeter Task 5 (2026-05-30): FX bus collapse (its own member group
    // only; the aux-to-aux chains below are separate aux groups, left as-is).
    mFXBusStrip->onCollapseToggled = [this](int chId) { onBusCollapseToggled(chId); };
    x += busW;
    {
        auto it = buckets.find(kFxBus);
        const bool hasMembers = (it != buckets.end() && ! it->second.empty());
        const bool collapsed  = hasMembers && isBusCollapsed(kFxBus);
        mFXBusStrip->setCollapseEnabled(hasMembers);
        mFXBusStrip->setCollapsed(collapsed);
        if (hasMembers)
        {
            if (collapsed)
                for (auto& m : it->second) { if (m.strip) m.strip->setVisible(false); }
            else
            {
                for (auto& m : it->second) { if (m.strip) m.strip->setVisible(true); }
                layoutGroup(it->second, x, juce::Colour(kEffectsTabPink), kFxBus);
            }
        }
    }
    // Aux-to-aux main-out chains still live visually in the FX family
    for (auto& [dst, members] : buckets)
    {
        if (dst >= kAuxBase && dst < kAuxBase + 16)
            layoutGroup(members, x, juce::Colour(kEffectsTabPink), dst);
    }
    x += kGroupSep;

    // Clips Bus sits between FX and the instrument buses - matches Builder tab color.
    laidOutBus(*mAudioClipsBusStrip,  kClipsBus,  VC::Warm);

    // R3.5 (2026-04-23): Vox + Inst BUS strips alongside Clips Bus.  Same
    // shape as other buses (full rack/EQ/fader DSP applied in PluginProcessor
    // before the accumulator drains to Master).
    laidOutBus(*mVoxBusStrip,  kVoxBus,  juce::Colour(0xFF0FAFA5));
    // G-6 (2026-04-29): Vox Bus 2 (when active) sits next to Vox Bus.
    if (mVoxBus2Active && mVoxBus2Strip)
        laidOutBus(*mVoxBus2Strip, kVoxBus2, juce::Colour(0xFF0FAFA5));
    laidOutBus(*mInstBusStrip, kInstBus, juce::Colour(0xFF1C3A8A));
    // G-6 (2026-04-29): Inst Bus 2 + 3 (when active) sit next to Inst Bus.
    if (mInstBus2Active && mInstBus2Strip)
        laidOutBus(*mInstBus2Strip, kInstBus2, juce::Colour(0xFF1C3A8A));
    if (mInstBus3Active && mInstBus3Strip)
        laidOutBus(*mInstBus3Strip, kInstBus3, juce::Colour(0xFF1C3A8A));

    laidOutBus(*mLayersBusStrip,      kLayersBus, VC::LayerCol[0]);
    laidOutBus(*mBassBusStrip,        kBassBus,   VC::BassCol[0]);
    // J-5 (2026-05-03): RustyDrums Bus + 13 inserts sit between Bass and
    // Drums so they read as a sibling of Drums.  Visible only when
    // mRustyDrumsBusActive (= a BaySickRustyDrums singleton has been spawned).
    if (mRustyDrumsBusActive && mRustyDrumsBusStrip)
        laidOutBus(*mRustyDrumsBusStrip, kRustyDrumsBus, VC::DrumsCol);
    laidOutBus(*mDrumsBusStrip,       kDrumsBus,  VC::DrumsCol);

    x += kSepW;
    mScrollContent->setSize(x, mScrollContent->getHeight());

    // Publish neon-line data for paintOverChildren
    if (mScrollContent)
    {
        mScrollContent->mNeonLines = std::move(neon);
        mScrollContent->repaint();
    }
    // R3.5: any layout shift moves cable sockets - invalidate the cached overlay.
    if (mCableOverlay) mCableOverlay->repaint();

    // Keep our top scrollbar in sync with the new content width.
    syncHScrollBar();
}

int MixerPage::getScrollX() const
{
    return mViewport ? mViewport->getViewPositionX() : 0;
}

void MixerPage::setScrollX (int x)
{
    if (mViewport)
    {
        mViewport->setViewPosition (juce::jmax (0, x), mViewport->getViewPositionY());
        syncHScrollBar();
    }
}

void MixerPage::syncHScrollBar()
{
    if (!mHScrollBar || !mViewport || !mScrollContent) return;
    const double total = juce::jmax(1.0, (double)mScrollContent->getWidth());
    const double visible = juce::jmin(total, (double)mViewport->getWidth());
    const double viewX   = (double)mViewport->getViewPositionX();
    mHScrollBar->setRangeLimits(0.0, total, juce::dontSendNotification);
    mHScrollBar->setCurrentRange(juce::jlimit(0.0, juce::jmax(0.0, total - visible), viewX),
                                   visible,
                                   juce::dontSendNotification);
}

void MixerPage::scrollBarMoved(juce::ScrollBar* sb, double newRangeStart)
{
    if (sb != mHScrollBar.get() || !mViewport) return;
    mViewport->setViewPosition((int)std::round(newRangeStart),
                               mViewport->getViewPositionY());
}

void MixerPage::resized()
{
    constexpr int kHScrollBarH = 10;   // horizontal scrollbar height - always visible
    auto b = getLocalBounds();

    // Horizontal scrollbar strip (bottom of the scrollable strips area).
    auto scrollBarRow = b.removeFromBottom(kHScrollBarH);

    // Master strip (fixed, full page height - stationary, so no scrollbar
    // space is reserved on its column).
    mMasterStrip->setBounds(0, 0, kFixedPanelW, getHeight());

    int vpX = kFixedPanelW + 2;
    mViewport->setBounds(vpX, b.getY(), b.getWidth() - vpX, b.getHeight());

    if (mHScrollBar)
        mHScrollBar->setBounds(vpX, scrollBarRow.getY(),
                                 b.getWidth() - vpX, kHScrollBarH);

    mScrollContent->setSize(mScrollContent->getWidth(), mViewport->getHeight());
    layoutScrollContent();
    syncHScrollBar();

    // Cable overlay covers the full page and paints on top of every strip +
    // the horizontal scrollbar.  CableOverlay::hitTest is transparent off
    // cables / sockets, so the scrollbar stays draggable underneath the
    // visual overlap.
    if (mCableOverlay)
    {
        mCableOverlay->setBounds(getLocalBounds());
        mCableOverlay->toFront(false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::drawSectionLabel(juce::Graphics& g, const juce::String& text,
                                  juce::Rectangle<int> bounds) const
{
    g.setColour(VC::TextDim);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(text, bounds, juce::Justification::centred);
}

void MixerPage::paint(juce::Graphics& g)
{
    g.fillAll(VC::Bg);
    g.setColour(VC::Accent);
    g.fillRect(kFixedPanelW, 0, 2, getHeight());
}
