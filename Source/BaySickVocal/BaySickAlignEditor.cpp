#include "BaySickAlignEditor.h"
#include "BaySickVocalProcessor.h"
#include "../Standalone/BaySickTitleBar.h"
#include "../Standalone/SharedUI.h"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignEditor - QA-F Task 4 (2026-07-09).  See header for the layout
// summary; sections 13a-13g of phantom-recording-mongoose are the spec.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // ── Layout constants ────────────────────────────────────────────────────
    constexpr int kToolbarH        = 36;
    constexpr int kRulerH          = 18;
    constexpr int kHeaderW         = 110;    // lane header column
    constexpr int kSyncStripH      = 18;
    constexpr int kProtectedStripH = 18;
    constexpr int kViewModeH       = 22;
    constexpr int kHistoryH        = 56;
    constexpr int kRightPanelW     = 210;

    // ── Palette ─────────────────────────────────────────────────────────────
    const juce::Colour kBg        = juce::Colour (0xff0e0f12);
    const juce::Colour kPanelBg   = juce::Colour (0xff16191e);
    const juce::Colour kStripBg   = juce::Colour (0xff1a1d22);
    const juce::Colour kHeaderBg  = juce::Colour (0xff14171c);
    const juce::Colour kToolbarBg = juce::Colour (0xff0d0f12);
    const juce::Colour kText      = juce::Colour (0xffd0d6dc);
    const juce::Colour kTextDim   = juce::Colour (0xff8a929c);
    const juce::Colour kDirtyDot  = juce::Colour (0xff58c067);
    const juce::Colour kStaleAmber= juce::Colour (0xffe8a13c);

    // Lane colors per section 13b: Leader = Bass active green, Follower =
    // Vox active teal, Output = Drums active red (hexes from VC:: palette /
    // VoxPage::getPageColor).
    const juce::Colour kLeaderCol   = juce::Colour (0xff33ff88);
    const juce::Colour kFollowerCol = juce::Colour (0xff0fafa5);
    const juce::Colour kOutputCol   = juce::Colour (0xffff4444);

    // Factory presets (section 13a order).  Each sets Mode + Pitch ON/OFF
    // together; the +Pitch variants also seat Range at the Mode default
    // (section 13f: Loose 0% / Close 50% / Tight 100%).
    struct FactoryPreset { const char* name; int mode; bool pitchOn; float range; };
    constexpr FactoryPreset kFactoryPresets[6] = {
        { "Loose-Align",        0, false,   0.0f },
        { "Loose-Align+Pitch",  0, true,    0.0f },
        { "Close-Align",        1, false,  50.0f },
        { "Close-Align+Pitch",  1, true,   50.0f },
        { "Tight-Align",        2, false, 100.0f },
        { "Tight-Align+Pitch",  2, true,  100.0f },
    };

    // Mode-driven Range knob window (section 13e).
    inline void rangeWindowForMode (int mode, float& lo, float& hi, float& def)
    {
        if (mode == 0)      { lo = 0.0f;  hi = 50.0f;  def = 0.0f;   }
        else if (mode == 2) { lo = 50.0f; hi = 100.0f; def = 100.0f; }
        else                { lo = 25.0f; hi = 75.0f;  def = 50.0f;  }
    }

    inline float fineTuneBaseForMode (int mode)
    {
        return (mode == 0) ? 150.0f : (mode == 2) ? 50.0f : 100.0f;
    }

    juce::File alignPresetsDir()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("BaySickDAW").getChildFile ("Presets")
                 .getChildFile ("BaySickAlign").getChildFile ("My Presets");
    }

    // The bsa_ ids that participate in presets + the dirty dot.
    const char* const kPresetParamIds[] = {
        "bsa_align_on", "bsa_align_mode", "bsa_align_fineTune",
        "bsa_pitch_on", "bsa_pitch_range", "bsa_pitch_algo",
        "bsa_pitch_transpose", "bsa_formant_on", "bsa_formant_shift"
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Ruler - shared time ruler across all 3 lanes (section 13b)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::Ruler : public juce::Component
{
public:
    explicit Ruler (BaySickAlignEditor& o) : mOwner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kHeaderBg);
        auto area = getLocalBounds().withTrimmedLeft (kHeaderW);
        g.setColour (kTextDim);
        g.setFont (10.0f);

        const double pps = mOwner.pixelsPerSecond();
        // Tick spacing: nearest power-of-two-ish second step >= 40 px.
        double step = 1.0;
        while (step * pps < 40.0)  step *= 2.0;
        while (step * pps > 120.0) step /= 2.0;

        const double t0 = mOwner.scrollSeconds();
        double t = std::floor (t0 / step) * step;
        for (;; t += step)
        {
            const int x = area.getX() + (int) std::round ((t - t0) * pps);
            if (x > area.getRight()) break;
            if (x < area.getX()) continue;
            g.drawVerticalLine (x, 2.0f, (float) getHeight());
            const int mins = (int) (t / 60.0);
            const double secs = t - mins * 60.0;
            g.drawText (juce::String (mins) + ":"
                          + juce::String (secs, (step < 1.0) ? 1 : 0).paddedLeft ('0', (step < 1.0) ? 4 : 2),
                        x + 2, 0, 60, getHeight() - 2, juce::Justification::centredLeft);
        }
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

private:
    BaySickAlignEditor& mOwner;
};

