#include "MixerPage.h"

// ─────────────────────────────────────────────────────────────────────────────
// Direct Routing label — vertical-text panel between Master and FX Bus group,
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
// colors — helps the user find "where to do things".
static constexpr juce::uint32 kMixerTabPurple = 0xff7b2fbe;  // Master strip
static constexpr juce::uint32 kEffectsTabPink = 0xffce3f8e;  // FX Bus + aux strips

// Accent-color resolver used by layoutScrollContent — maps (channelId, current
// main-out destination) to the strip's top-bar accent color.
static juce::Colour pickStripColor(int chId, int destChannelId)
{
    using namespace MixerChannelIds;
    // Aux: always Effects-tab pink
    if (chId >= kAuxBase && chId < kAuxBase + 16) return juce::Colour(kEffectsTabPink);
    // Colored bus groups — track the main-out destination
    if (destChannelId == kLayersBus) return VC::LayerCol[0];
    if (destChannelId == kBassBus)   return VC::BassCol[0];
    if (destChannelId == kDrumsBus)  return VC::DrumsCol;
    if (destChannelId == kClipsBus)  return VC::Warm;
    if (destChannelId == kFxBus)     return juce::Colour(kEffectsTabPink);
    // Direct Routing / aux chain: fall back to the strip's natural color
    if (chId >= kLayerBase && chId < kLayerBase + 16) return VC::LayerCol[0];
    if (chId >= kBassBase  && chId < kBassBase  + 16) return VC::BassCol[0];
    if (chId >= kDrumBase  && chId < kDrumBase  + 16) return VC::DrumsCol;
    if (chId >= kAudioBase && chId < kAudioBase + 50) return VC::Warm;
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
// 5F-4b B3: CableOverlay — green bezier cables between strip sockets
// ─────────────────────────────────────────────────────────────────────────────
MixerPage::CableOverlay::CableOverlay(MixerPage& o) : owner(o)
{
    // hitTest() gates click-through: only intercepts near sockets or while dragging.
    setInterceptsMouseClicks(true, false);
    // R3.5 (2026-04-23): cache the bezier render in a bitmap so sibling strip
    // repaints (meter ticks 30 Hz x ~20 strips) don't force the cable overlay
    // to re-rasterize.  Cables flicker without this once enough strips exist.
    setBufferedToImage(true);
}

bool MixerPage::CableOverlay::hitTest(int x, int y)
{
    if (mDragging) return true;
    if (mPendingSendSrcId >= 0) return true;   // B5: intercept everything in send-placement mode

    auto pt = juce::Point<float>((float) x, (float) y);

    // B6: right-click near a cable should be intercepted
    if (hitTestCable(pt).srcId >= 0) return true;

    return findSocketNear(pt, 14.f, true) >= 0;
}

void MixerPage::CableOverlay::paint(juce::Graphics& g)
{
    const auto& edges = owner.mProcessor.mVibeGraph.getRoutingGraph().edges();

    for (const auto& e : edges)
    {
        // While dragging a main-out, hide the source's existing main-out cable
        if (mDragging && e.srcId == mDragSrcId && e.isMainOut)
            continue;

        auto src = owner.getSocketPosition(e.srcId);
        auto dst = owner.getSocketPosition(e.dstId);

        if (src.x < 0 || dst.x < 0) continue;

        g.setColour(e.isMainOut
            ? juce::Colour(0xff33ff88)
            : juce::Colour(0xff33ff88).withAlpha(0.55f));

        const float hDist = std::abs(dst.x - src.x);
        const float sag   = juce::jlimit(15.f, 60.f, hDist * 0.15f);

        juce::Path path;
        path.startNewSubPath(src);
        path.cubicTo(src.x, src.y + sag,
                     dst.x, dst.y + sag,
                     dst.x, dst.y);

        g.strokePath(path, juce::PathStrokeType(2.5f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    }

    // Ghost cable while dragging
    if (mDragging)
    {
        g.setColour(juce::Colour(0xff33ff88).withAlpha(0.45f));

        const float hDist = std::abs(mDragMousePos.x - mDragSrcSocket.x);
        const float sag   = juce::jlimit(15.f, 60.f, hDist * 0.15f);

        juce::Path ghost;
        ghost.startNewSubPath(mDragSrcSocket);
        ghost.cubicTo(mDragSrcSocket.x, mDragSrcSocket.y + sag,
                      mDragMousePos.x,  mDragMousePos.y + sag,
                      mDragMousePos.x,  mDragMousePos.y);

        g.strokePath(ghost, juce::PathStrokeType(2.5f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    }

    // B5: ghost cable in send-placement mode (follows cursor)
    if (mPendingSendSrcId >= 0)
    {
        auto srcSock = owner.getSocketPosition(mPendingSendSrcId);
        if (srcSock.x >= 0)
        {
            g.setColour(juce::Colour(0xff33ff88).withAlpha(0.40f));

            const float hDist = std::abs(mDragMousePos.x - srcSock.x);
            const float sag   = juce::jlimit(15.f, 60.f, hDist * 0.15f);

            juce::Path ghost;
            ghost.startNewSubPath(srcSock);
            ghost.cubicTo(srcSock.x, srcSock.y + sag,
                          mDragMousePos.x, mDragMousePos.y + sag,
                          mDragMousePos.x, mDragMousePos.y);

            float dashLengths[] = { 6.f, 4.f };
            g.strokePath(ghost, juce::PathStrokeType(2.0f),
                         juce::AffineTransform{});
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
    // B6: right-click → show cable property popup
    if (e.mods.isRightButtonDown())
    {
        auto hit = hitTestCable(e.position);
        if (hit.srcId >= 0)
            showCablePopup(e.getScreenPosition().toFloat(), hit);
        return;
    }

    // B5: send-placement mode — click commits the send
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
            // All 4 send slots full — flash the source strip
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

    // B4: main-out drag — click near a non-locked socket
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
        // Dropped on empty space or self — cancel, cable snaps back
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

    return -1;
}

int MixerPage::CableOverlay::findStripUnder(juce::Point<float> pt) const
{
    // Check all strips — is pt within their page-coords bounds?
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

    // Audio insert: any bus EXCEPT FX · Master (FX reachable only via aux-send).
    // R1 (2026-04-23): added Vox + Inst bus destinations.
    if (srcIsAudio)
        return dstIsMaster || dstId == kLayersBus || dstId == kBassBus
            || dstId == kDrumsBus || dstId == kClipsBus
            || dstId == kVoxBus  || dstId == kInstBus;

    // Aux strip: Master · FX Bus · other Aux
    if (srcIsAux)
        return dstIsMaster || dstId == kFxBus || dstIsAux;

    // R1 (2026-04-23): Vox / Inst strips - main-out limited to Master,
    // Clips Bus, and their own bus (default).  Other instrument buses are
    // explicitly excluded - live-input goes to live-input destinations.
    if (srcIsVox)  return dstIsMaster || dstId == kClipsBus || dstId == kVoxBus;
    if (srcIsInst) return dstIsMaster || dstId == kClipsBus || dstId == kInstBus;

    // Unknown strip type — be safe
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// 5F-4b B5: send-placement mode
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::CableOverlay::mouseMove(const juce::MouseEvent& e)
{
    if (mPendingSendSrcId >= 0)
    {
        mDragMousePos = e.position;
        repaint();
    }
}

bool MixerPage::CableOverlay::keyPressed(const juce::KeyPress& k)
{
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

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4b B6: cable hit-testing + right-click popup
// ═══════════════════════════════════════════════════════════════════════════════

MixerPage::CableOverlay::CableHit
MixerPage::CableOverlay::hitTestCable(juce::Point<float> pt) const
{
    const auto& edges = owner.mProcessor.mVibeGraph.getRoutingGraph().edges();

    for (const auto& e : edges)
    {
        auto src = owner.getSocketPosition(e.srcId);
        auto dst = owner.getSocketPosition(e.dstId);
        if (src.x < 0 || dst.x < 0) continue;

        // Rebuild the same bezier as paint()
        const float hDist = std::abs(dst.x - src.x);
        const float sag   = juce::jlimit(15.f, 60.f, hDist * 0.15f);

        juce::Path bezier;
        bezier.startNewSubPath(src);
        bezier.cubicTo(src.x, src.y + sag, dst.x, dst.y + sag, dst.x, dst.y);

        // Stroke into a thick region for hit detection
        juce::Path hitZone;
        juce::PathStrokeType(10.f).createStrokedPath(hitZone, bezier);

        if (hitZone.contains(pt))
        {
            CableHit hit;
            hit.srcId    = e.srcId;
            hit.dstId    = e.dstId;
            hit.isMainOut = e.isMainOut;

            if (! e.isMainOut)
            {
                // Reverse-lookup: which send slot (0..3) has this dstId?
                const juce::String prefix = MixerChannelIds::prefixFromChannelId(e.srcId);
                for (int s = 0; s < 4; ++s)
                {
                    const auto paramId = prefix + "_send" + juce::String(s) + "_to";
                    if (auto* p = owner.mProcessor.apvts.getRawParameterValue(paramId))
                        if ((int) p->load() == e.dstId) { hit.sendSlot = s; break; }
                }
            }
            return hit;
        }
    }
    return {};   // no cable hit
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
    juce::Slider       mAmountSlider;
    juce::ToggleButton mPrePostBtn;
    juce::TextButton   mDeleteBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAmountAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mPrePostAtt;
};

// Minimal popup for main-out cables (info only, not editable)
class CableMainOutPopup : public juce::Component
{
public:
    CableMainOutPopup(const juce::String& srcName, const juce::String& dstName)
    {
        mLabel.setText(srcName + " " + juce::String(juce::CharPointer_UTF8("\xe2\x86\x92")) + " " + dstName + "  (main out)",
                       juce::dontSendNotification);
        mLabel.setFont(juce::Font(11.f, juce::Font::bold));
        mLabel.setColour(juce::Label::textColourId, juce::Colour(0xff33ff88));
        mLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(mLabel);
        setSize(200, 28);
    }

    void resized() override { mLabel.setBounds(getLocalBounds().reduced(4)); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff1e2024)); }

private:
    juce::Label mLabel;
};
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

    if (hit.isMainOut)
    {
        content = std::make_unique<CableMainOutPopup>(getStripName(hit.srcId),
                                                       getStripName(hit.dstId));
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
MixerTrackStrip* MixerPage::findStripByChannelId(int channelId) const
{
    using namespace MixerChannelIds;

    if (channelId == kMaster    && mMasterStrip)        return mMasterStrip.get();
    if (channelId == kLayersBus && mLayersBusStrip)     return mLayersBusStrip.get();
    if (channelId == kBassBus   && mBassBusStrip)       return mBassBusStrip.get();
    if (channelId == kDrumsBus  && mDrumsBusStrip)      return mDrumsBusStrip.get();
    if (channelId == kFxBus     && mFXBusStrip)         return mFXBusStrip.get();
    if (channelId == kClipsBus  && mAudioClipsBusStrip) return mAudioClipsBusStrip.get();
    if (channelId == kVoxBus    && mVoxBusStrip)        return mVoxBusStrip.get();
    if (channelId == kInstBus   && mInstBusStrip)       return mInstBusStrip.get();

    for (auto& [tabId, strip] : mLayerStrips)
        if (kLayerBase + tabId == channelId) return strip.get();
    for (auto& [tabId, strip] : mBassStrips)
        if (kBassBase + tabId == channelId) return strip.get();
    for (auto& [slot, strip] : mDrumStrips)
        if (kDrumBase + slot == channelId) return strip.get();
    for (auto& [row, strip] : mAudioStrips)
        if (kAudioBase + row == channelId) return strip.get();
    for (auto& [idx, strip] : mAuxStrips)
        if (kAuxBase + idx == channelId) return strip.get();
    // R1 (2026-04-23): Vox + Inst strips
    for (auto& [idx, strip] : mVoxStrips)
        if (kVoxBase + idx == channelId) return strip.get();
    for (auto& [idx, strip] : mInstStrips)
        if (kInstBase + idx == channelId) return strip.get();

    return nullptr;
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
    // Hide BOTH of the viewport's internal scrollbars — we place our own
    // permanent horizontal scrollbar at the top of the page so the cable
    // overlay can't cover it. Allow horizontal scrolling even without the
    // viewport's own bar (we drive it via setViewPosition).
    mViewport->setScrollBarsShown(false, false,
                                  /*allowVScrollWithoutBar*/ false,
                                  /*allowHScrollWithoutBar*/ true);
    addAndMakeVisible(*mViewport);

    mTopScrollBar = std::make_unique<juce::ScrollBar>(/*isVertical*/ false);
    mTopScrollBar->setAutoHide(false);              // permanent
    mTopScrollBar->addListener(this);
    mTopScrollBar->setRangeLimits(0.0, 1.0, juce::dontSendNotification);
    mTopScrollBar->setCurrentRange(0.0, 1.0, juce::dontSendNotification);
    addAndMakeVisible(*mTopScrollBar);

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
    // R3.5 (2026-04-23): Vox + Inst BUS strips — teal + navy accents.
    mVoxBusStrip  = std::make_unique<MixerTrackStrip>("Vox Bus",
                              MixerTrackStrip::StripType::Bus, juce::Colour(0xFF0FAFA5));
    mInstBusStrip = std::make_unique<MixerTrackStrip>("Inst Bus",
                              MixerTrackStrip::StripType::Bus, juce::Colour(0xFF1C3A8A));

    mLayersBusStrip    ->setAutomationPrefix("mixer_layers");
    mBassBusStrip      ->setAutomationPrefix("mixer_bass");
    mDrumsBusStrip     ->setAutomationPrefix("mixer_drums");
    mFXBusStrip        ->setAutomationPrefix("mixer_fx");
    mAudioClipsBusStrip->setAutomationPrefix("mixer_clipsbus");
    mVoxBusStrip       ->setAutomationPrefix("mixer_voxbus");
    mInstBusStrip      ->setAutomationPrefix("mixer_instbus");

    // 5F-4a: bind each bus strip's new controls (polarity/width/bypass) to APVTS
    mLayersBusStrip    ->setApvts(mProcessor.apvts, "mixer_layers");
    mBassBusStrip      ->setApvts(mProcessor.apvts, "mixer_bass");
    mDrumsBusStrip     ->setApvts(mProcessor.apvts, "mixer_drums");
    mFXBusStrip        ->setApvts(mProcessor.apvts, "mixer_fx");
    mAudioClipsBusStrip->setApvts(mProcessor.apvts, "mixer_clipsbus");
    mVoxBusStrip       ->setApvts(mProcessor.apvts, "mixer_voxbus");
    mInstBusStrip      ->setApvts(mProcessor.apvts, "mixer_instbus");

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

    mScrollContent->addAndMakeVisible(*mLayersBusStrip);
    mScrollContent->addAndMakeVisible(*mBassBusStrip);
    mScrollContent->addAndMakeVisible(*mDrumsBusStrip);
    mScrollContent->addAndMakeVisible(*mFXBusStrip);
    mScrollContent->addAndMakeVisible(*mAudioClipsBusStrip);
    mScrollContent->addAndMakeVisible(*mVoxBusStrip);
    mScrollContent->addAndMakeVisible(*mInstBusStrip);

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
            if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
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

    // 5F-4b B3: set channel IDs on fixed strips for cable rendering
    mMasterStrip      ->setChannelId(MixerChannelIds::kMaster);
    mLayersBusStrip   ->setChannelId(MixerChannelIds::kLayersBus);
    mBassBusStrip     ->setChannelId(MixerChannelIds::kBassBus);
    mDrumsBusStrip    ->setChannelId(MixerChannelIds::kDrumsBus);
    mFXBusStrip       ->setChannelId(MixerChannelIds::kFxBus);
    mAudioClipsBusStrip->setChannelId(MixerChannelIds::kClipsBus);
    mVoxBusStrip      ->setChannelId(MixerChannelIds::kVoxBus);
    mInstBusStrip     ->setChannelId(MixerChannelIds::kInstBus);

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
    mAddInstBtn->setTooltip("Add a live-input instrument strip (up to 6)");
    mAddInstBtn->setColour(juce::TextButton::buttonColourId, VC::Surface);
    mAddInstBtn->setColour(juce::TextButton::textColourOffId, VC::Text);
    mAddInstBtn->onClick = [this] { addInstChannel(); };

    // 5F-4b B7: restore any aux strips that were in the saved project.
    // VibeGraph already has their InsertNodes (registered by restoreAuxStripsFromState
    // in setStateInformation). Create the matching UI strips.
    for (int idx : mProcessor.mVibeGraph.getAuxIndices())
        addAuxChannelAtIndex(idx);

    syncFromModel();
    syncApvtsFromMixerState();   // 5F-4a Batch 6
    startTimerHz(30);
}

MixerPage::~MixerPage()
{
    stopTimer();
}

// ─────────────────────────────────────────────────────────────────────────────
// Lazy channel creation
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::addLayerChannel(int pageIndex, const juce::String& name)
{
    // pageIndex is authoritative — matches registerLayerEngine's InsertNode
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
        if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
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
    // pageIndex is authoritative — see addLayerChannel().
    if (mBassStrips.count(pageIndex) > 0) return;

    auto strip = std::make_unique<MixerTrackStrip>(name,
        MixerTrackStrip::StripType::BassChannel, VC::BassCol[0]);
    const juce::String prefix = "mixer_bass_" + juce::String(pageIndex);
    strip->setAutomationPrefix(prefix);
    strip->setApvts(mProcessor.apvts, prefix);
    strip->setChannelId(MixerChannelIds::bassInsert(pageIndex));
    strip->onAddSendRequested = [this](int chId) {
        if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
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
        if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
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
}

// R1 (2026-04-23): Vox + Inst strip creators.  Mirror of addAuxChannel /
// addAuxChannelAtIndex.  Capped at kMaxVoxStrips / kMaxInstStrips per the
// spec lock.  ensureVoxInsert / ensureInstInsert on the processor side
// registers APVTS params + InsertNode; this side just creates the UI strip.
void MixerPage::addVoxChannel()  { addVoxChannelAtIndex  (mNextVoxIdx);  }
void MixerPage::addInstChannel() { addInstChannelAtIndex (mNextInstIdx); }

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
        if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
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
}

// R2 (2026-04-23): shared ASIO input-channel picker for Vox + Inst Arm-LED
// clicks.  Reads channel names from the AudioDeviceManager via the
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

    for (int i = 0; i < names.size(); ++i)
        menu.addItem (juce::PopupMenu::Item ("Channel " + juce::String (i + 1)
                                                + ": " + names[i])
                          .setID (100 + i)
                          .setTicked (curIdx == i && curArmed));

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

            const bool disarm = (chosen == 99);
            const int  newIdx = disarm ? -1 : (chosen - 100);

            if (auto* p = self->mProcessor.apvts.getParameter (prefix + "_inputChannelIdx"))
                p->setValueNotifyingHost (
                    p->getNormalisableRange().convertTo0to1 ((float) newIdx));
            if (auto* p = self->mProcessor.apvts.getParameter (prefix + "_arm"))
                p->setValueNotifyingHost (disarm ? 0.f : 1.f);

            self->mProcessor.setInputChannelName (prefix,
                disarm ? juce::String() : names[newIdx]);
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
        if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
    };
    strip->onFXClicked = [this](const juce::String& id) {
        if (onEffectsTabRequested) onEffectsTabRequested(id);
    };
    strip->onNameChanged = [this](const juce::String&) {
        if (getWidth() > 0) layoutScrollContent();
        if (onAudioStripRenamed) onAudioStripRenamed();
    };

    mScrollContent->addAndMakeVisible(*strip);
    mInstStrips[idx] = std::move(strip);
    mInstOrder.push_back(idx);
    mNextInstIdx = juce::jmax(mNextInstIdx, idx + 1);

    if (getWidth() > 0) resized();
    if (onAudioStripRenamed) onAudioStripRenamed();
}

void MixerPage::clearDynamicStrips()
{
    mLayerStrips.clear();    mLayerTabOrder.clear();
    mBassStrips .clear();    mBassTabOrder .clear();
    mDrumStrips .clear();    mDrumSlotOrder.clear();
    mAudioStrips.clear();    mAudioRowOrder.clear();
    mAuxStrips  .clear();    mAuxOrder     .clear();    mNextAuxIdx  = 0;
    mVoxStrips  .clear();    mVoxOrder     .clear();    mNextVoxIdx  = 0;
    mInstStrips .clear();    mInstOrder    .clear();    mNextInstIdx = 0;
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
    if (idx < 0 || idx >= 16) return;
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
        if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
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
    mAuxStrips.erase(idx);
    mAuxOrder.erase(std::remove(mAuxOrder.begin(), mAuxOrder.end(), idx),
                    mAuxOrder.end());
    // Note: InsertNode + APVTS params intentionally preserved so re-creating
    // an aux at the same idx restores its prior settings.
    if (getWidth() > 0) resized();
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
        if (mCableOverlay) mCableOverlay->startSendPlacement(chId);
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

void MixerPage::renameChannel(int tabId, const juce::String& newName)
{
    if (auto it = mLayerStrips.find(tabId); it != mLayerStrips.end())
        it->second->setTrackName(newName);
    else if (auto it2 = mBassStrips.find(tabId); it2 != mBassStrips.end())
        it2->second->setTrackName(newName);
    else if (auto it3 = mDrumStrips.find(tabId); it3 != mDrumStrips.end())
        it3->second->setTrackName(newName);   // D1.4: dynamic-drum strip rename
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
    return out;   // std::map iterates keys in ascending order — stable
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
    mMasterStrip       ->setLevel(mProcessor.mMasterPeakDb          .load(std::memory_order_relaxed));
    mLayersBusStrip    ->setLevel(mProcessor.mLayersPeakDb          .load(std::memory_order_relaxed));
    mBassBusStrip      ->setLevel(mProcessor.mBassPeakDb            .load(std::memory_order_relaxed));
    mDrumsBusStrip     ->setLevel(mProcessor.mDrumsPeakDb           .load(std::memory_order_relaxed));
    mFXBusStrip        ->setLevel(-60.0f);
    mAudioClipsBusStrip->setLevel(mProcessor.mAudioClipsBusPeakDb   .load(std::memory_order_relaxed));
    mVoxBusStrip       ->setLevel(mProcessor.mVoxBusPeakDb          .load(std::memory_order_relaxed));
    mInstBusStrip      ->setLevel(mProcessor.mInstBusPeakDb         .load(std::memory_order_relaxed));

    // Per-insert peak from each strip's own InsertNode — showing the bus
    // peak on every strip made all strips meter identically regardless of
    // which slot was actually playing. Matches the Aux pattern below.
    for (auto& [pageIdx, strip] : mLayerStrips)
        strip->setLevel(mProcessor.mVibeGraph.getInsertPeakDb(
            VibeGraph::InsertKind::Layer, pageIdx));

    for (auto& [pageIdx, strip] : mBassStrips)
        strip->setLevel(mProcessor.mVibeGraph.getInsertPeakDb(
            VibeGraph::InsertKind::Bass, pageIdx));

    for (auto& [slot, strip] : mDrumStrips)
        strip->setLevel(mProcessor.mVibeGraph.getInsertPeakDb(
            VibeGraph::InsertKind::Drum, slot));

    for (auto& [row, strip] : mAudioStrips)
    {
        if (row >= 0 && row < VibeSynthProcessor::kMaxAudioRows)
            strip->setLevel(mProcessor.mAudioRowPeakDb[row].load(std::memory_order_relaxed));
    }

    // 5F-4b B2: aux strip peak meters — driven by each InsertNode's peakDb atomic
    for (auto& [idx, strip] : mAuxStrips)
        strip->setLevel(mProcessor.mVibeGraph.getInsertPeakDb(VibeGraph::InsertKind::Aux, idx));

    // 5F-4b B3: repaint cable overlay only when scroll position changes — the
    // bezier path itself only depends on socket positions, which only move on
    // scroll or layout.  The overlay is setBufferedToImage(true) so sibling
    // strip-meter repaints don't force a re-rasterize.  Drag / flash / send-
    // placement modes self-trigger repaints via the overlay's own timer +
    // mouseDrag handlers, so we don't need to pump them here.
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

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────
void MixerPage::layoutScrollContent()
{
    using namespace MixerChannelIds;

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
        busStrip.setBounds(x, 0, busW, stripH);
        busStrip.setAccentColor(busAccent);
        x += busW;
        if (auto it = buckets.find(busChId); it != buckets.end())
            layoutGroup(it->second, x, busAccent, busChId);
        x += kGroupSep;
    };

    // ── FX Bus group ─────────────────────────────────────────────────
    mFXBusStrip->setBounds(x, 0, busW, stripH);
    mFXBusStrip->setAccentColor(juce::Colour(kEffectsTabPink));
    x += busW;
    if (auto it = buckets.find(kFxBus); it != buckets.end())
        layoutGroup(it->second, x, juce::Colour(kEffectsTabPink), kFxBus);
    // Aux-to-aux main-out chains still live visually in the FX family
    for (auto& [dst, members] : buckets)
    {
        if (dst >= kAuxBase && dst < kAuxBase + 16)
            layoutGroup(members, x, juce::Colour(kEffectsTabPink), dst);
    }
    x += kGroupSep;

    // Clips Bus sits between FX and the instrument buses — matches Builder tab color.
    laidOutBus(*mAudioClipsBusStrip,  kClipsBus,  VC::Warm);

    // R3.5 (2026-04-23): Vox + Inst BUS strips alongside Clips Bus.  Same
    // shape as other buses (full rack/EQ/fader DSP applied in PluginProcessor
    // before the accumulator drains to Master).
    laidOutBus(*mVoxBusStrip,  kVoxBus,  juce::Colour(0xFF0FAFA5));
    laidOutBus(*mInstBusStrip, kInstBus, juce::Colour(0xFF1C3A8A));

    laidOutBus(*mLayersBusStrip,      kLayersBus, VC::LayerCol[0]);
    laidOutBus(*mBassBusStrip,        kBassBus,   VC::BassCol[0]);
    laidOutBus(*mDrumsBusStrip,       kDrumsBus,  VC::DrumsCol);

    x += kSepW;
    mScrollContent->setSize(x, mScrollContent->getHeight());

    // Publish neon-line data for paintOverChildren
    if (mScrollContent)
    {
        mScrollContent->mNeonLines = std::move(neon);
        mScrollContent->repaint();
    }
    // R3.5: any layout shift moves cable sockets — invalidate the cached overlay.
    if (mCableOverlay) mCableOverlay->repaint();

    // Keep our top scrollbar in sync with the new content width.
    syncTopScrollBar();
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
        syncTopScrollBar();
    }
}

void MixerPage::syncTopScrollBar()
{
    if (!mTopScrollBar || !mViewport || !mScrollContent) return;
    const double total = juce::jmax(1.0, (double)mScrollContent->getWidth());
    const double visible = juce::jmin(total, (double)mViewport->getWidth());
    const double viewX   = (double)mViewport->getViewPositionX();
    mTopScrollBar->setRangeLimits(0.0, total, juce::dontSendNotification);
    mTopScrollBar->setCurrentRange(juce::jlimit(0.0, juce::jmax(0.0, total - visible), viewX),
                                   visible,
                                   juce::dontSendNotification);
}

void MixerPage::scrollBarMoved(juce::ScrollBar* sb, double newRangeStart)
{
    if (sb != mTopScrollBar.get() || !mViewport) return;
    mViewport->setViewPosition((int)std::round(newRangeStart),
                               mViewport->getViewPositionY());
}

void MixerPage::resized()
{
    constexpr int kTopScrollH = 10;   // top scrollbar height — always visible
    auto b = getLocalBounds();

    // ── Top scrollbar strip (above everything, spans the console area) ────
    auto topBar = b.removeFromTop(kTopScrollH);

    // Master strip (fixed, full remaining height)
    mMasterStrip->setBounds(0, b.getY(), kFixedPanelW, b.getHeight());

    int vpX = kFixedPanelW + 2;
    mViewport->setBounds(vpX, b.getY(), b.getWidth() - vpX, b.getHeight());

    if (mTopScrollBar)
        mTopScrollBar->setBounds(vpX, topBar.getY(),
                                 b.getWidth() - vpX, kTopScrollH);

    mScrollContent->setSize(mScrollContent->getWidth(), mViewport->getHeight());
    layoutScrollContent();
    syncTopScrollBar();

    // 5F-4b B3: cable overlay covers full page, paints on top of all strips.
    if (mCableOverlay)
    {
        mCableOverlay->setBounds(getLocalBounds());
        mCableOverlay->toFront(false);
    }
    // Keep the permanent top scrollbar above the cable overlay so cables
    // drawn over the console never cover it.
    if (mTopScrollBar) mTopScrollBar->toFront(false);
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
