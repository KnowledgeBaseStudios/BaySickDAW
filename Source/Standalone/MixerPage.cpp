#include "MixerPage.h"
#include "UndoBracket.h"
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

// Same idea for a strip's main-out lines.  Line 0 stays exactly on the socket
// so a single-main strip is unchanged; lines 1..3 fan out far enough that the
// 10 px hit zones do not merge into one unclickable cable, and away from the
// send offsets above so a main and a send from the same strip stay separable.
static constexpr int kMainOutOffsets[4] = { 0, -12, 12, -24 };

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
    if (chId >= kAuxBase && chId < kAuxBase + kMaxAuxStrips) return juce::Colour(kEffectsTabPink);
    // Colored bus groups - track the main-out destination.  T10: secondary
    // group buses share their family accent.
    if (destChannelId == kLayersBus || destChannelId == kLayersBus2) return VC::LayerCol[0];
    if (destChannelId == kBassBus   || destChannelId == kBassBus2)   return VC::BassCol[0];
    if (destChannelId == kDrumsBus  || destChannelId == kDrumsBus2)  return VC::DrumsCol;
    if (destChannelId == kClipsBus  || destChannelId == kClipsBus2)  return VC::Warm;
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
    // QA-ModelShell TS6: Plugins Bus strips carry the purple accent that the
    // ribbon tab and the bus group divider also use -- one channel identity.
    if (destChannelId == kPluginsBus || destChannelId == kPluginsBus2) return VC::Purple;
    // Direct Routing / aux chain: fall back to the strip's natural color
    if (chId >= kLayerBase && chId < kLayerBase + kMaxLayerStrips) return VC::LayerCol[0];
    if (chId >= kBassBase  && chId < kBassBase  + kMaxBassStrips)  return VC::BassCol[0];
    if (chId >= kDrumBase  && chId < kDrumBase  + kMaxDrumStrips)  return VC::DrumsCol;
    if (chId >= kAudioBase && chId < kAudioBase + kMaxAudioStrips) return VC::Warm;
    if (chId >= kVoxBase   && chId < kVoxBase   + kMaxVoxStrips)  return juce::Colour(0xFF0FAFA5);
    if (chId >= kInstBase  && chId < kInstBase  + kMaxInstStrips) return juce::Colour(0xFF1C3A8A);
    if (chId >= kRustyBase && chId < kRustyBase + kMaxRustyStrips) return VC::DrumsCol;
    if (chId >= kPluginBase && chId < kPluginBase + kMaxPluginStrips) return VC::Purple;
    if (chId >= kDirectBase && chId < kDirectBase + kMaxDirectStrips) return VC::DirectGrey;
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
    // hitTest() gates click-through: only intercepts near a cable (T10/L12).
    setInterceptsMouseClicks(true, false);
}