// ─────────────────────────────────────────────────────────────────────────────
// LaneView - one composite lane (header column + waveform / pitch / energy)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::LaneView : public juce::Component
{
public:
    enum class Role { Leader, Follower, Output };

    LaneView (BaySickAlignEditor& o, Role r, LaneCache& cache)
        : mOwner (o), mRole (r), mCache (cache)
    {
        if (mRole != Role::Output)
        {
            addAndMakeVisible (mChannelCombo);
            mChannelCombo.setTextWhenNothingSelected (
                mRole == Role::Leader ? "Pick Leader..." : "(This Vox)");
            mChannelCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
            mChannelCombo.setColour (juce::ComboBox::textColourId, kText);
            mChannelCombo.onChange = [this] { pushChannelPick(); };
        }
    }

    juce::String roleName() const
    {
        return mRole == Role::Leader ? "LEADER"
             : mRole == Role::Follower ? "FOLLOWER" : "OUTPUT";
    }
    juce::Colour roleColour() const
    {
        return mRole == Role::Leader ? kLeaderCol
             : mRole == Role::Follower ? kFollowerCol : kOutputCol;
    }

    // Rebuild the channel combo from the processor's candidate list.  The
    // Follower adds a "(This Vox)" default entry (channel -1 sentinel).
    void setChannelItems (const std::vector<std::pair<int, juce::String>>& items,
                          int currentChannelId)
    {
        if (mRole == Role::Output) return;
        mRebuilding = true;
        mChannelCombo.clear (juce::dontSendNotification);
        if (mRole == Role::Follower)
            mChannelCombo.addItem ("(This Vox)", 1);        // id 1 -> channel -1
        for (const auto& it : items)
            mChannelCombo.addItem (it.second, it.first + 2); // id = channel + 2
        const int wantId = (currentChannelId < 0) ? 1 : currentChannelId + 2;
        if (mChannelCombo.indexOfItemId (wantId) >= 0)
            mChannelCombo.setSelectedId (wantId, juce::dontSendNotification);
        else
            mChannelCombo.setSelectedId (0, juce::dontSendNotification);
        mRebuilding = false;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);

        // Header column
        auto header = getLocalBounds().removeFromLeft (kHeaderW);
        g.setColour (kHeaderBg);
        g.fillRect (header);
        g.setColour (roleColour());
        g.fillRect (header.removeFromLeft (3));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (roleName(), kHeaderW - 100, 4, 94, 14, juce::Justification::centredLeft);

        auto area = getLocalBounds().withTrimmedLeft (kHeaderW);
        g.setColour (kPanelBg.brighter (0.03f));
        g.fillRect (area);

        if (! mCache.valid || mCache.audio.getNumSamples() <= 0)
        {
            g.setColour (kTextDim);
            g.setFont (12.0f);
            g.drawText (mRole == Role::Output
                            ? "Analyze to preview the aligned output"
                            : "No clips on this channel yet",
                        area, juce::Justification::centred);
            return;
        }

        const double pps = mOwner.pixelsPerSecond();
        const double t0  = mOwner.scrollSeconds();
        const int    n   = mCache.audio.getNumSamples();
        const float* src = mCache.audio.getReadPointer (0);
        const double sr  = mCache.sampleRate;
        const int midY   = area.getCentreY();
        const float halfH = (float) area.getHeight() * 0.46f;

        const auto mode = mOwner.mViewMode;
        if (mode == ViewMode::Wave)
        {
            g.setColour (roleColour().withAlpha (0.85f));
            for (int x = area.getX(); x < area.getRight(); ++x)
            {
                const juce::int64 s0 = (juce::int64) ((t0 + (x - area.getX())     / pps) * sr);
                const juce::int64 s1 = (juce::int64) ((t0 + (x - area.getX() + 1) / pps) * sr);
                if (s0 >= n) break;
                if (s1 <= 0) continue;
                const juce::int64 a = juce::jmax ((juce::int64) 0, s0);
                const juce::int64 z = juce::jmin ((juce::int64) n, juce::jmax (s0 + 1, s1));
                // Stride-sample wide zoom-outs: at most ~64 taps per pixel
                // keeps repaint O(width), not O(samples).
                const juce::int64 stride = juce::jmax ((juce::int64) 1, (z - a) / 64);
                float lo = 0.0f, hi = 0.0f;
                for (juce::int64 s = a; s < z; s += stride)
                {
                    lo = juce::jmin (lo, src[s]);
                    hi = juce::jmax (hi, src[s]);
                }
                g.drawVerticalLine (x, (float) midY - hi * halfH,
                                       (float) midY - lo * halfH + 1.0f);
            }
        }
        else if (mode == ViewMode::Pitch)
        {
            ensurePitchTrack();
            g.setColour (roleColour());
            juce::Path p;
            bool started = false;
            const double hopSec = 2048.0 / sr;
            for (size_t f = 0; f < mCache.f0Hz.size(); ++f)
            {
                const float hz = mCache.f0Hz[f];
                const double t = (double) f * hopSec;
                const int x = area.getX() + (int) std::round ((t - t0) * pps);
                if (x < area.getX() - 4) continue;
                if (x > area.getRight() + 4) break;
                if (hz <= 0.0f) { started = false; continue; }
                // 60..1200 Hz mapped log onto the lane height.
                const float norm = (std::log2 (hz / 60.0f)) / std::log2 (1200.0f / 60.0f);
                const float y = (float) area.getBottom()
                              - juce::jlimit (0.0f, 1.0f, norm) * (float) area.getHeight();
                if (! started) { p.startNewSubPath ((float) x, y); started = true; }
                else             p.lineTo ((float) x, y);
            }
            g.strokePath (p, juce::PathStrokeType (1.6f));
        }
        else // Energy
        {
            ensureRmsTrack();
            g.setColour (roleColour().withAlpha (0.7f));
            const double hopSec = 512.0 / sr;
            for (int x = area.getX(); x < area.getRight(); ++x)
            {
                const double t = t0 + (x - area.getX()) / pps;
                const int f = (int) (t / hopSec);
                if (f < 0) continue;
                if (f >= (int) mCache.rms.size()) break;
                const float v = juce::jlimit (0.0f, 1.0f, mCache.rms[(size_t) f] * 2.5f);
                g.drawVerticalLine (x, (float) area.getBottom() - v * (float) area.getHeight(),
                                       (float) area.getBottom());
            }
        }
    }

    void resized() override
    {
        if (mRole != Role::Output)
            mChannelCombo.setBounds (4, getHeight() - 26, kHeaderW - 10, 22);
    }

    // Ctrl+wheel zoom / wheel scroll, shared across lanes via the owner.
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCtrlDown())
        {
            const double factor = (wheel.deltaY > 0) ? 1.2 : 1.0 / 1.2;
            mOwner.setView (mOwner.pixelsPerSecond() * factor, mOwner.scrollSeconds());
        }
        else
        {
            const double d = (std::abs (wheel.deltaX) > std::abs (wheel.deltaY)
                                  ? wheel.deltaX : wheel.deltaY);
            mOwner.setView (mOwner.pixelsPerSecond(),
                            mOwner.scrollSeconds() - d * 60.0 / mOwner.pixelsPerSecond());
        }
    }

private:
    void ensurePitchTrack()
    {
        if (! mCache.f0Hz.empty() || ! mCache.valid) return;
        BaySickAlignDSP::estimateF0Track (mCache.audio.getReadPointer (0),
                                          mCache.audio.getNumSamples(),
                                          mCache.sampleRate, 2048, mCache.f0Hz);
    }
    void ensureRmsTrack()
    {
        if (! mCache.rms.empty() || ! mCache.valid) return;
        const int n = mCache.audio.getNumSamples();
        const float* s = mCache.audio.getReadPointer (0);
        mCache.rms.reserve ((size_t) (n / 512 + 1));
        for (int i = 0; i < n; i += 512)
        {
            const int len = juce::jmin (512, n - i);
            double acc = 0.0;
            for (int k = 0; k < len; ++k) acc += (double) s[i + k] * s[i + k];
            mCache.rms.push_back ((float) std::sqrt (acc / juce::jmax (1, len)));
        }
    }

    void pushChannelPick()
    {
        if (mRebuilding) return;
        const int id = mChannelCombo.getSelectedId();
        const int channel = (id <= 1) ? -1 : id - 2;
        mOwner.setParamValue (mRole == Role::Leader ? "bsa_leader_channel"
                                                    : "bsa_follower_channel",
                              (float) channel);
        mOwner.refreshComposites();
    }

    BaySickAlignEditor& mOwner;
    Role                mRole;
    LaneCache&          mCache;
    juce::ComboBox      mChannelCombo;
    bool                mRebuilding { false };
};