bool MixerPage::CableOverlay::hitTest(int x, int y)
{
    // QA-Layout T10 (L12): the click-to-place + drag routing modes are
    // retired (targets are picked from the "+" menu now) -- the overlay only
    // intercepts near a cable, for the right-click property popup.
    auto pt = juce::Point<float>((float) x, (float) y);
    return hitTestCable(pt).srcId >= 0;
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

        // Pass A1: main-out cables (paint underneath sends).  The per-srcId
        // counter fans a multi-main strip's cables apart; the routing graph
        // emits a strip's lines in line order, so the counter IS the line
        // index and hit-testing can reproduce the same offset.
        std::map<int, int> mainLineIndex;
        for (const auto& e : edges)
        {
            if (! e.isMainOut) continue;

            const int mIdx = mainLineIndex[e.srcId]++;

            auto src = owner.getSocketPosition(e.srcId);
            auto dst = owner.getSocketPosition(e.dstId);
            if (src.x < 0 || dst.x < 0) continue;

            src.x += (float) kMainOutOffsets[mIdx % 4];

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

        // Pass B1: mains terminating at Master.  Counter walks every main-out
        // in lockstep with Phase A's pass so the line index -- and with it the
        // fan-out offset -- matches what was drawn there.
        std::map<int, int> mainLineIndexB;
        for (const auto& e : edges)
        {
            if (! e.isMainOut) continue;
            const int mIdx = mainLineIndexB[e.srcId]++;
            if (e.dstId != kMasterCh) continue;

            auto src = owner.getSocketPosition(e.srcId);
            auto dst = owner.getSocketPosition(e.dstId);

            // Fan out only a REAL socket: shifting the (-1,-1) sentinel would
            // hide the missing-strip marker renderMasterStub tests for.
            if (! (src.x == -1.f && src.y == -1.f))
                src.x += (float) kMainOutOffsets[mIdx % 4];

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


    // QA-Layout T10 (L12): the drag/placement ghost cables + the red
    // rejected-drop flash are gone with the modes that drew them -- routing
    // targets are picked from the "+" menu, where illegal targets are simply
    // disabled rows.
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
        // A main-out cable is actionable only when it is an EXTRA line (1..3):
        // those can be deleted.  Line 0 has no editable properties -- moving it
        // is a "+" menu action -- so it still opens nothing.
        auto isActionable = [] (const CableHit& h)
        { return ! h.isMainOut || h.mainLine >= 1; };

        if (hits.size() == 1)
        {
            if (isActionable (hits.front()))
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
                label = "Main " + juce::String (juce::jmax (0, h.mainLine) + 1) + ": "
                      + srcName + " -> " + dstName;
            else
                label = "Send " + juce::String(h.sendSlot + 1) + ": "
                      + srcName + " -> " + dstName;
            // QA-Eg: a main entry with no popup behind it is grayed-out; it
            // appears in the chooser for visibility only.
            chooser.addItem((int) i + 1, label, isActionable (h));
        }
        chooser.showMenuAsync(juce::PopupMenu::Options{},
            [this, hits, screenPt, isActionable](int r)
            {
                if (r > 0 && r <= (int) hits.size()
                    && isActionable (hits[(size_t)(r - 1)]))
                    showCablePopup(screenPt, hits[(size_t)(r - 1)]);
            });
        return;
    }

    // QA-Layout T10 (L12): non-right clicks do nothing -- the click-to-place
    // send/SC modes and the main-out socket drag are retired; every routing
    // action lives on the strip's "+" target menu.
    juce::ignoreUnused (e);
}

// QA-Layout T10 (L12): timerCallback (rejected-drop flash), findSocketNear
// and findStripUnder deleted with the drag/placement modes they served.

bool MixerPage::CableOverlay::isRouteAllowed(int srcId, int dstId) const
{
    using namespace MixerChannelIds;

    // Self-route / terminal output
    if (srcId == dstId)        return false;
    if (dstId == kOutput)      return false;

    // Bus/Master main-out is locked (drag never starts); defensive check
    if (isMainOutLocked(srcId)) return false;

    const bool srcIsLayer  = (srcId >= kLayerBase && srcId < kLayerBase + kMaxLayerStrips);
    const bool srcIsBass   = (srcId >= kBassBase  && srcId < kBassBase  + kMaxBassStrips);
    const bool srcIsDrum   = (srcId >= kDrumBase  && srcId < kDrumBase  + kMaxDrumStrips);
    const bool srcIsAudio  = (srcId >= kAudioBase && srcId < kAudioBase + kMaxAudioStrips);
    const bool srcIsAux    = (srcId >= kAuxBase   && srcId < kAuxBase   + kMaxAuxStrips);
    const bool srcIsVox    = (srcId >= kVoxBase   && srcId < kVoxBase   + kMaxVoxStrips);
    const bool srcIsInst   = (srcId >= kInstBase  && srcId < kInstBase  + kMaxInstStrips);
    const bool srcIsRusty  = (srcId >= kRustyBase && srcId < kRustyBase + kMaxRustyStrips);
    const bool srcIsPlugin = (srcId >= kPluginBase && srcId < kPluginBase + kMaxPluginStrips);
    const bool srcIsDirect = (srcId >= kDirectBase && srcId < kDirectBase + kMaxDirectStrips);

    const bool dstIsMaster = (dstId == kMaster);
    const bool dstIsAux    = (dstId >= kAuxBase && dstId < kAuxBase + kMaxAuxStrips);

    // Layer insert: Layers Bus · Bass Bus · Master.  T10: + active secondary
    // family buses (activation checked so cables to inactive buses don't open
    // invisible routing).
    if (srcIsLayer)
        return dstIsMaster || dstId == kLayersBus || dstId == kBassBus
            || (dstId == kLayersBus2 && owner.isLayersBus2Active())
            || (dstId == kBassBus2   && owner.isBassBus2Active());

    // Bass insert: Bass Bus · Layers Bus · Master (+ T10 secondaries).
    if (srcIsBass)
        return dstIsMaster || dstId == kBassBus || dstId == kLayersBus
            || (dstId == kBassBus2   && owner.isBassBus2Active())
            || (dstId == kLayersBus2 && owner.isLayersBus2Active());

    // Drum insert: its OWN kit's Drums Bus · Master.  QA-SOUNDNESS (2026-08-07):
    // the two kits are independent, so a bank-2 drum is offered Drums Bus 2 and
    // never bank 1's bus (and vice versa) -- crossing the banks at the mixer
    // would put one kit's audio back through the other kit's inserts.
    if (srcIsDrum)
        return dstIsMaster || dstId == drumBusForPage (srcId - kDrumBase);

    // QA-TrueLevel SC-10: a Direct to Master strip goes to the master, full stop
    // (its main out is locked there; this is the aux-send side).
    if (srcIsDirect)
        return dstIsMaster || dstIsAux;

    // QA-ModelShell TS6 (BLU-447): Plugin insert: Plugins Bus, Layers Bus,
    // Bass Bus, Master.  Jeff's spec -- a VST strip moves under Layers or Bass
    // exactly as those two already move between each other's bus, so this rule
    // is the Layer rule plus its own bus, not a new routing class.
    if (srcIsPlugin)
        return dstIsMaster || dstId == kPluginsBus
            || dstId == kLayersBus || dstId == kBassBus
            || (dstId == kPluginsBus2 && owner.isPluginsBus2Active())
            || (dstId == kLayersBus2  && owner.isLayersBus2Active())
            || (dstId == kBassBus2    && owner.isBassBus2Active());

    // J-5 (2026-05-03): Rusty insert main-out is LOCKED to kRustyDrumsBus
    // (enforced via isMainOutLocked).  This rule covers send cables only -
    // sends are restricted to aux strips per spec, no inter-bus routing.
    if (srcIsRusty)
        return dstIsAux;

    // Audio insert: any bus EXCEPT FX · Master (FX reachable only via aux-send).
    // R1 (2026-04-23): added Vox + Inst bus destinations.
    // G-6 (2026-04-29): secondary Vox/Inst buses also valid destinations.
    // QA-SOUNDNESS (2026-08-07): the second drums bus is offered on the same
    // terms as the first and takes NO activation guard -- kDrumsBus2 has no
    // *Active flag by design (its visibility is membership-driven in
    // laidOutBus), unlike the Clips/Layers/Bass secondaries below.
    if (srcIsAudio)
        return dstIsMaster || dstId == kLayersBus || dstId == kBassBus
            || dstId == kDrumsBus || dstId == kDrumsBus2 || dstId == kClipsBus
            || dstId == kVoxBus  || dstId == kInstBus
            || dstId == kVoxBus2 || dstId == kInstBus2 || dstId == kInstBus3
            || (dstId == kClipsBus2  && owner.isClipsBus2Active())
            || (dstId == kLayersBus2 && owner.isLayersBus2Active())
            || (dstId == kBassBus2   && owner.isBassBus2Active());

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
// QA-Layout T10 (L12): the send/SC placement modes (mouseMove ghost tracking,
// Escape cancel, start/cancel pairs) are deleted -- the slot finders below
// survive because the "+" target menu commits through them.
// ─────────────────────────────────────────────────────────────────────────────
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

// ── Main-out line helpers ────────────────────────────────────────────────────
// Line 0 lives in <prefix>_sendTo (always active); lines 1..3 in
// <prefix>_mainOut{N}_to with -1 = inactive.
int MixerPage::CableOverlay::mainOutDest (const juce::String& prefix, int line) const
{
    if (prefix.isEmpty() || line < 0 || line >= MixerChannelIds::kMaxMainOutsPerStrip)
        return -1;
    if (auto* p = owner.mProcessor.apvts.getRawParameterValue (
                      MixerChannelIds::mainOutParamId (prefix, line)))
        return (int) p->load();
    return -1;
}

int MixerPage::CableOverlay::findAvailableMainOutLine (const juce::String& prefix) const
{
    for (int line = 1; line < MixerChannelIds::kMaxMainOutsPerStrip; ++line)
        if (mainOutDest (prefix, line) < 0)
            return line;
    return -1;
}

bool MixerPage::CableOverlay::isMainOutDestInUse (const juce::String& prefix, int dstId) const
{
    if (dstId < 0) return false;
    for (int line = 0; line < MixerChannelIds::kMaxMainOutsPerStrip; ++line)
        if (mainOutDest (prefix, line) == dstId)
            return true;
    return false;
}

// C.4 Phase 1 (2026-04-30): target-side SC slot finder -- SC lines are
// encoded on the TARGET's _sc_recv{N}_from so per-module pickers see stable
// line indices regardless of cable order.
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

// QA-Layout T10 (L12): the per-strip "+" menu enumerates CONCRETE targets --
// Send... / Sidechain... / Move Output... submenus built from the live strip
// set, filtered by the same rules the retired click-to-place + drag paths
// enforced (isValidBusSendTarget / isRouteAllowed / wouldCreateCycle);
// picking writes the same params those paths wrote.  Illegal targets render
// as disabled rows.  Items are action-lambdas (itemID -1, self-dispatching).
void MixerPage::onAddCableRequestedFor(int srcChannelId)
{
    using namespace MixerChannelIds;
    if (mCableOverlay == nullptr) return;

    auto& graph = mProcessor.mVibeGraph.getRoutingGraph();
    juce::Component::SafePointer<MixerPage> safeThis (this);

    auto writeNatural = [safeThis] (const juce::String& paramId, float natural)
    {
        if (! safeThis) return;
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                safeThis->mProcessor.apvts.getParameter (paramId)))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (natural));
    };

    const juce::String srcPrefix = prefixFromChannelId (srcChannelId);
    const auto entries = getStemPickEntries();

    juce::PopupMenu m;

    // Send... -> aux strips only (isValidBusSendTarget), cycle-filtered.
    {
        juce::PopupMenu sendSub;
        const bool haveFreeSlot = mCableOverlay->findAvailableSendSlot (srcPrefix) >= 0;
        for (const auto& en : entries)
        {
            if (! isValidBusSendTarget (en.channelId) || en.channelId == srcChannelId) continue;
            const int dstId = en.channelId;
            const bool ok = haveFreeSlot && ! graph.wouldCreateCycle (srcChannelId, dstId);
            sendSub.addItem (en.name, ok, false,
                [safeThis, writeNatural, srcPrefix, dstId]
                {
                    if (! safeThis || safeThis->mCableOverlay == nullptr) return;
                    const int slot = safeThis->mCableOverlay->findAvailableSendSlot (srcPrefix);
                    if (slot < 0) return;
                    beginParamUndoGesture (safeThis->mProcessor.apvts, srcPrefix + "_send" + juce::String (slot) + "_to"); // Task 6 (12-iv)
                    writeNatural (srcPrefix + "_send" + juce::String (slot) + "_to", (float) dstId);
                });
        }
        // The retired click-to-place auto-created an aux when none existed;
        // the menu keeps that capability as an explicit row.
        sendSub.addItem ("New Aux Strip", haveFreeSlot, false,
            [safeThis, writeNatural, srcPrefix]
            {
                if (! safeThis || safeThis->mCableOverlay == nullptr) return;
                const int auxIdx = safeThis->mNextAuxIdx;
                safeThis->addAuxChannelAtIndex (auxIdx);
                const int slot = safeThis->mCableOverlay->findAvailableSendSlot (srcPrefix);
                if (slot < 0) return;
                beginParamUndoGesture (safeThis->mProcessor.apvts, srcPrefix + "_send" + juce::String (slot) + "_to"); // Task 6 (12-iv)
                writeNatural (srcPrefix + "_send" + juce::String (slot) + "_to",
                              (float) auxStrip (auxIdx));
            });
        m.addSubMenu ("Send...", sendSub);
    }

    // Sidechain... -> any other strip with a free SC receive line (target-side
    // encoding), cycle-filtered.
    {
        juce::PopupMenu scSub;
        for (const auto& en : entries)
        {
            if (en.channelId == srcChannelId) continue;
            const int dstId = en.channelId;
            const juce::String targetPrefix = prefixFromChannelId (dstId);
            const bool ok = targetPrefix.isNotEmpty()
                         && mCableOverlay->findAvailableScRecvSlot (targetPrefix) >= 0
                         && ! graph.wouldCreateCycle (srcChannelId, dstId);
            scSub.addItem (en.name, ok, false,
                [safeThis, writeNatural, srcChannelId, dstId]
                {
                    if (! safeThis || safeThis->mCableOverlay == nullptr) return;
                    const juce::String tp = MixerChannelIds::prefixFromChannelId (dstId);
                    const int slot = safeThis->mCableOverlay->findAvailableScRecvSlot (tp);
                    if (slot < 0) return;
                    beginParamUndoGesture (safeThis->mProcessor.apvts, tp + "_sc_recv" + juce::String (slot) + "_from"); // Task 6 (12-iv)
                    writeNatural (tp + "_sc_recv" + juce::String (slot) + "_from",
                                  (float) srcChannelId);
                });
        }
        m.addSubMenu ("Sidechain...", scSub);
    }

    // ── Main-out lines ────────────────────────────────────────────────────────
    // A strip feeds up to kMaxMainOutsPerStrip destinations.  Line 0 is its
    // permanent output and can only be MOVED; lines 1..3 are added and removed.
    // Every line answers to the same three rules the single line always did:
    // isRouteAllowed for this source type, no cycle against the whole edge set
    // (which now includes every strip's extra lines), and no two lines of one
    // strip on the same destination.  A main-out-locked strip (Master, buses,
    // Rusty inserts) gets none of these submenus and so keeps exactly one.
    if (! isMainOutLocked (srcChannelId))
    {
        int line0Dest = defaultSendTo (srcChannelId);
        if (auto* p = mProcessor.apvts.getRawParameterValue (srcPrefix + "_sendTo"))
        {
            const int v = (int) p->load();
            if (v >= 0) line0Dest = v;
        }

        auto destName = [&entries] (int chId) -> juce::String
        {
            for (const auto& en : entries)
                if (en.channelId == chId) return en.name;
            return friendlyName (chId);
        };

        // Move Output... -> retarget line 0; current destination shows a tick.
        {
            juce::PopupMenu moveSub;
            for (const auto& en : entries)
            {
                if (en.channelId == srcChannelId) continue;
                if (! mCableOverlay->isRouteAllowed (srcChannelId, en.channelId)) continue;
                const int dstId = en.channelId;
                const bool ticked = dstId == line0Dest;
                const bool ok = ticked
                             || (! mCableOverlay->isMainOutDestInUse (srcPrefix, dstId)
                                 && ! graph.wouldCreateCycle (srcChannelId, dstId));
                moveSub.addItem (en.name, ok, ticked,
                    [safeThis, writeNatural, srcPrefix, dstId]
                    {
                        if (safeThis) beginParamUndoGesture (safeThis->mProcessor.apvts, srcPrefix + "_sendTo"); // Task 6 (12-iv)
                        writeNatural (srcPrefix + "_sendTo", (float) dstId);
                    });
            }
            m.addSubMenu ("Move Output...", moveSub);
        }

        // Add Main Out... -> claim the first free line 1..3 for this target.
        {
            juce::PopupMenu addSub;
            const bool haveFreeLine = mCableOverlay->findAvailableMainOutLine (srcPrefix) >= 0;
            for (const auto& en : entries)
            {
                if (en.channelId == srcChannelId) continue;
                if (! mCableOverlay->isRouteAllowed (srcChannelId, en.channelId)) continue;
                const int dstId = en.channelId;
                const bool ok = haveFreeLine
                             && ! mCableOverlay->isMainOutDestInUse (srcPrefix, dstId)
                             && ! graph.wouldCreateCycle (srcChannelId, dstId);
                addSub.addItem (en.name, ok, false,
                    [safeThis, writeNatural, srcPrefix, dstId]
                    {
                        if (! safeThis || safeThis->mCableOverlay == nullptr) return;
                        if (safeThis->mCableOverlay->isMainOutDestInUse (srcPrefix, dstId)) return;
                        const int line = safeThis->mCableOverlay->findAvailableMainOutLine (srcPrefix);
                        if (line < 0) return;
                        const juce::String id = MixerChannelIds::mainOutParamId (srcPrefix, line);
                        beginParamUndoGesture (safeThis->mProcessor.apvts, id); // Task 6 (12-iv)
                        writeNatural (id, (float) dstId);
                    });
            }
            m.addSubMenu ("Add Main Out...", addSub);
        }

        // Remove Main Out... -> drop an extra line.  Line 0 is listed disabled
        // rather than hidden so it is visible WHY it cannot be removed: a strip
        // always keeps one output, and moving it is the other submenu.
        {
            juce::PopupMenu remSub;
            for (int line = 0; line < kMaxMainOutsPerStrip; ++line)
            {
                const int dstId = (line == 0) ? line0Dest
                                              : mCableOverlay->mainOutDest (srcPrefix, line);
                if (dstId < 0) continue;
                if (line == 0)
                {
                    remSub.addItem (destName (dstId) + "  (main output)", false, false, [] {});
                    continue;
                }
                remSub.addItem (destName (dstId), true, false,
                    [safeThis, writeNatural, srcPrefix, line]
                    {
                        if (! safeThis) return;
                        const juce::String id = MixerChannelIds::mainOutParamId (srcPrefix, line);
                        beginParamUndoGesture (safeThis->mProcessor.apvts, id); // Task 6 (12-iv)
                        writeNatural (id, -1.0f);
                    });
            }
            m.addSubMenu ("Remove Main Out...", remSub);
        }
    }

    m.showMenuAsync (juce::PopupMenu::Options{});
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

    // Reverse-lookup: which persisted main-out line (0..3) of this source
    // currently points at dstId?  The paint-order index is NOT the line index
    // when an earlier line sits inactive, and the popup acts on the persisted
    // line.  -1 when the strip has no registered params (nothing to act on).
    auto mainLineForDest = [this] (int srcId, int dstId) -> int
    {
        const juce::String prefix = MixerChannelIds::prefixFromChannelId (srcId);
        if (prefix.isEmpty()) return -1;
        for (int line = 0; line < MixerChannelIds::kMaxMainOutsPerStrip; ++line)
            if (mainOutDest (prefix, line) == dstId) return line;
        return -1;
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

    // Pass 2: main-out cables.  Mirrors paint's Pass A1 counter so a strip with
    // several mains hit-tests at the offsets those cables were drawn at; the
    // line index is also what the popup needs to know WHICH line was clicked.
    std::map<int, int> mainLineIndex;
    for (const auto& e : edges)
    {
        if (! e.isMainOut) continue;
        const int mIdx = mainLineIndex[e.srcId]++;

        auto src = owner.getSocketPosition(e.srcId);
        auto dst = owner.getSocketPosition(e.dstId);
        if (src.x < 0 || dst.x < 0) continue;

        src.x += (float) kMainOutOffsets[mIdx % 4];

        if (cableHits(src, dst))
        {
            CableHit hit;
            hit.srcId     = e.srcId;
            hit.dstId     = e.dstId;
            hit.isMainOut = true;
            hit.mainLine  = mainLineForDest (e.srcId, e.dstId);
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
                hit.mainLine  = mainLineForDest (e.srcId, e.dstId);
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
    BaySickSlider         mAmountSlider;
    juce::ToggleButton mPrePostBtn;
    juce::TextButton   mDeleteBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAmountAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mPrePostAtt;
};

// QA-Eg: the old CableMainOutPopup was removed as a glorified tooltip.  A main
// cable on line 0 still has no properties to edit and stays a grayed-out
// chooser entry; the extra main lines carry a delete, whose popup is defined
// inline in showCablePopup next to the SC one.
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
            beginParamUndoGesture(owner.mProcessor.apvts, sp + "_from"); // Task 6 (12-iv)
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
        // Line 0 has no popup: it is the strip's permanent output and moving it
        // is a "+" menu action.  An EXTRA line (1..3) gets a delete affordance
        // here so a cable the user can see is a cable the user can cut.
        if (hit.mainLine < 1) return;

        const juce::String prefix = prefixFromChannelId (hit.srcId);
        if (prefix.isEmpty()) return;
        auto deleteAction = [this, prefix, line = hit.mainLine]
        {
            const juce::String id = MixerChannelIds::mainOutParamId (prefix, line);
            beginParamUndoGesture (owner.mProcessor.apvts, id); // Task 6 (12-iv)
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
                    owner.mProcessor.apvts.getParameter (id)))
                p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (-1.f));
            // Same reason as the SC delete below: the routing graph rebuilds
            // from APVTS on the next audio block, but nothing repaints the
            // overlay off a param write, so the cut cable would linger.
            repaint();
        };

        // MainOutCablePopup mirrors ScCablePopup: an info line plus Delete, no
        // amount or pre/post (a main out is unity-gain by definition).
        struct MainOutCablePopup : public juce::Component {
            juce::Label      info;
            juce::TextButton delBtn;
            std::function<void()> onDelete;
            MainOutCablePopup (juce::String src, juce::String dst, int line,
                               std::function<void()> del)
                : onDelete (std::move (del))
            {
                info.setText ("Main " + juce::String (line + 1) + ": "
                                + src + " -> " + dst,
                              juce::dontSendNotification);
                info.setColour (juce::Label::textColourId, juce::Colours::white);
                info.setJustificationType (juce::Justification::centred);
                addAndMakeVisible (info);

                delBtn.setButtonText ("Remove Main Out");
                delBtn.setColour (juce::TextButton::buttonColourId,
                                  juce::Colour (0xff4a3030));
                delBtn.onClick = [this] {
                    if (onDelete) onDelete();
                    if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                        cb->dismiss();
                };
                addAndMakeVisible (delBtn);

                setSize (230, 60);
            }
            void resized() override
            {
                info  .setBounds (0, 4, getWidth(), 22);
                delBtn.setBounds (getWidth() / 2 - 65, 30, 130, 24);
            }
            void paint (juce::Graphics& g) override
            {
                g.fillAll (juce::Colour (0xff1e2024));
            }
        };

        content = std::make_unique<MainOutCablePopup>(
            getStripName (hit.srcId), getStripName (hit.dstId),
            hit.mainLine, std::move (deleteAction));
    }
    else if (hit.sendSlot >= 0)
    {
        const juce::String prefix = prefixFromChannelId(hit.srcId);
        auto deleteAction = [this, prefix, slot = hit.sendSlot]
        {
            const juce::String sp = prefix + "_send" + juce::String(slot);
            beginParamUndoGesture(owner.mProcessor.apvts, sp + "_to"); // Task 6 (12-iv)
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

    // Bus strips (fixed-id slots; the lazy secondary buses may be null).
    reg(kMaster,        mMasterStrip       .get());
    reg(kLayersBus,     mLayersBusStrip    .get());
    reg(kBassBus,       mBassBusStrip      .get());
    reg(kDrumsBus,      mDrumsBusStrip     .get());
    reg(kDrumsBus2,     mDrumsBus2Strip    .get());
    reg(kFxBus,         mFXBusStrip        .get());
    reg(kClipsBus,      mAudioClipsBusStrip.get());
    reg(kVoxBus,        mVoxBusStrip       .get());
    reg(kInstBus,       mInstBusStrip      .get());
    reg(kVoxBus2,       mVoxBus2Strip      .get());
    reg(kInstBus2,      mInstBus2Strip     .get());
    reg(kInstBus3,      mInstBus3Strip     .get());
    reg(kRustyDrumsBus, mRustyDrumsBusStrip.get());
    reg(kPluginsBus,    mPluginsBusStrip   .get());
    reg(kLayersBus2,    mLayersBus2Strip   .get());   // T10
    reg(kBassBus2,      mBassBus2Strip     .get());
    reg(kClipsBus2,     mClipsBus2Strip    .get());
    reg(kPluginsBus2,   mPluginsBus2Strip  .get());

    // Dynamic instrument channels (8 std::map containers, keyed by index).
    for (auto& [tabId, strip] : mLayerStrips) reg(kLayerBase + tabId, strip.get());
    for (auto& [tabId, strip] : mBassStrips)  reg(kBassBase  + tabId, strip.get());
    for (auto& [slot,  strip] : mDrumStrips)  reg(kDrumBase  + slot,  strip.get());
    for (auto& [row,   strip] : mAudioStrips) reg(kAudioBase + row,   strip.get());
    for (auto& [idx,   strip] : mAuxStrips)   reg(kAuxBase   + idx,   strip.get());
    for (auto& [idx,   strip] : mVoxStrips)   reg(kVoxBase   + idx,   strip.get());
    for (auto& [idx,   strip] : mInstStrips)  reg(kInstBase  + idx,   strip.get());
    for (auto& [idx,   strip] : mRustyStrips) reg(kRustyBase + idx,   strip.get());
    for (auto& [idx,   strip] : mPluginStrips) reg(kPluginBase + idx, strip.get());
    for (auto& [idx,   strip] : mDirectStrips) reg(kDirectBase + idx, strip.get());
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
MixerPage::MixerPage(BaySickDAWProcessor& processor, PatternManager& pm)
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
    // QA-SOUNDNESS (2026-08-07): kit 2's bus.  Same Drums-red accent as Drums
    // Bus so the two kits read as one family.
    mDrumsBus2Strip     = std::make_unique<MixerTrackStrip>("Drums Bus 2",
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
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument bus.
    mPluginsBusStrip = std::make_unique<MixerTrackStrip>("Plugins Bus",
                              MixerTrackStrip::StripType::Bus, VC::Purple);

    mLayersBusStrip    ->setAutomationPrefix("mixer_layers");
    mBassBusStrip      ->setAutomationPrefix("mixer_bass");
    mDrumsBusStrip     ->setAutomationPrefix("mixer_drums");
    mDrumsBus2Strip    ->setAutomationPrefix("mixer_drumsbus2");
    mFXBusStrip        ->setAutomationPrefix("mixer_fx");
    mAudioClipsBusStrip->setAutomationPrefix("mixer_clipsbus");
    mVoxBusStrip       ->setAutomationPrefix("mixer_voxbus");
    mInstBusStrip      ->setAutomationPrefix("mixer_instbus");
    mRustyDrumsBusStrip->setAutomationPrefix("mixer_rustybus");
    mPluginsBusStrip   ->setAutomationPrefix("mixer_pluginbus");

    // 5F-4a: bind each bus strip's new controls (polarity/width/bypass) to APVTS
    mLayersBusStrip    ->setApvts(mProcessor.apvts, "mixer_layers");
    mBassBusStrip      ->setApvts(mProcessor.apvts, "mixer_bass");
    mDrumsBusStrip     ->setApvts(mProcessor.apvts, "mixer_drums");
    mDrumsBus2Strip    ->setApvts(mProcessor.apvts, "mixer_drumsbus2");
    mFXBusStrip        ->setApvts(mProcessor.apvts, "mixer_fx");
    mAudioClipsBusStrip->setApvts(mProcessor.apvts, "mixer_clipsbus");
    mVoxBusStrip       ->setApvts(mProcessor.apvts, "mixer_voxbus");
    mInstBusStrip      ->setApvts(mProcessor.apvts, "mixer_instbus");
    mRustyDrumsBusStrip->setApvts(mProcessor.apvts, "mixer_rustybus");
    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  Without this the Plugins
    // bus strip's polarity / width / bypass had no parameter behind them.
    mPluginsBusStrip   ->setApvts(mProcessor.apvts, "mixer_pluginbus");

    wireBusCallbacks(mLayersBusStrip.get(),     mx.layersLevel,          mx.layersPan,        mx.layersMute,         mx.layersSolo);
    wireBusCallbacks(mBassBusStrip.get(),        mx.bassLevel,            mx.bassPan,          mx.bassMute,           mx.bassSolo);
    wireBusCallbacks(mDrumsBusStrip.get(),       mx.drumsLevel,           mx.drumsPan,         mx.drumsMute,          mx.drumsSolo);
    wireBusCallbacks(mAudioClipsBusStrip.get(),  mx.audioClipsBusLevel,   mx.audioClipsBusPan, mx.audioClipsBusMute,  mx.audioClipsBusSolo);

    // FX Bus doesn't participate in the legacy MixerState struct (no dedicated
    // level/pan/mute/solo members), so wireBusCallbacks isn't used. Still wire
    // onFXClicked so its FX Rack button navigates to the Effects Page.
    // Drums Bus 2, the secondary group buses and the Vox/Inst/Rusty/Plugins
    // buses are all in that same class -- their fader/pan/mute/solo live only
    // in APVTS, which is where the audio path and undo both read them.
    mFXBusStrip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    mDrumsBus2Strip->onFXClicked = [this](const juce::String& id) {
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
    mPluginsBusStrip->onFXClicked = [this](const juce::String& id) {
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
    // QA-SOUNDNESS: parented hidden so the empty kit-2 bus never flashes
    // between construction and the first layout pass, which is what decides
    // its visibility from then on (laidOutBus, membership-gated).
    mScrollContent->addChildComponent(*mDrumsBus2Strip);
    mDrumsBus2Strip->setVisible(false);
    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  This was the whole reason
    // the Plugins bus never appeared: the strip was constructed and configured
    // but never PARENTED, so it was an orphan Component that could not render no
    // matter what mPluginsBusActive said.  Same lazy shape as Rusty above.
    mScrollContent->addChildComponent(*mPluginsBusStrip);
    mPluginsBusStrip->setVisible(false);

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
    // TS7 (CL-044): master's "+" is the Analyzer button, so it fires this instead.
    if (mMasterStrip)
        mMasterStrip->onAnalyzerRequested = [this]
        {
            if (onAnalyzerRequested) onAnalyzerRequested();
        };
    wireSendBtn(mLayersBusStrip.get());
    wireSendBtn(mBassBusStrip.get());
    wireSendBtn(mDrumsBusStrip.get());
    wireSendBtn(mDrumsBus2Strip.get());
    wireSendBtn(mFXBusStrip.get());
    wireSendBtn(mAudioClipsBusStrip.get());
    wireSendBtn(mVoxBusStrip.get());
    wireSendBtn(mInstBusStrip.get());
    wireSendBtn(mRustyDrumsBusStrip.get());
    wireSendBtn(mPluginsBusStrip.get());

    // 5F-4b B3: set channel IDs on fixed strips for cable rendering
    mMasterStrip      ->setChannelId(MixerChannelIds::kMaster);
    mLayersBusStrip   ->setChannelId(MixerChannelIds::kLayersBus);
    mBassBusStrip     ->setChannelId(MixerChannelIds::kBassBus);
    mDrumsBusStrip    ->setChannelId(MixerChannelIds::kDrumsBus);
    mDrumsBus2Strip   ->setChannelId(MixerChannelIds::kDrumsBus2);
    mFXBusStrip       ->setChannelId(MixerChannelIds::kFxBus);
    mAudioClipsBusStrip->setChannelId(MixerChannelIds::kClipsBus);
    mVoxBusStrip      ->setChannelId(MixerChannelIds::kVoxBus);
    mInstBusStrip     ->setChannelId(MixerChannelIds::kInstBus);
    mRustyDrumsBusStrip->setChannelId(MixerChannelIds::kRustyDrumsBus);
    mPluginsBusStrip   ->setChannelId(MixerChannelIds::kPluginsBus);

    // QA-Layout T10 (L13): the five title-strip add buttons are gone -- the
    // strip's "Add" titled menu (StandaloneEditor's Mixer branch) carries
    // Aux Strip + the bus adds; Vox/Inst STRIP adds live on the ribbon's "+".

    // 5F-4b B7: restore any aux strips that were in the saved project.
    // BaySickGraph already has their InsertNodes (registered by restoreAuxStripsFromState
    // in setStateInformation). Create the matching UI strips.
    for (int idx : mProcessor.mVibeGraph.getAuxIndices())
        addAuxChannelAtIndex(idx);

    syncFromModel();
    syncApvtsFromMixerState();   // 5F-4a Batch 6

    // The 30 Hz poll and the vblank meter feed are NOT started here -- both are
    // owned by parentHierarchyChanged, which starts them only once this page is
    // attached to something on screen.  Starting them in the constructor would
    // run them for a page that is built but never framed, which is now a normal
    // state (QA-ModelShell TS4: windows open lazily).
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
        if (onChannelRenamed) onChannelRenamed(StripKind::Layer, pageIndex, newName);
    };

    mScrollContent->addAndMakeVisible(*strip);
    mLayerStrips[pageIndex] = std::move(strip);
    mLayerTabOrder.push_back(pageIndex);

    if (getWidth() > 0) resized();
}

// QA-ModelShell TS6 (BLU-447): hosted VST3 instrument strip.  Mirrors
// addLayerChannel -- engine-driven, no arm/monitor, spawned by engine
// registration rather than an "Add Strip" button.  Also flips
// mPluginsBusActive, which is what makes the Plugins BUS strip appear: the bus
// is always allocated in the graph but stays hidden until it has members.
void MixerPage::addPluginChannel(int pageIndex, const juce::String& name)
{
    if (mPluginStrips.count(pageIndex) > 0) return;

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::LayerChannel, VC::Purple);
    const juce::String prefix = "mixer_plugin_" + juce::String(pageIndex);
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::pluginInsert(pageIndex));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };

    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onNameChanged = [this, pageIndex](const juce::String& newName) {
        if (onChannelRenamed) onChannelRenamed(StripKind::Plugin, pageIndex, newName);
    };

    mScrollContent->addAndMakeVisible(*strip);
    mPluginStrips[pageIndex] = std::move(strip);
    mPluginOrder.push_back(pageIndex);
    mPluginsBusActive = true;
    mStripCacheDirty  = true;

    if (getWidth() > 0) resized();
}

// QA-TrueLevel SC-10: Direct to Master strip.  Engine-less and page-less: a
// file plays into it (DirectFileTask) and it sits under the master because its
// _sendTo is the master and locked there.  Layer shape (no arm / monitor).
void MixerPage::addDirectChannel(int idx, const juce::String& name)
{
    if (mDirectStrips.count(idx) > 0) return;

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::LayerChannel, VC::DirectGrey);
    const juce::String prefix = "mixer_direct_" + juce::String(idx);
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::directInsert(idx));
    strip->onAddSendRequested = [this](int chId) {
        onAddCableRequestedFor(chId);
    };
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onNameChanged = [this, idx](const juce::String& newName) {
        if (onChannelRenamed) onChannelRenamed(StripKind::Direct, idx, newName);
    };

    mScrollContent->addAndMakeVisible(*strip);
    mDirectStrips[idx] = std::move(strip);
    mDirectOrder.push_back(idx);
    mStripCacheDirty = true;

    if (getWidth() > 0) resized();
}

void MixerPage::removeDirectChannel(int idx)
{
    mStripCacheDirty = true;
    mDirectStrips.erase(idx);
    mDirectOrder.erase(std::remove(mDirectOrder.begin(), mDirectOrder.end(), idx),
                       mDirectOrder.end());
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::setDirectChannelMissing(int idx, bool missing)
{
    if (auto it = mDirectStrips.find(idx); it != mDirectStrips.end() && it->second)
        it->second->setMissingFile(missing);
}

juce::String MixerPage::getDirectStripName (int idx) const
{
    auto it = mDirectStrips.find (idx);
    return (it != mDirectStrips.end() && it->second) ? it->second->getName()
                                                     : juce::String ("Direct " + juce::String (idx + 1));
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
        if (onChannelRenamed) onChannelRenamed(StripKind::Bass, pageIndex, newName);
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

    if (slot >= 0 && slot < kMaxDrumPages)
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
// T10: addVoxChannel / addInstChannel wrappers deleted -- Vox/Inst strip
// creation flows exclusively through the ribbon's "+" (addVoxChannelAtIndex /
// addInstChannelAtIndex remain the entry points).

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
    return true;
}

// QA-Layout T10 (L13): one shared body for the four secondary group buses --
// same shape as activateVoxBus2 above, parameterized instead of cloned.
bool MixerPage::activateGroupBus (std::unique_ptr<MixerTrackStrip>& strip, bool& activeFlag,
                                  const juce::String& name, juce::Colour color,
                                  const char* prefix, int chId,
                                  const juce::String& deleteTitle,
                                  const juce::String& parentName)
{
    if (activeFlag) return false;
    if (! strip)
    {
        strip = std::make_unique<MixerTrackStrip>(name,
            MixerTrackStrip::StripType::Bus, color);
        strip->setAutomationPrefix(prefix);
        strip->setApvts(mProcessor.apvts, prefix);
        strip->setChannelId(chId);
        strip->onAddSendRequested = [this](int ch) {
            onAddCableRequestedFor(ch);
        };
        strip->onFXClicked = [this](const juce::String& id) {
            if (onEffectsTabRequested) onEffectsTabRequested(id);
        };
        strip->onNameChanged = [this](const juce::String&) {
            if (getWidth() > 0) layoutScrollContent();
            if (onAudioStripRenamed) onAudioStripRenamed();
        };
        strip->setRenameable (true);
        {
            juce::Component::SafePointer<MixerPage> safeThis (this);
            strip->onContextMenuRequested =
                [safeThis, chId, deleteTitle, parentName] (juce::Point<int> screenPos)
            {
                if (! safeThis) return;
                juce::PopupMenu m;
                m.addItem (1, deleteTitle);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                                    juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1)),
                    [safeThis, chId, deleteTitle, parentName] (int r)
                    {
                        if (! safeThis || r != 1) return;
                        auto* aw = new juce::AlertWindow (
                            deleteTitle,
                            "Deleting this bus removes its Effects Rack.\n"
                            "All attached strips will be moved to the " + parentName + ".",
                            juce::AlertWindow::WarningIcon);
                        aw->addButton ("Delete", 1);
                        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                        aw->enterModalState (true, juce::ModalCallbackFunction::create (
                            [safeThis, aw, chId] (int result)
                            {
                                std::unique_ptr<juce::AlertWindow> own (aw);
                                if (result != 1 || ! safeThis) return;
                                safeThis->deleteSecondaryBus (chId);
                            }), false);
                    });
            };
        }
        mScrollContent->addAndMakeVisible(*strip);
    }
    else
    {
        strip->setVisible(true);
    }
    activeFlag = true;
    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
    return true;
}