// ─────────────────────────────────────────────────────────────────────────────
// SyncStrip - user-placed hard pairing anchors (section 13c)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::SyncStrip : public juce::Component
{
public:
    explicit SyncStrip (BaySickAlignEditor& o) : mOwner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kStripBg);
        g.setColour (kTextDim.withAlpha (0.4f));
        g.setFont (9.0f);
        g.drawText ("SYNC POINTS", 4, 0, kHeaderW - 8, getHeight(),
                    juce::Justification::centredLeft);

        const auto& pts = mOwner.mProc.mAlignState.syncPoints;
        for (const auto& p : pts)
        {
            const int xF = xForSec (p.followerSec);
            const int xL = xForSec (p.leaderSec);
            g.setColour (kFollowerCol);
            g.fillRect (xF - 1, getHeight() / 2, 3, getHeight() / 2);
            g.setColour (kLeaderCol);
            g.fillRect (xL - 1, 0, 3, getHeight() / 2);
            g.setColour (kTextDim);
            g.drawLine ((float) xL, (float) getHeight() / 2.0f - 1.0f,
                        (float) xF, (float) getHeight() / 2.0f + 1.0f, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < kHeaderW) return;
        auto& pts = mOwner.mProc.mAlignState.syncPoints;

        const int hit = hitTest (e.x);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            if (hit >= 0) m.addItem (1, "Delete Sync Point");
            m.addItem (2, "Automatic Sync Points");
            juce::Component::SafePointer<SyncStrip> self (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                [self, hit] (int r)
                {
                    if (! self) return;
                    auto& owner = self->mOwner;
                    if (r == 1 && hit >= 0
                        && hit < (int) owner.mProc.mAlignState.syncPoints.size())
                    {
                        owner.pushUndo();
                        owner.mProc.mAlignState.syncPoints.erase (
                            owner.mProc.mAlignState.syncPoints.begin() + hit);
                        owner.mEditsSinceAnalyze = true;
                        self->repaint();
                    }
                    else if (r == 2)
                        self->seedAutomatic();
                });
            return;
        }

        if (hit >= 0)
        {
            mDragIdx    = hit;
            mDragLeader = (e.y < getHeight() / 2);
        }
        else
        {
            mOwner.pushUndo();
            AlignSyncPoint p;
            p.followerSec = p.leaderSec = secForX (e.x);
            pts.push_back (p);
            mDragIdx    = (int) pts.size() - 1;
            mDragLeader = false;
            mOwner.mEditsSinceAnalyze = true;
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& pts = mOwner.mProc.mAlignState.syncPoints;
        if (mDragIdx < 0 || mDragIdx >= (int) pts.size()) return;
        const double t = juce::jmax (0.0, secForX (e.x));
        if (mDragLeader) pts[(size_t) mDragIdx].leaderSec   = t;
        else             pts[(size_t) mDragIdx].followerSec = t;
        mOwner.mEditsSinceAnalyze = true;
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { mDragIdx = -1; }

private:
    int xForSec (double sec) const
    {
        return kHeaderW + (int) std::round ((sec - mOwner.scrollSeconds())
                                            * mOwner.pixelsPerSecond());
    }
    double secForX (int x) const
    {
        return mOwner.scrollSeconds() + (double) (x - kHeaderW) / mOwner.pixelsPerSecond();
    }
    int hitTest (int x) const
    {
        const auto& pts = mOwner.mProc.mAlignState.syncPoints;
        for (int i = 0; i < (int) pts.size(); ++i)
            if (std::abs (xForSec (pts[(size_t) i].followerSec) - x) < 5
                || std::abs (xForSec (pts[(size_t) i].leaderSec) - x) < 5)
                return i;
        return -1;
    }

    // "Automatic Sync Points": seed high-confidence onset pairs as a baseline
    // the user can adjust (section 13c).  Uses the current map's anchors --
    // requires an analysis to exist.
    void seedAutomatic()
    {
        const auto& map = mOwner.mProc.mAlignState.map;
        if (! map.isValid())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                "Automatic Sync Points", "Analyze first - automatic sync points "
                "seed from the analysis pairing.");
            return;
        }
        mOwner.pushUndo();
        auto& pts = mOwner.mProc.mAlignState.syncPoints;
        pts.clear();
        // Every 4th interior anchor keeps the seed sparse enough to drag.
        for (size_t i = 1; i + 1 < map.anchors.size(); i += 4)
        {
            AlignSyncPoint p;
            p.followerSec = map.anchors[i].dubTimeSec;
            p.leaderSec   = map.anchors[i].guideTimeSec;
            pts.push_back (p);
        }
        mOwner.mEditsSinceAnalyze = true;
        repaint();
    }

    BaySickAlignEditor& mOwner;
    int  mDragIdx    { -1 };
    bool mDragLeader { false };
};

// ─────────────────────────────────────────────────────────────────────────────
// ProtectedStrip - drag-create exempt regions (section 13c)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::ProtectedStrip : public juce::Component
{
public:
    explicit ProtectedStrip (BaySickAlignEditor& o) : mOwner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kStripBg);
        g.setColour (kTextDim.withAlpha (0.4f));
        g.setFont (9.0f);
        g.drawText ("PROTECTED", 4, 0, kHeaderW - 8, getHeight(),
                    juce::Justification::centredLeft);

        for (const auto& a : mOwner.mProc.mAlignState.protectedAreas)
        {
            const int x0 = xForSec (a.startSec);
            const int x1 = xForSec (a.endSec);
            g.setColour (kOutputCol.withAlpha (0.30f));
            g.fillRect (x0, 1, juce::jmax (2, x1 - x0), getHeight() - 2);
            g.setColour (kOutputCol.withAlpha (0.8f));
            g.drawRect (x0, 1, juce::jmax (2, x1 - x0), getHeight() - 2);
            juce::String tag;
            if (a.protectTime)  tag << "T";
            if (a.protectPitch) tag << "P";
            g.setFont (9.0f);
            g.drawText (tag, x0 + 2, 0, 24, getHeight(), juce::Justification::centredLeft);
        }
        if (mDragging)
        {
            const int x0 = xForSec (juce::jmin (mDragStartSec, mDragEndSec));
            const int x1 = xForSec (juce::jmax (mDragStartSec, mDragEndSec));
            g.setColour (kOutputCol.withAlpha (0.18f));
            g.fillRect (x0, 1, juce::jmax (2, x1 - x0), getHeight() - 2);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < kHeaderW) return;
        auto& areas = mOwner.mProc.mAlignState.protectedAreas;
        const int hit = hitTest (e.x);

        if (e.mods.isPopupMenu())
        {
            if (hit < 0) return;
            auto& a = areas[(size_t) hit];
            juce::PopupMenu m;
            m.addItem (1, "Protect Timing",  true, a.protectTime);
            m.addItem (2, "Protect Pitch",   true, a.protectPitch);
            m.addSeparator();
            m.addItem (3, "Delete Protected Area");
            juce::Component::SafePointer<ProtectedStrip> self (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                [self, hit] (int r)
                {
                    if (! self || r <= 0) return;
                    auto& owner = self->mOwner;
                    auto& list = owner.mProc.mAlignState.protectedAreas;
                    if (hit >= (int) list.size()) return;
                    owner.pushUndo();
                    if (r == 1) list[(size_t) hit].protectTime  = ! list[(size_t) hit].protectTime;
                    if (r == 2) list[(size_t) hit].protectPitch = ! list[(size_t) hit].protectPitch;
                    if (r == 3) list.erase (list.begin() + hit);
                    owner.mEditsSinceAnalyze = true;
                    self->repaint();
                });
            return;
        }

        mDragging = true;
        mDragStartSec = mDragEndSec = secForX (e.x);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! mDragging) return;
        mDragEndSec = juce::jmax (0.0, secForX (e.x));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! mDragging) return;
        mDragging = false;
        const double s = juce::jmin (mDragStartSec, mDragEndSec);
        const double e = juce::jmax (mDragStartSec, mDragEndSec);
        if (e - s > 0.02)   // ignore accidental clicks
        {
            mOwner.pushUndo();
            AlignProtectedArea a;
            a.startSec = juce::jmax (0.0, s);
            a.endSec   = e;
            mOwner.mProc.mAlignState.protectedAreas.push_back (a);
            mOwner.mEditsSinceAnalyze = true;
        }
        repaint();
    }