bool MixerPage::isSecondaryBus (int chId)
{
    using namespace MixerChannelIds;
    return chId == kVoxBus2   || chId == kInstBus2  || chId == kInstBus3
        || chId == kLayersBus2 || chId == kBassBus2
        || chId == kClipsBus2  || chId == kPluginsBus2;
}

void MixerPage::deactivateBusFlagOnly (int chId)
{
    using namespace MixerChannelIds;
    switch (chId)
    {
        case kVoxBus2:     mVoxBus2Active     = false; break;
        case kInstBus2:    mInstBus2Active    = false; break;
        case kInstBus3:    mInstBus3Active    = false; break;
        case kLayersBus2:  mLayersBus2Active  = false; break;
        case kBassBus2:    mBassBus2Active    = false; break;
        case kClipsBus2:   mClipsBus2Active   = false; break;
        case kPluginsBus2: mPluginsBus2Active = false; break;
        default: break;
    }
}

bool MixerPage::activateLayersBus2()
{
    return activateGroupBus (mLayersBus2Strip, mLayersBus2Active,
                             "Layers Bus 2", VC::LayerCol[0],
                             "mixer_layersbus2", MixerChannelIds::kLayersBus2,
                             "Delete Layers Bus", "main Layers Bus");
}

bool MixerPage::activateBassBus2()
{
    return activateGroupBus (mBassBus2Strip, mBassBus2Active,
                             "Bass Bus 2", VC::BassCol[0],
                             "mixer_bassbus2", MixerChannelIds::kBassBus2,
                             "Delete Bass Bus", "main Bass Bus");
}