private:
    int xForSec (double sec) const
    {
        return kHeaderW + (int) std::round ((sec - mOwner.scrollSeconds())
                                            * mOwner.pixelsPerSecond());
    }
    double secForX (int x) const
    {
        return mOwner.scrollSeconds() + (double) (x - kHeaderW) / mOwner.pixelsPerSecond();
    }
    int hitTest (int x) const
    {
        const auto& areas = mOwner.mProc.mAlignState.protectedAreas;
        for (int i = 0; i < (int) areas.size(); ++i)
            if (x >= xForSec (areas[(size_t) i].startSec) - 2
                && x <= xForSec (areas[(size_t) i].endSec) + 2)
                return i;
        return -1;
    }

    BaySickAlignEditor& mOwner;
    bool   mDragging { false };
    double mDragStartSec { 0.0 }, mDragEndSec { 0.0 };
};

// ─────────────────────────────────────────────────────────────────────────────
// ViewModeBar - Wave / Pitch / Energy (section 13d)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::ViewModeBar : public juce::Component
{
public:
    explicit ViewModeBar (BaySickAlignEditor& o) : mOwner (o)
    {
        auto setup = [this] (juce::TextButton& b, const juce::String& t, int idx)
        {
            b.setButtonText (t);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (0x414C4E56);
            b.setColour (juce::TextButton::buttonColourId,   kPanelBg);
            b.setColour (juce::TextButton::buttonOnColourId, kPanelBg.brighter (0.25f));
            b.setColour (juce::TextButton::textColourOnId,   kText);
            b.setColour (juce::TextButton::textColourOffId,  kTextDim);
            b.onClick = [this, idx]
            {
                mOwner.mViewMode = (ViewMode) idx;
                mOwner.repaint();
            };
            addAndMakeVisible (b);
        };
        setup (mWave,   "Wave",   0);
        setup (mPitch,  "Pitch",  1);
        setup (mEnergy, "Energy", 2);
        mWave.setToggleState (true, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override { g.fillAll (kToolbarBg); }

    void resized() override
    {
        auto b = getLocalBounds().reduced (2);
        b.removeFromLeft (kHeaderW - 2);
        mWave  .setBounds (b.removeFromLeft (64));
        b.removeFromLeft (2);
        mPitch .setBounds (b.removeFromLeft (64));
        b.removeFromLeft (2);
        mEnergy.setBounds (b.removeFromLeft (64));
    }

private:
    BaySickAlignEditor& mOwner;
    juce::TextButton mWave, mPitch, mEnergy;
};

// ─────────────────────────────────────────────────────────────────────────────
// HistoryScrubber - render-version list + Del / + / - (section 13d)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::HistoryScrubber : public juce::Component
{
public:
    explicit HistoryScrubber (BaySickAlignEditor& o) : mOwner (o)
    {
        auto plain = [this] (juce::TextButton& b, const juce::String& t,
                             const juce::String& tt)
        {
            b.setButtonText (t);
            b.setTooltip (tt);
            b.setColour (juce::TextButton::buttonColourId, kPanelBg);
            b.setColour (juce::TextButton::textColourOnId,  kText);
            b.setColour (juce::TextButton::textColourOffId, kText);
            addAndMakeVisible (b);
        };
        plain (mDelBtn,  "Del", "Remove the selected render from the history list (the file stays on disk)");
        plain (mZoomIn,  "+",   "Zoom in");
        plain (mZoomOut, "-",   "Zoom out");
        mDelBtn.onClick = [this]
        {
            auto& renders = mOwner.mProc.mAlignState.renders;
            if (mSelected >= 0 && mSelected < (int) renders.size())
            {
                renders.erase (renders.begin() + mSelected);
                mSelected = -1;
                repaint();
            }
        };
        mZoomIn .onClick = [this] { mOwner.setView (mOwner.pixelsPerSecond() * 1.25, mOwner.scrollSeconds()); };
        mZoomOut.onClick = [this] { mOwner.setView (mOwner.pixelsPerSecond() / 1.25, mOwner.scrollSeconds()); };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kStripBg);
        g.setColour (kTextDim.withAlpha (0.5f));
        g.setFont (9.0f);
        g.drawText ("RENDERS", 4, 2, kHeaderW - 8, 12, juce::Justification::centredLeft);

        const auto& renders = mOwner.mProc.mAlignState.renders;
        auto area = getLocalBounds().withTrimmedLeft (kHeaderW)
                                    .withTrimmedRight (88).reduced (2);
        if (renders.empty())
        {
            g.drawText ("No renders yet - Render bakes Aligned/{name}_align_v{N}.wav",
                        area, juce::Justification::centredLeft);
            return;
        }
        const int cellW = 132;
        for (int i = 0; i < (int) renders.size(); ++i)
        {
            auto cell = juce::Rectangle<int> (area.getX() + i * (cellW + 4),
                                              area.getY(), cellW, area.getHeight());
            if (cell.getX() > area.getRight()) break;
            g.setColour (i == mSelected ? kPanelBg.brighter (0.3f) : kPanelBg);
            g.fillRoundedRectangle (cell.toFloat(), 3.0f);
            g.setColour (i == mSelected ? kText : kTextDim);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText ("v" + juce::String (renders[(size_t) i].version),
                        cell.removeFromTop (16).reduced (6, 1),
                        juce::Justification::centredLeft);
            g.setFont (9.0f);
            // ISO date -> readable "YYYY-MM-DD HH:MM"
            juce::String d = renders[(size_t) i].dateIso;
            d = d.replaceCharacter ('T', ' ');
            if (d.length() > 16) d = d.substring (0, 16);
            g.drawText (d, cell.reduced (6, 1), juce::Justification::topLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds().withTrimmedLeft (kHeaderW)
                                    .withTrimmedRight (88).reduced (2);
        if (! area.contains (e.getPosition())) return;
        const int idx = (e.x - area.getX()) / 136;
        const auto& renders = mOwner.mProc.mAlignState.renders;
        mSelected = (idx >= 0 && idx < (int) renders.size()) ? idx : -1;
        repaint();
    }

    void resized() override
    {
        auto right = getLocalBounds().removeFromRight (86).reduced (2);
        const int w = 26;
        mDelBtn .setBounds (right.removeFromLeft (w));
        right.removeFromLeft (2);
        mZoomIn .setBounds (right.removeFromLeft (w));
        right.removeFromLeft (2);
        mZoomOut.setBounds (right.removeFromLeft (w));
    }

private:
    BaySickAlignEditor& mOwner;
    juce::TextButton mDelBtn, mZoomIn, mZoomOut;
    int mSelected { -1 };
};

// ─────────────────────────────────────────────────────────────────────────────
// RightPanel - Align box + Pitch box, always visible (section 13e)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::RightPanel : public juce::Component,
                                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit RightPanel (BaySickAlignEditor& o, juce::AudioProcessorValueTreeState& apvts)
        : mOwner (o), mApvts (apvts)
    {
        // ── Align box ────────────────────────────────────────────────────────
        addAndMakeVisible (mAlignOn);
        mAlignOn.setButtonText ("ON");
        mAlignOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            mApvts, "bsa_align_on", mAlignOn);

        addAndMakeVisible (mModeCombo);
        mModeCombo.addItem ("Loose", 1);
        mModeCombo.addItem ("Close", 2);
        mModeCombo.addItem ("Tight", 3);
        mModeCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mModeCombo.setColour (juce::ComboBox::textColourId, kText);
        mModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            mApvts, "bsa_align_mode", mModeCombo);

        setupRotary (mFineTune, "bsa_align_fineTune", mFineTuneAtt);
        mFineTune.textFromValueFunction = [this] (double v)
        {
            const int mode = (int) mOwner.paramValue ("bsa_align_mode", 1.0f);
            return juce::String (fineTuneBaseForMode (mode) + v, 0) + " ms";
        };
        mFineTune.updateText();
        mFineTune.setTooltip ("Max onset difference used for pairing.  12 o'clock = "
                              "the Mode base (Loose 150 ms / Close 100 ms / Tight 50 ms); "
                              "turn left for tighter, right for looser.");

        // ── Pitch box ────────────────────────────────────────────────────────
        addAndMakeVisible (mPitchOn);
        mPitchOn.setButtonText ("ON");
        mPitchOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            mApvts, "bsa_pitch_on", mPitchOn);

        // Range: manual binding -- the Mode dropdown re-windows its travel
        // (section 13e), which a stock SliderAttachment can't express.
        addAndMakeVisible (mRange);
        mRange.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mRange.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        mRange.setTextValueSuffix (" %");
        mRange.setTooltip ("How much of the Leader's pitch contour is applied to the "
                           "Follower.  The Mode dropdown sets this knob's window: "
                           "Loose 0-50, Close 25-75, Tight 50-100.");
        mRange.onValueChange = [this]
        {
            if (mRangeRewindowing) return;
            mOwner.setParamValue ("bsa_pitch_range", (float) mRange.getValue());
        };
        applyModeWindowToRange ((int) mOwner.paramValue ("bsa_align_mode", 1.0f));

        addAndMakeVisible (mAlgoCombo);
        mAlgoCombo.addItem ("PSOLA",         1);
        mAlgoCombo.addItem ("Granular",      2);
        mAlgoCombo.addItem ("Phase Vocoder", 3);
        mAlgoCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mAlgoCombo.setColour (juce::ComboBox::textColourId, kText);
        mAlgoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            mApvts, "bsa_pitch_algo", mAlgoCombo);

        setupRotary (mTranspose, "bsa_pitch_transpose", mTransposeAtt);
        mTranspose.setTextValueSuffix (" st");
        mTranspose.setTooltip ("Flat semitone shift applied on top of the pitch matching.");

        addAndMakeVisible (mFormantOn);
        mFormantOn.setButtonText ("Formant");
        mFormantOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            mApvts, "bsa_formant_on", mFormantOn);

        setupRotary (mFormantShift, "bsa_formant_shift", mFormantShiftAtt);
        mFormantShift.setTextValueSuffix (" st");
        mFormantShift.setTooltip ("Shifts the vocal character (spectral envelope) without "
                                  "re-pitching - up for thinner, down for fuller.");

        mApvts.addParameterListener ("bsa_align_mode", this);
    }

    ~RightPanel() override
    {
        mApvts.removeParameterListener ("bsa_align_mode", this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg);
        auto b = getLocalBounds().reduced (6);

        auto drawBox = [&g] (juce::Rectangle<int> r, const juce::String& title)
        {
            g.setColour (kBg);
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            g.setColour (kTextDim);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (title, r.removeFromTop (18).reduced (8, 2),
                        juce::Justification::centredLeft);
        };
        drawBox (b.removeFromTop (mAlignBoxH), "ALIGN");
        b.removeFromTop (6);
        drawBox (b, "PITCH");

        // Knob captions
        g.setColour (kTextDim);
        g.setFont (10.0f);
        auto caption = [&g] (juce::Component& c, const juce::String& t)
        {
            g.drawText (t, c.getX(), c.getY() - 12, c.getWidth(), 12,
                        juce::Justification::centred);
        };
        caption (mFineTune,     "Fine Tune");
        caption (mRange,        "Range");
        caption (mTranspose,    "Transpose");
        caption (mFormantShift, "Formant Shift");
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6);

        auto align = b.removeFromTop (mAlignBoxH);
        align.removeFromTop (18);
        auto row1 = align.removeFromTop (24).reduced (8, 0);
        mAlignOn.setBounds (row1.removeFromLeft (52));
        row1.removeFromLeft (6);
        mModeCombo.setBounds (row1);
        align.removeFromTop (14);
        mFineTune.setBounds (align.removeFromTop (78).withSizeKeepingCentre (74, 76));

        b.removeFromTop (6);
        auto pitch = b;
        pitch.removeFromTop (18);
        auto prow = pitch.removeFromTop (24).reduced (8, 0);
        mPitchOn.setBounds (prow.removeFromLeft (52));
        prow.removeFromLeft (6);
        mAlgoCombo.setBounds (prow);
        pitch.removeFromTop (14);
        auto knobRow = pitch.removeFromTop (78);
        mRange    .setBounds (knobRow.removeFromLeft (knobRow.getWidth() / 2)
                                     .withSizeKeepingCentre (74, 76));
        mTranspose.setBounds (knobRow.withSizeKeepingCentre (74, 76));
        pitch.removeFromTop (16);
        auto frow = pitch.removeFromTop (24).reduced (8, 0);
        mFormantOn.setBounds (frow.removeFromLeft (80));
        pitch.removeFromTop (14);
        mFormantShift.setBounds (pitch.removeFromTop (78).withSizeKeepingCentre (74, 76));
    }