bool MixerPage::activateClipsBus2()
{
    return activateGroupBus (mClipsBus2Strip, mClipsBus2Active,
                             "Clips Bus 2", VC::Warm,
                             "mixer_clipsbus2", MixerChannelIds::kClipsBus2,
                             "Delete Clips Bus", "main Clips Bus");
}

bool MixerPage::activatePluginsBus2()
{
    return activateGroupBus (mPluginsBus2Strip, mPluginsBus2Active,
                             "Plugins Bus 2", VC::Purple,
                             "mixer_pluginbus2", MixerChannelIds::kPluginsBus2,
                             "Delete Plugins Bus", "main Plugins Bus");
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
    // A Vox strip's name persists in <VoxNames>, but the owning Vox tab persists
    // its own copy -- so the rename has to reach the tab too or the two names
    // drift apart permanently.
    strip->onNameChanged = [this, idx](const juce::String& newName) {
        if (onChannelRenamed) onChannelRenamed (StripKind::Vox, idx, newName);
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
    // QA-Layout L2: one term for the live-input Inst flavor -- "LiveInst"
    // (was "Instrument Input", a third spelling next to "Inst" and the tab
    // names).  Guitars/Basses strips never open this picker (no arm LED).
    menu.addSectionHeader (prefix.startsWith("mixer_vox_") ? "Vocal Input" : "LiveInst Input");

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

            beginParamUndoGesture (self->mProcessor.apvts, prefix + "_inputChannelIdx"); // Task 6 (12-iv)
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

void MixerPage::setInstStripKitMissing (int idx, bool b)
{
    auto it = mInstStrips.find (idx);
    if (it == mInstStrips.end() || ! it->second) return;
    it->second->setKitMissing (b);
}

void MixerPage::addInstChannelAtIndex(int idx)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxInstStrips) return;
    if (mInstStrips.count(idx) > 0) return;

    const juce::String prefix = "mixer_inst_" + juce::String(idx);
    const juce::String name   = "LiveInst " + juce::String(idx + 1);
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
    // Same tab-agreement rule as the Vox strip above: <InstNames> persists the
    // strip name, the Inst tab persists its own, so the rename must reach both.
    strip->onNameChanged = [this, idx](const juce::String& newName) {
        if (onChannelRenamed) onChannelRenamed (StripKind::Inst, idx, newName);
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
    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  This is the project-load
    // reset; without it a plugin strip from the OUTGOING project survived into
    // the incoming one, bound to an APVTS prefix whose engine no longer exists.
    mPluginStrips.clear();   mPluginOrder  .clear();
    mDirectStrips.clear();   mDirectOrder  .clear();   // QA-TrueLevel SC-10
    // UNCONDITIONAL hide (2026-08-17): gating it on the flag skipped the
    // sweep whenever the flag was already false with the strip leaked
    // visible (the removePluginChannel hole above) - the ghost then rode
    // straight into the incoming project.
    mPluginsBusActive = false;
    if (mPluginsBusStrip) mPluginsBusStrip->setVisible(false);
    // QA-Layout T10: project-load reset for every user-added secondary bus --
    // the incoming project's <Buses> element re-activates its own set.
    // (Vox2/Inst2/3 previously leaked across projects; with activation now
    // project-persisted the reset is the correct half of that contract.)
    auto dropBus = [] (bool& flag, std::unique_ptr<MixerTrackStrip>& strip)
    {
        flag = false;
        if (strip) strip->setVisible (false);
    };
    dropBus (mVoxBus2Active,     mVoxBus2Strip);
    dropBus (mInstBus2Active,    mInstBus2Strip);
    dropBus (mInstBus3Active,    mInstBus3Strip);
    dropBus (mLayersBus2Active,  mLayersBus2Strip);
    dropBus (mBassBus2Active,    mBassBus2Strip);
    dropBus (mClipsBus2Active,   mClipsBus2Strip);
    dropBus (mPluginsBus2Active, mPluginsBus2Strip);
    mBusEverRouted.clear();
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
                                                   : juce::String ("LiveInst " + juce::String (idx + 1));
}

juce::String MixerPage::getPluginStripName (int idx) const
{
    auto it = mPluginStrips.find (idx);
    return (it != mPluginStrips.end() && it->second) ? it->second->getName()
                                                     : juce::String ("Plugin " + juce::String (idx + 1));
}

void MixerPage::addAuxChannelAtIndex(int idx)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxAuxStrips) return;
    if (mAuxStrips.count(idx) > 0) return;   // already exists

    const juce::String prefix = "mixer_aux_" + juce::String(idx);
    const juce::String name   = "Aux " + juce::String(idx + 1);

    // Register the BaySickGraph InsertNode + APVTS params (lazy / idempotent).
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
// the order entry; the underlying BaySickGraph InsertNode + APVTS params stay
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

void MixerPage::removePluginChannel(int pageIndex)
{
    mStripCacheDirty = true;
    mPluginStrips.erase(pageIndex);
    mPluginOrder.erase(std::remove(mPluginOrder.begin(), mPluginOrder.end(), pageIndex),
                       mPluginOrder.end());
    // Last plugin strip gone -> the bus strip retires with it, matching the
    // secondary Vox/Inst buses rather than the always-visible FX/Master pair.
    // HIDE as well as deactivate (Jeff's find, 2026-08-17): layout skips an
    // inactive bus, so a still-visible strip freezes at its last bounds and
    // floats over whatever occupies that space next - his repro was a ghost
    // "Plugins Bus" at minimized-window size covering the Inst Bus, cable
    // dangling, surviving into the NEXT project because the load reset's
    // hide was gated on the flag this line had already cleared.
    if (mPluginStrips.empty())
    {
        mPluginsBusActive = false;
        if (mPluginsBusStrip) mPluginsBusStrip->setVisible (false);
    }
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
        if (prefix == "mixer_drumsbus2") return kDrumsBus2;
        if (prefix == "mixer_fx")        return kFxBus;
        if (prefix == "mixer_clipsbus")  return kClipsBus;
        if (prefix == "mixer_voxbus")    return kVoxBus;
        if (prefix == "mixer_instbus")   return kInstBus;
        if (prefix == "mixer_voxbus2")   return kVoxBus2;
        if (prefix == "mixer_instbus2")  return kInstBus2;
        if (prefix == "mixer_instbus3")  return kInstBus3;
        if (prefix == "mixer_layersbus2")  return kLayersBus2;   // T10
        if (prefix == "mixer_bassbus2")    return kBassBus2;
        if (prefix == "mixer_clipsbus2")   return kClipsBus2;
        if (prefix == "mixer_pluginbus2")  return kPluginsBus2;
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

    // Which kind of routing destination a parameter id names.  All three kinds
    // must be swept when a destination strip is deleted -- a main line left
    // pointing at a dead channel drops that strip out of the mix silently.
    enum class DestKind
    {
        MainLine0,   // <prefix>_sendTo         -- the strip's permanent output
        MainLineN,   // <prefix>_mainOut{N}_to  -- extra main outs, -1 = off
        Send         // <prefix>_send{N}_to     -- aux send, -1 = off
    };

    // Called once per registered parameter by every sweep, so the suffix tables
    // are built once at first use rather than rebuilt (and freed) on each call
    // -- with lazily-registered per-strip params the walk covers thousands of
    // ids.
    const juce::String& sendSuffix (int n)
    {
        static const juce::String suffixes[4] = { "_send0_to", "_send1_to",
                                                  "_send2_to", "_send3_to" };
        return suffixes[n];
    }

    const juce::String& mainLineSuffix (int n)
    {
        // Mirrors MixerChannelIds::mainOutParamId; the two must agree.
        static_assert (MixerChannelIds::kMaxMainOutsPerStrip == 4,
                       "mainLineSuffix needs one entry per main-out line");
        static const juce::String suffixes[4] = { "_sendTo", "_mainOut1_to",
                                                  "_mainOut2_to", "_mainOut3_to" };
        return suffixes[n];
    }

    // Splits a destination id into its kind and the owning strip's prefix.
    bool isSendDestId (const juce::String& id, DestKind& kind, juce::String& stripPrefix)
    {
        for (int n = 0; n < MixerChannelIds::kMaxMainOutsPerStrip; ++n)
        {
            const juce::String& sfx = mainLineSuffix (n);
            if (! id.endsWith (sfx)) continue;
            kind = (n == 0) ? DestKind::MainLine0 : DestKind::MainLineN;
            stripPrefix = id.dropLastCharacters (sfx.length());
            return true;
        }
        for (int n = 0; n < 4; ++n)
        {
            const juce::String& sfx = sendSuffix (n);
            if (! id.endsWith (sfx)) continue;
            kind = DestKind::Send;
            stripPrefix = id.dropLastCharacters (sfx.length());
            return true;
        }
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
        // T10: the secondary group buses join the flag-gated set -- their
        // hide path is the L14 lifecycle in laidOutBus, not the generic
        // membership check.
        return chId == kMaster   || chId == kFxBus
            || chId == kVoxBus2  || chId == kInstBus2
            || chId == kInstBus3 || chId == kRustyDrumsBus
            || chId == kLayersBus2 || chId == kBassBus2
            || chId == kClipsBus2  || chId == kPluginsBus2;
        // kPluginsBus deliberately NOT here (Jeff, 2026-07-31): a plugin strip's
        // main-out is unlocked by spec and moves under Layers or Bass, and when
        // it does the Plugins bus has no members and must disappear -- the same
        // "shown when used, gone when nothing feeds it" rule every other
        // member-gated bus follows.
    }

    // Walks every parameter whose id is a routing destination on a strip prefix
    // -- both main-out lines and aux sends.  For each match where the current
    // value equals targetChId, calls onMatch (which decides what to reset to).
    // onMatch returns the new natural value.
    void sweepSendsTargeting (juce::AudioProcessor& processor,
                              int targetChId,
                              std::function<float (juce::RangedAudioParameter*,
                                                    int /*stripChId*/,
                                                    DestKind)> onMatch)
    {
        for (auto* p : processor.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (rp == nullptr) continue;
            const juce::String id = rp->getParameterID();
            DestKind     kind = DestKind::Send;
            juce::String stripPrefix;
            if (! isSendDestId (id, kind, stripPrefix)) continue;

            const float current = rp->convertFrom0to1 (rp->getValue());
            if ((int) current != targetChId) continue;

            const int stripChId = channelIdFromMixerPrefix (stripPrefix);
            const float newVal = onMatch (rp, stripChId, kind);
            writeParamNatural (rp, newVal);
        }
    }

}

void MixerPage::deleteAuxStrip (int idx, int auxChannelId)
{
    beginParamUndoGesture (mProcessor.apvts, MixerChannelIds::prefixFromChannelId (auxChannelId) + "_level"); // Task 6 (12-iv)
    // Reset every route that targeted this aux.
    //  - Main line 0 (_sendTo)      → natural parent for that strip's channel id
    //  - Extra main lines / sends   → -1 (inactive)
    // An extra main line goes inactive rather than to the natural parent: line 0
    // already feeds that, and two lines on one destination is a double-sum.
    sweepSendsTargeting (mProcessor, auxChannelId,
        [] (juce::RangedAudioParameter*, int stripChId, DestKind kind) -> float
        {
            if (kind == DestKind::MainLine0)
                return (float) MixerChannelIds::defaultSendTo (stripChId);
            return -1.0f;
        });

    // Remove the UI strip.
    removeAuxChannel (idx);

    // Free the audio-graph InsertNode.  Re-creating an aux at the same idx
    // calls ensureAuxInsert which lazily reallocates.
    //
    // THREAD SAFETY: the aux's PassiveStripTask persists for the project
    // lifetime by design, so the audio thread reaches this node through
    // processInsert on every block and can be inside its rack / EQs right now.
    // Raise the project-load shield and settle FIRST so the in-flight block
    // drains before the node is freed -- the same order clearAllAuxInserts and
    // loadBaySickRustyDrumsKit use.  Costs a brief audio bail on a destructive
    // gesture.  shieldWasUp keeps it nest-aware.  Do NOT unregister the render
    // task here: its persistence is deliberate and processInsert no-ops on a
    // null node.
    const bool shieldWasUp = mProcessor.isProjectLoadInProgress();
    mProcessor.setProjectLoadInProgress (true);
    if (! shieldWasUp) mProcessor.settleAudioThread();
    mProcessor.mVibeGraph.removeInsertNode (BaySickGraph::InsertKind::Aux, idx);
    mProcessor.setProjectLoadInProgress (shieldWasUp);

    // Routing graph rebuild on the next block picks up the param changes.
    if (onAudioStripRenamed) onAudioStripRenamed();
    repaint();
}