private:
    void setupRotary (juce::Slider& s, const char* paramId,
                      std::unique_ptr<TaggedSliderAttachment>& att)
    {
        addAndMakeVisible (s);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        att = std::make_unique<TaggedSliderAttachment> (mApvts, paramId, s);
    }

    // Mode drives the Range knob's window + default (section 13e).
    void applyModeWindowToRange (int mode)
    {
        float lo, hi, def;
        rangeWindowForMode (mode, lo, hi, def);
        mRangeRewindowing = true;
        mRange.setRange (lo, hi, 0.1);
        const float cur = mOwner.paramValue ("bsa_pitch_range", def);
        mRange.setValue (juce::jlimit (lo, hi, cur), juce::dontSendNotification);
        mRange.setDoubleClickReturnValue (true, def);
        mRangeRewindowing = false;
    }

    void parameterChanged (const juce::String& id, float newValue) override
    {
        if (id != "bsa_align_mode") return;
        // APVTS listeners can fire off the message thread; UI work bounces.
        const int mode = (int) newValue;
        juce::Component::SafePointer<RightPanel> self (this);
        juce::MessageManager::callAsync ([self, mode]
        {
            if (! self) return;
            float lo, hi, def;
            rangeWindowForMode (mode, lo, hi, def);
            self->applyModeWindowToRange (mode);
            // Mode change re-seats Range at the mode default (section 13e:
            // "sets both the default position and the min/max travel").
            self->mOwner.setParamValue ("bsa_pitch_range", def);
            self->mRange.setValue (def, juce::dontSendNotification);
            self->mFineTune.updateText();
        });
    }

    BaySickAlignEditor& mOwner;
    juce::AudioProcessorValueTreeState& mApvts;

    int mAlignBoxH { 160 };

    juce::ToggleButton mAlignOn, mPitchOn, mFormantOn;
    juce::ComboBox     mModeCombo, mAlgoCombo;
    juce::Slider       mFineTune, mRange, mTranspose, mFormantShift;
    bool               mRangeRewindowing { false };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        mAlignOnAtt, mPitchOnAtt, mFormantOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        mModeAtt, mAlgoAtt;
    std::unique_ptr<TaggedSliderAttachment>
        mFineTuneAtt, mTransposeAtt, mFormantShiftAtt;
};

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar (section 13a)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::Toolbar : public juce::Component
{
public:
    explicit Toolbar (BaySickAlignEditor& o) : mOwner (o)
    {
        addAndMakeVisible (mTitleLbl);

        addAndMakeVisible (mPresetCombo);
        for (int i = 0; i < 6; ++i)
            mPresetCombo.addItem (kFactoryPresets[i].name, i + 1);
        mPresetCombo.addItem ("(User)", 7);
        mPresetCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mPresetCombo.setColour (juce::ComboBox::textColourId, kText);
        mPresetCombo.onChange = [this]
        {
            if (mMirroring) return;
            const int id = mPresetCombo.getSelectedId();
            if (id >= 1 && id <= 6)
                mOwner.applyFactoryPreset (id - 1);
        };

        auto plain = [this] (juce::TextButton& b, const juce::String& t,
                             const juce::String& tt)
        {
            b.setButtonText (t);
            b.setTooltip (tt);
            b.setColour (juce::TextButton::buttonColourId, kPanelBg);
            b.setColour (juce::TextButton::textColourOnId,  kText);
            b.setColour (juce::TextButton::textColourOffId, kText);
            addAndMakeVisible (b);
        };
        plain (mSaveBtn,    "Save",    "Save the current Align + Pitch settings as a user preset");
        plain (mLoadBtn,    "Load",    "Load a saved user preset");
        plain (mAnalyzeBtn, "Analyze/Apply",
               "Pair the Follower's onsets to the Leader, build the warp, and apply it to playback");
        plain (mVersionsBtn, "Versions",
               "Revert to an earlier applied state (every Analyze/Apply is a restore point)");
        plain (mRenderBtn,  "Render",
               "Export the aligned Follower to Aligned/{name}_align_v{N}.wav (file only - playback is already live)");
        plain (mUndoBtn,    "Undo",    "Undo the last sync-point / protected-area / analysis change");
        plain (mRedoBtn,    "Redo",    "Redo");

        mSaveBtn    .onClick = [this] { mOwner.saveUserPreset(); };
        mLoadBtn    .onClick = [this] { mOwner.loadUserPreset(); };
        mAnalyzeBtn .onClick = [this] { mOwner.runAnalyze(); };
        mVersionsBtn.onClick = [this] { mOwner.showVersionsMenu(); };
        mRenderBtn  .onClick = [this] { mOwner.runRender(); };
        mUndoBtn    .onClick = [this] { mOwner.doUndo(); };
        mRedoBtn    .onClick = [this] { mOwner.doRedo(); };
    }

    void setDirty (bool d)          { if (mDirty != d) { mDirty = d; repaint(); } }
    void setStale (bool s, bool pending)
    {
        if (mStale != s || mStalePending != pending)
            { mStale = s; mStalePending = pending; repaint(); }
    }
    void mirrorPreset (int presetParam)   // 0..5 factory, 6 = user
    {
        mMirroring = true;
        mPresetCombo.setSelectedId (juce::jlimit (0, 6, presetParam) + 1,
                                    juce::dontSendNotification);
        mMirroring = false;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kToolbarBg);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        // Preset-dirty green dot (section 13a: lights when current values
        // diverge from the selected preset).
        if (mDirty)
        {
            g.setColour (kDirtyDot);
            g.fillEllipse ((float) mPresetCombo.getRight() + 5.0f,
                           (float) getHeight() / 2.0f - 4.0f, 8.0f, 8.0f);
        }

        if (mStale)
        {
            // Pending = the grid changed while the transport runs; the
            // stop-gated auto re-analyze fires at the next stop (item 4 of
            // the recovery bundle).
            g.setColour (kStaleAmber);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (mStalePending ? "RE-ANALYZE ON STOP" : "RE-ANALYZE",
                        mAnalyzeBtn.getX() - 154, 0, 150, getHeight(),
                        juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 4);
        mTitleLbl.setBounds (b.removeFromLeft (110));
        b.removeFromLeft (8);
        mPresetCombo.setBounds (b.removeFromLeft (170).reduced (0, 2));
        b.removeFromLeft (18);   // dirty-dot slot
        mSaveBtn.setBounds (b.removeFromLeft (46));
        b.removeFromLeft (4);
        mLoadBtn.setBounds (b.removeFromLeft (46));

        auto right = b;
        const int gap = 4;
        mRedoBtn  .setBounds (right.removeFromRight (48));
        right.removeFromRight (gap);
        mUndoBtn  .setBounds (right.removeFromRight (48));
        right.removeFromRight (12);
        mRenderBtn.setBounds (right.removeFromRight (62));
        right.removeFromRight (gap);
        mVersionsBtn.setBounds (right.removeFromRight (66));
        right.removeFromRight (gap);
        mAnalyzeBtn.setBounds (right.removeFromRight (94));
    }

private:
    BaySickAlignEditor& mOwner;
    BaySickEngineLabel  mTitleLbl { "BaySickAlign", juce::Colour (0xFF0FAFA5) };
    juce::ComboBox      mPresetCombo;
    juce::TextButton    mSaveBtn, mLoadBtn, mAnalyzeBtn, mVersionsBtn, mRenderBtn,
                        mUndoBtn, mRedoBtn;
    bool mDirty { false }, mStale { false }, mStalePending { false },
         mMirroring { false };
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignEditor
// ─────────────────────────────────────────────────────────────────────────────
BaySickAlignEditor::BaySickAlignEditor (BaySickVocalProcessor& p)
    : mProc (p)
{
    mToolbar        = std::make_unique<Toolbar> (*this);
    mRuler          = std::make_unique<Ruler> (*this);
    mLeaderLane     = std::make_unique<LaneView> (*this, LaneView::Role::Leader,   mLeaderCache);
    mSyncStrip      = std::make_unique<SyncStrip> (*this);
    mFollowerLane   = std::make_unique<LaneView> (*this, LaneView::Role::Follower, mFollowerCache);
    mProtectedStrip = std::make_unique<ProtectedStrip> (*this);
    mOutputLane     = std::make_unique<LaneView> (*this, LaneView::Role::Output,   mOutputCache);
    mViewModeBar    = std::make_unique<ViewModeBar> (*this);
    mHistory        = std::make_unique<HistoryScrubber> (*this);
    mRightPanel     = std::make_unique<RightPanel> (*this, mProc.apvts);

    addAndMakeVisible (*mToolbar);
    addAndMakeVisible (*mRuler);
    addAndMakeVisible (*mLeaderLane);
    addAndMakeVisible (*mSyncStrip);
    addAndMakeVisible (*mFollowerLane);
    addAndMakeVisible (*mProtectedStrip);
    addAndMakeVisible (*mOutputLane);
    addAndMakeVisible (*mViewModeBar);
    addAndMakeVisible (*mHistory);
    addAndMakeVisible (*mRightPanel);

    snapshotPresetValues();
    startTimer (500);
}

BaySickAlignEditor::~BaySickAlignEditor() = default;

void BaySickAlignEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
}