void MixerPage::deleteSecondaryBus (int channelId)
{
    using namespace MixerChannelIds;
    if (channelId != kVoxBus2 && channelId != kInstBus2 && channelId != kInstBus3
        && channelId != kLayersBus2 && channelId != kBassBus2
        && channelId != kClipsBus2  && channelId != kPluginsBus2)
        return;   // primary buses + master never deletable

    const int parentBus = (channelId == kVoxBus2)    ? kVoxBus
                        : (channelId == kLayersBus2) ? kLayersBus
                        : (channelId == kBassBus2)   ? kBassBus
                        : (channelId == kClipsBus2)  ? kClipsBus
                        : (channelId == kPluginsBus2) ? kPluginsBus
                                                      : kInstBus;

    beginParamUndoGesture (mProcessor.apvts, prefixFromChannelId (channelId) + "_level"); // Task 6 (12-iv)
    // Reroute any strip whose main line 0 targets this bus → parent bus.
    // Extra main lines and sends targeting it go to -1 (inactive) so we don't
    // double-up the parent bus, which line 0 may already feed.
    sweepSendsTargeting (mProcessor, channelId,
        [parentBus] (juce::RangedAudioParameter*, int /*stripChId*/, DestKind kind) -> float
        {
            return kind == DestKind::MainLine0 ? (float) parentBus : -1.0f;
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
    else if (channelId == kLayersBus2)
    {
        if (mLayersBus2Strip) mLayersBus2Strip->setVisible (false);
        mLayersBus2Active = false;
    }
    else if (channelId == kBassBus2)
    {
        if (mBassBus2Strip) mBassBus2Strip->setVisible (false);
        mBassBus2Active = false;
    }
    else if (channelId == kClipsBus2)
    {
        if (mClipsBus2Strip) mClipsBus2Strip->setVisible (false);
        mClipsBus2Active = false;
    }
    else if (channelId == kPluginsBus2)
    {
        if (mPluginsBus2Strip) mPluginsBus2Strip->setVisible (false);
        mPluginsBus2Active = false;
    }

    // L14: a deleted bus starts a fresh lifecycle if re-added.
    mBusEverRouted[channelId] = false;

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
    // An audio-row strip has no <...Names> list of its own: the owning Clips tab
    // is where the name is stored, and project load pushes that tab name back
    // over the strip.  Renaming the strip therefore has to rename the tab -- and
    // a row with no Clips tab behind it has nowhere to put the name at all, so
    // the editor refuses that rename and snaps the label back rather than
    // letting the edit look accepted until the next load eats it.
    strip->onNameChanged = [this, row](const juce::String& newName) {
        if (onChannelRenamed)    onChannelRenamed (StripKind::Audio, row, newName);
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
        case StripKind::Plugin:
            if (auto it = mPluginStrips.find(pageIdx); it != mPluginStrips.end())
                it->second->setTrackName(newName);
            break;
        case StripKind::Direct:
            if (auto it = mDirectStrips.find(pageIdx); it != mDirectStrips.end())
                it->second->setTrackName(newName);
            break;
        // Vox / Inst keep their own setters (the sfizz program pick and the
        // project-load name restore both call them directly); the enum cases
        // route through those rather than duplicating the relayout + Effects
        // dropdown refresh they carry.
        case StripKind::Vox:   setVoxStripName  (pageIdx, newName); break;
        case StripKind::Inst:  setInstStripName (pageIdx, newName); break;
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

std::vector<MixerPage::StemPickEntry> MixerPage::getStemPickEntries() const
{
    using namespace MixerChannelIds;
    std::vector<StemPickEntry> out;

    auto add = [&out] (const MixerTrackStrip* s, int chId, bool defChecked)
    {
        if (s == nullptr || ! s->isVisible()) return;
        out.push_back ({ chId, s->getName(), defChecked });
    };
    auto addMap = [&add] (const std::map<int, std::unique_ptr<MixerTrackStrip>>& m,
                          const std::vector<int>& order, int (*chIdOf) (int))
    {
        for (int k : order)
        {
            auto it = m.find (k);
            if (it != m.end())
                add (it->second.get(), chIdOf (k), true);
        }
    };

    add (mMasterStrip.get(),        kMaster,    false);
    add (mLayersBusStrip.get(),     kLayersBus, false);
    if (mLayersBus2Active) add (mLayersBus2Strip.get(), kLayersBus2, false);   // T10
    addMap (mLayerStrips, mLayerTabOrder, &layerInsert);
    add (mBassBusStrip.get(),       kBassBus,   false);
    if (mBassBus2Active)   add (mBassBus2Strip.get(),   kBassBus2,   false);   // T10
    addMap (mBassStrips,  mBassTabOrder,  &bassInsert);
    add (mDrumsBusStrip.get(),      kDrumsBus,  false);
    add (mDrumsBus2Strip.get(),     kDrumsBus2, false);   // QA-SOUNDNESS
    addMap (mDrumStrips,  mDrumSlotOrder, &drumInsert);
    add (mAudioClipsBusStrip.get(), kClipsBus,  false);
    if (mClipsBus2Active)  add (mClipsBus2Strip.get(),  kClipsBus2,  false);   // T10
    addMap (mAudioStrips, mAudioRowOrder, &audioInsert);
    add (mVoxBusStrip.get(),        kVoxBus,    false);
    if (mVoxBus2Active)  add (mVoxBus2Strip.get(),  kVoxBus2,  false);
    addMap (mVoxStrips,   mVoxOrder,      &voxInsert);
    add (mInstBusStrip.get(),       kInstBus,   false);
    if (mInstBus2Active) add (mInstBus2Strip.get(), kInstBus2, false);
    if (mInstBus3Active) add (mInstBus3Strip.get(), kInstBus3, false);
    addMap (mInstStrips,  mInstOrder,     &instInsert);
    if (mRustyDrumsBusActive) add (mRustyDrumsBusStrip.get(), kRustyDrumsBus, false);
    if (mPluginsBusActive) { add (mPluginsBusStrip.get(), kPluginsBus, false);
                             addMap (mPluginStrips, mPluginOrder, &pluginInsert); }
    addMap (mDirectStrips, mDirectOrder, &directInsert);   // QA-TrueLevel SC-10
    if (mPluginsBus2Active) add (mPluginsBus2Strip.get(), kPluginsBus2, false);   // T10
    addMap (mRustyStrips, mRustyOrder,    &rustyInsert);
    add (mFXBusStrip.get(),         kFxBus,     false);
    addMap (mAuxStrips,   mAuxOrder,      &auxStrip);

    return out;
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
        if (slot >= 0 && slot < kMaxDrumPages)
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
    // QA-UndoCoverage Task 6: ctor-time model->APVTS mirror is programmatic.
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
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
    for (int slot = 0; slot < kMaxDrumPages; ++slot)
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
void MixerPage::parentHierarchyChanged()
{
    // Peer-keyed suspend -- the SAME rule VUMeter, DBFSMeter and SlotComponent
    // already apply to their own vblank attachments.  This page did not follow
    // it, and with the windowed shell that became a real cost: a closed page is
    // no longer destroyed, it survives detached and peerless, so its per-strip
    // meter drain and its 30 Hz page poll kept running for a window nobody was
    // looking at.  That cost scales with STRIP COUNT, which is exactly the axis
    // the instance caps are being re-evaluated along.
    //
    // Nothing here affects audio: the audio thread CAS-maxes and stores its peak
    // atomics every block whether or not a reader exists, so suspending the
    // reader removes message-thread work only.  On reopen the atomics already
    // hold the current block's values, so meters are live within one block --
    // what is lost is only the history for the period nobody was watching.
    const bool onScreen = getPeer() != nullptr;

    if (onScreen && mVBlank == nullptr)
        mVBlank = std::make_unique<juce::VBlankAttachment> (this, [this] { onVBlank(); });
    else if (! onScreen && mVBlank != nullptr)
        mVBlank.reset();

    if (onScreen)
    {
        if (! isTimerRunning()) startTimerHz (30);
    }
    else if (isTimerRunning())
    {
        stopTimer();
    }
}

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

    // Detect main-out changes on any strip and re-layout so strips visually
    // move between bus groups when their main-out cable is rerouted, and so a
    // bus that only an EXTRA main line feeds appears (or disappears with it).
    {
        using namespace MixerChannelIds;
        // The cached value is a fold of ALL of the strip's main-out lines, not
        // line 0's destination: it exists purely as a change key.
        auto check = [&](int chId, const juce::String& prefix) -> bool
        {
            // UNSIGNED fold: 1009^3 is over a billion, so line 0's destination
            // alone overflows a signed int on nearly every strip, and signed
            // overflow is undefined.  Unsigned wraparound is defined, and the
            // value is only ever compared for equality, so narrowing once at
            // the end costs nothing.
            bool     haveAny = false;
            unsigned fold    = 0;
            for (int line = 0; line < kMaxMainOutsPerStrip; ++line)
                if (auto* p = mProcessor.apvts.getRawParameterValue (mainOutParamId (prefix, line)))
                {
                    haveAny = true;
                    fold    = fold * 1009u + (unsigned) ((int) p->load() + 1);
                }
            if (! haveAny) return false;

            const int key = (int) (fold & 0x7fffffffu);

            auto it = mLastSendToCache.find(chId);
            if (it == mLastSendToCache.end() || it->second != key)
            {
                mLastSendToCache[chId] = key;
                return true;
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
        // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  Plugin inserts are
        // main-out UNLOCKED by spec (a VST strip moves under Layers or Bass), so
        // omitting them from this scan is worse here than for a locked kind: the
        // reroute the feature exists for would never re-lay-out.
        for (int k : mPluginOrder)
            anyChange |= check(pluginInsert(k), "mixer_plugin_" + juce::String(k));
        for (int k : mDirectOrder)
            anyChange |= check(directInsert(k), "mixer_direct_" + juce::String(k));

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
    drainStereoBus (mDrumsBus2Strip    .get(), kDrumsBus2,     mProcessor.mDrumsBus2PeakDbL,     mProcessor.mDrumsBus2PeakDbR);   // QA-SOUNDNESS
    drainStereoBus (mFXBusStrip        .get(), kFxBus,         mProcessor.mFxBusPeakDbL,         mProcessor.mFxBusPeakDbR);
    drainStereoBus (mAudioClipsBusStrip.get(), kClipsBus,      mProcessor.mAudioClipsBusPeakDbL, mProcessor.mAudioClipsBusPeakDbR);
    drainStereoBus (mVoxBusStrip       .get(), kVoxBus,        mProcessor.mVoxBusPeakDbL,        mProcessor.mVoxBusPeakDbR);
    drainStereoBus (mInstBusStrip      .get(), kInstBus,       mProcessor.mInstBusPeakDbL,       mProcessor.mInstBusPeakDbR);
    drainStereoBus (mRustyDrumsBusStrip.get(), kRustyDrumsBus, mProcessor.mRustyDrumsBusPeakDbL, mProcessor.mRustyDrumsBusPeakDbR);   // J-7b
    drainStereoBus (mPluginsBusStrip   .get(), kPluginsBus,    mProcessor.mPluginsBusPeakDbL,    mProcessor.mPluginsBusPeakDbR);      // TS6
    // QA-Eg: cable overlay reads each strip's cached peak (MixerTrackStrip::
    // getCurrentPeakDb) for telemetry-driven alpha + warning-color animation.
    // Repaint here so cable updates land in the same vblank as the strip
    // meters they're sourced from -- no race between cable read and strip drain.

    // QA-AudioMeters (2026-05-24): unified per-insert drain via the new
    // BaySickDAWProcessor::drainInsertPeakDbStereo accessor.  Reads + exchange-
    // resets the m<Kind>InsertPeakDb*L/R[index] mirror that drainMeterAtomicsForUI
    // populates from BaySickGraph's per-kind public-member arrays.  Audio kind
    // shares this path (no more inline mAudioRowPeakDb*L/R exchange-reset).
    auto drainStereoInsert = [&] (BaySickGraph::InsertKind kind, int idx, MixerTrackStrip* strip)
    {
        if (! strip) return;
        const auto [pkL, pkR] = mProcessor.drainInsertPeakDbStereo (kind, idx);
        strip->setStereoLevel (pkL, pkR);
        // QA-RustyMeter: feed the split meter's scrolling RMS top half.
        const auto [rmL, rmR] = mProcessor.drainInsertRmsDbStereo (kind, idx);
        strip->setRmsStereo (rmL, rmR);
    };

    for (auto& [pageIdx, strip] : mLayerStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Layer, pageIdx, strip.get());
    for (auto& [pageIdx, strip] : mBassStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Bass, pageIdx, strip.get());
    for (auto& [slot, strip] : mDrumStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Drum, slot, strip.get());
    for (auto& [row, strip] : mAudioStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Audio, row, strip.get());
    for (auto& [idx, strip] : mAuxStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Aux, idx, strip.get());
    for (auto& [idx, strip] : mVoxStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Vox, idx, strip.get());
    for (auto& [idx, strip] : mInstStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Inst, idx, strip.get());
    // J-5 (2026-05-03): per-Rusty-strip peak meter drain.
    for (auto& [idx, strip] : mRustyStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Rusty, idx, strip.get());
    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.
    for (auto& [idx, strip] : mPluginStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Plugin, idx, strip.get());
    for (auto& [idx, strip] : mDirectStrips)
        drainStereoInsert (BaySickGraph::InsertKind::Direct, idx, strip.get());   // QA-TrueLevel

    if (mVoxBus2Strip)  drainStereoBus (mVoxBus2Strip .get(), kVoxBus2,  mProcessor.mVoxBus2PeakDbL,  mProcessor.mVoxBus2PeakDbR);
    if (mInstBus2Strip) drainStereoBus (mInstBus2Strip.get(), kInstBus2, mProcessor.mInstBus2PeakDbL, mProcessor.mInstBus2PeakDbR);
    if (mInstBus3Strip) drainStereoBus (mInstBus3Strip.get(), kInstBus3, mProcessor.mInstBus3PeakDbL, mProcessor.mInstBus3PeakDbR);
    // T10: secondary group buses.
    if (mLayersBus2Strip)  drainStereoBus (mLayersBus2Strip .get(), kLayersBus2,  mProcessor.mLayersBus2PeakDbL,  mProcessor.mLayersBus2PeakDbR);
    if (mBassBus2Strip)    drainStereoBus (mBassBus2Strip   .get(), kBassBus2,    mProcessor.mBassBus2PeakDbL,    mProcessor.mBassBus2PeakDbR);
    if (mClipsBus2Strip)   drainStereoBus (mClipsBus2Strip  .get(), kClipsBus2,   mProcessor.mClipsBus2PeakDbL,   mProcessor.mClipsBus2PeakDbR);
    if (mPluginsBus2Strip) drainStereoBus (mPluginsBus2Strip.get(), kPluginsBus2, mProcessor.mPluginsBus2PeakDbL, mProcessor.mPluginsBus2PeakDbR);

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
    pushPeak(mDrumsBus2Strip    .get());
    pushPeak(mFXBusStrip        .get());
    pushPeak(mAudioClipsBusStrip.get());
    pushPeak(mVoxBusStrip       .get());
    pushPeak(mInstBusStrip      .get());
    pushPeak(mRustyDrumsBusStrip.get());
    pushPeak(mVoxBus2Strip      .get());
    pushPeak(mInstBus2Strip     .get());
    pushPeak(mInstBus3Strip     .get());
    pushPeak(mLayersBus2Strip   .get());   // T10
    pushPeak(mBassBus2Strip     .get());
    pushPeak(mClipsBus2Strip    .get());
    pushPeak(mPluginsBus2Strip  .get());

    for (auto& kv : mLayerStrips) pushPeak(kv.second.get());
    for (auto& kv : mBassStrips)  pushPeak(kv.second.get());
    for (auto& kv : mDrumStrips)  pushPeak(kv.second.get());
    for (auto& kv : mAudioStrips) pushPeak(kv.second.get());
    for (auto& kv : mAuxStrips)   pushPeak(kv.second.get());
    for (auto& kv : mVoxStrips)   pushPeak(kv.second.get());
    for (auto& kv : mInstStrips)  pushPeak(kv.second.get());
    for (auto& kv : mRustyStrips) pushPeak(kv.second.get());
    for (auto& kv : mPluginStrips) pushPeak(kv.second.get());   // TS6
    for (auto& kv : mDirectStrips) pushPeak(kv.second.get());   // QA-TrueLevel
    if (mPluginsBusStrip) pushPeak(mPluginsBusStrip.get());     // TS6

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
    beginParamUndoGesture (mProcessor.apvts, prefix + "_collapsed"); // Task 6 (12-iv)
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

    // Bucket each strip by its line-0 destination channel id.
    struct Member { MixerTrackStrip* strip; int width; int chId; int dest; };
    std::map<int, std::vector<Member>> buckets;

    // Destinations reached by any strip's EXTRA main-out lines.  A strip sits
    // in exactly one group (line 0's), so these targets need tracking
    // separately or a bus fed only by an extra line would count as empty.
    std::set<int> extraMainTargets;

    auto bucketPush = [&](MixerTrackStrip* s, int chId, int w, const juce::String& prefix) {
        int dest = getSendTo(prefix, defaultSendTo(chId));
        buckets[dest].push_back({ s, w, chId, dest });
        for (int line = 1; line < kMaxMainOutsPerStrip; ++line)
            if (auto* p = mProcessor.apvts.getRawParameterValue (mainOutParamId (prefix, line)))
                if (const int d = (int) p->load(); d >= 0)
                    extraMainTargets.insert (d);
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
    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  addPluginChannel builds the
    // strip and parents it, but with no bucket entry here it was never given
    // bounds: a zero-size child at 0,0.  Everything else about it was correct,
    // which is why it read as "no strip is created".  LayerChannel width to match
    // the strip type addPluginChannel constructs.
    for (int k : mPluginOrder)
    {
        auto it = mPluginStrips.find(k);
        if (it != mPluginStrips.end())
            bucketPush(it->second.get(), pluginInsert(k), layerW,
                       "mixer_plugin_" + juce::String(k));
    }
    // QA-TrueLevel SC-10: _sendTo is the master, so the bucket lands under the
    // master strip -- the same grouping a Layers strip moved to master gets.
    for (int k : mDirectOrder)
    {
        auto it = mDirectStrips.find(k);
        if (it != mDirectStrips.end())
            bucketPush(it->second.get(), directInsert(k), layerW,
                       "mixer_direct_" + juce::String(k));
    }

    // QA-Layout T10: a routed-to secondary bus SELF-ACTIVATES -- preset and
    // project loads write _sendTo values before any activation flag arrives,
    // and members bucketed under an unrendered bus would otherwise never get
    // bounds.  Activation is deferred via callAsync because activate*()
    // re-enters this layout.
    {
        struct SelfAct { int chId; bool active; bool (MixerPage::*fn)(); };
        const SelfAct kSelfAct[] = {
            { kVoxBus2,     mVoxBus2Active,     &MixerPage::activateVoxBus2     },
            { kInstBus2,    mInstBus2Active,    &MixerPage::activateInstBus2    },
            { kInstBus3,    mInstBus3Active,    &MixerPage::activateInstBus3    },
            { kLayersBus2,  mLayersBus2Active,  &MixerPage::activateLayersBus2  },
            { kBassBus2,    mBassBus2Active,    &MixerPage::activateBassBus2    },
            { kClipsBus2,   mClipsBus2Active,   &MixerPage::activateClipsBus2   },
            { kPluginsBus2, mPluginsBus2Active, &MixerPage::activatePluginsBus2 },
        };
        juce::Component::SafePointer<MixerPage> safeThis (this);
        for (const auto& sa : kSelfAct)
        {
            if (sa.active) continue;
            auto it = buckets.find (sa.chId);
            const bool routedTo = (it != buckets.end() && ! it->second.empty())
                               || extraMainTargets.count (sa.chId) > 0;
            if (! routedTo) continue;
            auto fn = sa.fn;
            juce::MessageManager::callAsync ([safeThis, fn]
            {
                if (safeThis) ((*safeThis).*fn)();
            });
        }
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
        // A bus fed only by another strip's EXTRA main-out line has no bucket
        // members -- buckets key off line 0 -- but audio does flow into it.
        // Hiding it would leave a live route with no visible strip, no socket
        // and so no cable the user could see or cut.
        const bool routedTo = hasMembers || extraMainTargets.count (busChId) > 0;

        // QA-ProjectSave docket 18 (2026-07-26): an empty bus is hidden and
        // consumes no width, so its group gap closes instead of leaving a hole.
        // "Routed to" is the whole test: buckets are keyed by each EXISTING
        // strip's line-0 destination, so re-pointing a main-out at a bus
        // re-buckets that strip INTO the bus's group.  (Sends never enter into
        // it; _sendN_to only ever lands on an aux strip.)
        //
        // The flag-gated buses opt out: they are created by explicit user
        // action and must appear immediately, before anything is routed to them.
        //
        // L14 (QA-Layout T10): the user-added secondary buses live by the
        // has-ever-had-route lifecycle instead -- a fresh bus stays visible
        // while never-routed, and once it HAS had members and empties again
        // it auto-deactivates (flag only; no param sweep -- it is already
        // empty, and deleteSecondaryBus would recurse into this layout).
        if (routedTo) mBusEverRouted[busChId] = true;
        if (! routedTo && isSecondaryBus (busChId) && getBusEverRouted (busChId))
        {
            // No onAudioStripRenamed here -- it can re-enter this layout;
            // pickers read the active flag live on their next open anyway.
            busStrip.setVisible (false);
            deactivateBusFlagOnly (busChId);
            return;
        }
        if (! routedTo && ! isAlwaysVisibleBus (busChId))
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
        if (dst >= kAuxBase && dst < kAuxBase + kMaxAuxStrips)
            layoutGroup(members, x, juce::Colour(kEffectsTabPink), dst);
    }
    x += kGroupSep;

    // Clips Bus sits between FX and the instrument buses - matches Builder tab color.
    laidOutBus(*mAudioClipsBusStrip,  kClipsBus,  VC::Warm);
    // QA-Layout T10 (L13): Clips Bus 2 (when active) sits next to Clips Bus.
    if (mClipsBus2Active && mClipsBus2Strip)
        laidOutBus(*mClipsBus2Strip, kClipsBus2, VC::Warm);

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

    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  Jeff's placement: AFTER the
    // whole Inst family (all three buses and their strips) and BEFORE Layers, not
    // wedged in among the secondary Inst buses.  Gated on mPluginsBusActive so the
    // bus takes no mixer width until a plugin tab exists.
    if (mPluginsBusActive && mPluginsBusStrip)
        laidOutBus(*mPluginsBusStrip, kPluginsBus, VC::Purple);
    // QA-Layout T10 (L13): Plugins Bus 2 (when active) sits next to Plugins Bus.
    if (mPluginsBus2Active && mPluginsBus2Strip)
        laidOutBus(*mPluginsBus2Strip, kPluginsBus2, VC::Purple);

    laidOutBus(*mLayersBusStrip,      kLayersBus, VC::LayerCol[0]);
    // QA-Layout T10 (L13): Layers Bus 2 (when active) sits next to Layers Bus.
    if (mLayersBus2Active && mLayersBus2Strip)
        laidOutBus(*mLayersBus2Strip, kLayersBus2, VC::LayerCol[0]);
    laidOutBus(*mBassBusStrip,        kBassBus,   VC::BassCol[0]);
    // QA-Layout T10 (L13): Bass Bus 2 (when active) sits next to Bass Bus.
    if (mBassBus2Active && mBassBus2Strip)
        laidOutBus(*mBassBus2Strip, kBassBus2, VC::BassCol[0]);
    // J-5 (2026-05-03): RustyDrums Bus + 13 inserts sit between Bass and
    // Drums so they read as a sibling of Drums.  Visible only when
    // mRustyDrumsBusActive (= a BaySickRustyDrums singleton has been spawned).
    if (mRustyDrumsBusActive && mRustyDrumsBusStrip)
        laidOutBus(*mRustyDrumsBusStrip, kRustyDrumsBus, VC::DrumsCol);
    laidOutBus(*mDrumsBusStrip,       kDrumsBus,  VC::DrumsCol);
    // QA-SOUNDNESS (2026-08-07): kit 2's bus sits immediately right of kit 1's.
    // No activation gate -- laidOutBus's own membership test hides it while no
    // bank-2 drum strip routes to it, exactly as it does for Drums Bus.
    laidOutBus(*mDrumsBus2Strip,      kDrumsBus2, VC::DrumsCol);

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
void MixerPage::paint(juce::Graphics& g)
{
    g.fillAll(VC::Bg);
    g.setColour(VC::Accent);
    g.fillRect(kFixedPanelW, 0, 2, getHeight());
}