void BaySickAlignEditor::resized()
{
    auto b = getLocalBounds();
    mToolbar->setBounds (b.removeFromTop (kToolbarH));
    mRightPanel->setBounds (b.removeFromRight (kRightPanelW));
    mHistory->setBounds (b.removeFromBottom (kHistoryH));
    mViewModeBar->setBounds (b.removeFromBottom (kViewModeH));
    mRuler->setBounds (b.removeFromTop (kRulerH));

    const int laneH = (b.getHeight() - kSyncStripH - kProtectedStripH) / 3;
    mLeaderLane    ->setBounds (b.removeFromTop (laneH));
    mSyncStrip     ->setBounds (b.removeFromTop (kSyncStripH));
    mFollowerLane  ->setBounds (b.removeFromTop (laneH));
    mProtectedStrip->setBounds (b.removeFromTop (kProtectedStripH));
    mOutputLane    ->setBounds (b);
}

void BaySickAlignEditor::setView (double pps, double scrollSec)
{
    mPps    = juce::jlimit (4.0, 4000.0, pps);
    mScroll = juce::jmax (0.0, scrollSec);
    repaint();
}

void BaySickAlignEditor::timerCallback()
{
    const bool stale = mProc.isAlignStale() || mEditsSinceAnalyze;
    mToolbar->setStale (stale, stale && DSPBase::isTransportPlaying());

    const bool dirty = paramsDivergeFromSnapshot();
    mToolbar->setDirty (dirty);
    if (auto* p = mProc.apvts.getRawParameterValue ("bsa_preset_dirty"))
        if ((p->load() > 0.5f) != dirty)
            setParamValue ("bsa_preset_dirty", dirty ? 1.0f : 0.0f);
    mToolbar->mirrorPreset ((int) paramValue ("bsa_preset", 2.0f));

    if (++mSlowTick >= 4)   // ~2 s: channel lists move rarely
    {
        mSlowTick = 0;
        rebuildChannelCombos();
    }
}

// ─── Actions ──────────────────────────────────────────────────────────────────
void BaySickAlignEditor::runAnalyze()
{
    pushUndo();
    juce::String err;
    if (! mProc.analyzeAlign (err))
    {
        mUndoStack.pop_back();   // nothing changed
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Analyze", err);
        return;
    }
    mEditsSinceAnalyze = false;
    refreshComposites();
    refreshOutputPreview();
    repaint();
}

void BaySickAlignEditor::runRender()
{
    juce::String err;
    const auto file = mProc.renderAlignedTake (err);
    if (file == juce::File())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Render", err);
        return;
    }
    refreshOutputPreview();
    repaint();
}

// QA-Fa recovery (bundle item 3): dropdown of applied states, newest first.
// Entries whose analyze-time grid signature differs from the current grid
// carry a "(grid changed)" marker; reverting to one is allowed and lights
// the stale badge immediately.
void BaySickAlignEditor::showVersionsMenu()
{
    const auto& versions = mProc.mAlignVersions;
    if (versions.empty())
    {
        juce::PopupMenu m;
        m.addItem (1, "No versions yet - Analyze/Apply creates one", false);
        m.showMenuAsync (juce::PopupMenu::Options());
        return;
    }

    juce::int64 curL = 0, curF = 0;
    if (mProc.onChannelClipSignature)
    {
        const int l = mProc.resolveLeaderChannel();
        const int f = mProc.resolveFollowerChannel();
        if (l >= 0) curL = mProc.onChannelClipSignature (l);
        if (f >= 0) curF = mProc.onChannelClipSignature (f);
    }

    juce::PopupMenu m;
    for (int i = (int) versions.size() - 1; i >= 0; --i)
    {
        const auto& v = versions[(size_t) i];
        juce::String label = "v" + juce::String (i + 1) + "  "
            + v.dateIso.substring (0, 16).replace ("T", " ");
        if (v.sigA != curL || v.sigB != curF)
            label += "  (grid changed)";
        m.addItem (i + 1, "Revert to " + label);
    }
    juce::Component::SafePointer<BaySickAlignEditor> self (this);
    m.showMenuAsync (juce::PopupMenu::Options(),
        [self] (int result)
        {
            if (! self || result <= 0) return;
            self->pushUndo();
            if (! self->mProc.revertAlignToVersion (result - 1))
                { self->mUndoStack.pop_back(); return; }
            self->mEditsSinceAnalyze = false;
            self->refreshComposites();
            self->refreshOutputPreview();
            self->repaint();
        });
}

void BaySickAlignEditor::refreshComposites()
{
    juce::int64 lStart = 0, fStart = 0;
    auto load = [this] (int channelId, LaneCache& cache, juce::int64& startOut)
    {
        cache.clear();
        startOut = 0;
        if (channelId < 0 || ! mProc.onRenderComposite) return;
        double beat = 0.0, sr = 44100.0;
        auto buf = mProc.onRenderComposite (channelId, beat, startOut, sr);
        if (buf.getNumSamples() <= 0) return;
        cache.audio       = std::move (buf);
        cache.sampleRate  = sr;
        cache.durationSec = cache.audio.getNumSamples() / sr;
        cache.valid       = true;
    };
    load (mProc.resolveLeaderChannel(),   mLeaderCache,   lStart);
    load (mProc.resolveFollowerChannel(), mFollowerCache, fStart);

    // Common-origin shift for painting: mirror the analysis-side padding so
    // lane x positions line up across Leader / Follower / Output.
    if (mLeaderCache.valid && mFollowerCache.valid)
    {
        const juce::int64 common = juce::jmin (lStart, fStart);
        auto pad = [] (LaneCache& c, juce::int64 padN)
        {
            if (padN <= 0) return;
            juce::AudioBuffer<float> padded (1, c.audio.getNumSamples() + (int) padN);
            padded.clear();
            padded.copyFrom (0, (int) padN, c.audio, 0, 0, c.audio.getNumSamples());
            c.audio = std::move (padded);
            c.durationSec = c.audio.getNumSamples() / c.sampleRate;
            c.f0Hz.clear();
            c.rms.clear();
        };
        pad (mLeaderCache,   lStart - common);
        pad (mFollowerCache, fStart - common);
    }
    repaint();
}

void BaySickAlignEditor::refreshOutputPreview()
{
    mOutputCache.clear();
    auto out = mProc.renderAlignedPreview();
    if (out.getNumSamples() > 0)
    {
        mOutputCache.audio       = std::move (out);
        mOutputCache.sampleRate  = mProc.mAlignState.analysisSampleRate;
        mOutputCache.durationSec = mOutputCache.audio.getNumSamples()
                                   / mOutputCache.sampleRate;
        mOutputCache.valid       = true;
    }
    repaint();
}

void BaySickAlignEditor::rebuildChannelCombos()
{
    if (! mProc.onListCandidateChannels) return;
    auto items = mProc.onListCandidateChannels();
    // The Follower's own channel shouldn't be offered as its own Leader.
    auto leaderItems = items;
    const int follower = mProc.resolveFollowerChannel();
    leaderItems.erase (std::remove_if (leaderItems.begin(), leaderItems.end(),
                        [follower] (const std::pair<int, juce::String>& it)
                        { return it.first == follower; }),
                       leaderItems.end());

    int leaderParam = -1, followerParam = -1;
    if (auto* p = mProc.apvts.getRawParameterValue ("bsa_leader_channel"))
        leaderParam = (int) p->load();
    if (auto* p = mProc.apvts.getRawParameterValue ("bsa_follower_channel"))
        followerParam = (int) p->load();

    mLeaderLane  ->setChannelItems (leaderItems, leaderParam);
    mFollowerLane->setChannelItems (items,       followerParam);
}

// ─── Presets (section 13a) ────────────────────────────────────────────────────
void BaySickAlignEditor::applyFactoryPreset (int presetIdx)
{
    presetIdx = juce::jlimit (0, 5, presetIdx);
    const auto& fp = kFactoryPresets[presetIdx];
    setParamValue ("bsa_preset",         (float) presetIdx);
    setParamValue ("bsa_align_mode",     (float) fp.mode);
    setParamValue ("bsa_pitch_on",       fp.pitchOn ? 1.0f : 0.0f);
    setParamValue ("bsa_pitch_range",    fp.range);
    setParamValue ("bsa_align_fineTune", 0.0f);
    snapshotPresetValues();
    repaint();
}

void BaySickAlignEditor::snapshotPresetValues()
{
    mPresetSnapshot.clear();
    for (auto* id : kPresetParamIds)
        mPresetSnapshot[id] = paramValue (id);
}

bool BaySickAlignEditor::paramsDivergeFromSnapshot() const
{
    for (auto* id : kPresetParamIds)
    {
        const auto it = mPresetSnapshot.find (id);
        if (it == mPresetSnapshot.end()) return true;
        if (std::abs (paramValue (id) - it->second) > 0.001f) return true;
    }
    return false;
}

void BaySickAlignEditor::saveUserPreset()
{
    auto* aw = new juce::AlertWindow ("Save Align Preset",
                                      "Enter a name for this Align preset:",
                                      juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Align");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<BaySickAlignEditor> self (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [self, aw] (int r)
        {
            if (r != 1 || ! self) return;
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            juce::XmlElement el ("BaySickAlignPreset");
            for (auto* id : kPresetParamIds)
                el.setAttribute (id, (double) self->paramValue (id));

            auto dir = alignPresetsDir();
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");
            target.replaceWithText (el.toString());

            self->setParamValue ("bsa_preset", 6.0f);   // "(User)"
            self->snapshotPresetValues();
        }), true);
}

void BaySickAlignEditor::loadUserPreset()
{
    juce::PopupMenu m;
    juce::Array<juce::File> files;
    auto dir = alignPresetsDir();
    if (dir.isDirectory())
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
            files.add (f);
    if (files.isEmpty())
        m.addItem (-1, "(no presets saved)", false, false);
    else
        for (int i = 0; i < files.size(); ++i)
            m.addItem (i + 1, files[i].getFileNameWithoutExtension());

    juce::Component::SafePointer<BaySickAlignEditor> self (this);
    m.showMenuAsync (juce::PopupMenu::Options(),
        [self, files] (int r)
        {
            if (! self || r <= 0 || r > files.size()) return;
            auto xml = juce::parseXML (files[r - 1]);
            if (xml == nullptr || ! xml->hasTagName ("BaySickAlignPreset")) return;
            for (auto* id : kPresetParamIds)
                if (xml->hasAttribute (id))
                    self->setParamValue (id, (float) xml->getDoubleAttribute (id));
            self->setParamValue ("bsa_preset", 6.0f);   // "(User)"
            self->snapshotPresetValues();
            self->repaint();
        });
}

// ─── Local align-edit undo ────────────────────────────────────────────────────
BaySickAlignEditor::EditSnapshot BaySickAlignEditor::captureEdits() const
{
    EditSnapshot s;
    s.map            = mProc.mAlignState.map;
    s.syncPoints     = mProc.mAlignState.syncPoints;
    s.protectedAreas = mProc.mAlignState.protectedAreas;
    s.analyzed       = mProc.mAlignState.analyzed;
    return s;
}

void BaySickAlignEditor::restoreEdits (const EditSnapshot& s)
{
    mProc.mAlignState.map            = s.map;
    mProc.mAlignState.syncPoints     = s.syncPoints;
    mProc.mAlignState.protectedAreas = s.protectedAreas;
    mProc.mAlignState.analyzed       = s.analyzed;
    if (s.analyzed && s.map.isValid()) mProc.mAlign.setWarpMap (s.map);
    else                               mProc.mAlign.clearWarpMap();
    refreshOutputPreview();
    repaint();
}

void BaySickAlignEditor::pushUndo()
{
    mUndoStack.push_back (captureEdits());
    if (mUndoStack.size() > 50)
        mUndoStack.erase (mUndoStack.begin());
    mRedoStack.clear();
}

void BaySickAlignEditor::doUndo()
{
    if (mUndoStack.empty()) return;
    mRedoStack.push_back (captureEdits());
    restoreEdits (mUndoStack.back());
    mUndoStack.pop_back();
}

void BaySickAlignEditor::doRedo()
{
    if (mRedoStack.empty()) return;
    mUndoStack.push_back (captureEdits());
    restoreEdits (mRedoStack.back());
    mRedoStack.pop_back();
}

// ─── Param helpers ────────────────────────────────────────────────────────────
float BaySickAlignEditor::paramValue (const char* id, float fallback) const
{
    if (auto* p = mProc.apvts.getRawParameterValue (id))
        return p->load();
    return fallback;
}

void BaySickAlignEditor::setParamValue (const char* id, float v)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
            mProc.apvts.getParameter (id)))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v));
}
