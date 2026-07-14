#include "EventEditor.h"
#include <cmath>

using namespace juce;

// ─────────────────────────────────────────────────────────────────────────────
// Shared colours
// ─────────────────────────────────────────────────────────────────────────────
static const Colour kTeal         { 0xff40e0d0 };
static const Colour kGridBgEE     { 0xff1a1c1e };
static const Colour kGridZebraEE  { 0xff1e2022 };
static const Colour kGridLineEE   { 0xff2a2c2e };
static const Colour kGridLineMjEE { 0xff3a3c3e };
static const Colour kDeepRed      { 0xff7b241c };
static const Colour kSelBlue      { 0xff1a3a6a };
static const Colour kTextGray     { 0xffa0a4aa };

// ─────────────────────────────────────────────────────────────────────────────
// EEAutomationGrid
// ─────────────────────────────────────────────────────────────────────────────

EEAutomationGrid::EEAutomationGrid(UndoManager& um)
    : mUM(um)
{
    setOpaque(true);
}

void EEAutomationGrid::setBlock(PatternManager* pm, int blockIdx, float clipLengthBeats)
{
    mPM        = pm;
    mBlockIdx  = blockIdx;
    mTotalBeats = jmax(0.25f, clipLengthBeats);
    repaint();
}

void EEAutomationGrid::setLaneReadOnly(const AutomationLane* /*lane*/, float clipLengthBeats)
{
    // read-only display: caller updates mPM to nullptr and provides lane externally
    // For simplicity we just set mPM=nullptr and use setBlock instead
    mPM        = nullptr;
    mBlockIdx  = -1;
    mTotalBeats = jmax(0.25f, clipLengthBeats);
    repaint();
}

void EEAutomationGrid::setTotalBeats(float beats)
{
    mTotalBeats = jmax(0.25f, beats);
    repaint();
}

void EEAutomationGrid::setTool(EETool t)
{
    mTool = t;
    updateCursor();
    repaint();
}

void EEAutomationGrid::setLFOMode(bool lfo)
{
    mLFOMode = lfo;
    repaint();
}

void EEAutomationGrid::setPixelsPerBeat(float ppb)
{
    mPPBeat = jmax(4.f, ppb);
    resized();
    repaint();
}

void EEAutomationGrid::selectAll()
{
    mSelection.clear();
    if (auto* lane = lanePtr())
        for (int i = 0; i < (int)lane->points.size(); ++i)
            mSelection.push_back(i);
    repaint();
}

void EEAutomationGrid::deleteSelected()
{
    auto* lane = lanePtr();
    if (!lane || mSelection.empty()) return;

    AutomationLane before = *lane;
    std::sort(mSelection.rbegin(), mSelection.rend());
    for (int i : mSelection)
        if (i >= 0 && i < (int)lane->points.size())
            lane->points.erase(lane->points.begin() + i);
    AutomationLane after = *lane;
    mSelection.clear();
    commitEdit("Delete Points", before, after);
    if (onChanged) onChanged();
}

// ── Coordinate helpers ─────────────────────────────────────────────────────

float EEAutomationGrid::beatToX(float beat) const
{
    const float contentW = (float)(getWidth() - kValLabelW);
    return (float)kValLabelW + beat / mTotalBeats * contentW;
}

float EEAutomationGrid::xToBeat(float px) const
{
    const float contentW = (float)(getWidth() - kValLabelW);
    if (contentW <= 0.f) return 0.f;
    return (px - (float)kValLabelW) / contentW * mTotalBeats;
}

float EEAutomationGrid::valToY(float v01) const
{
    const float contentH = (float)(getHeight() - kRulerH);
    return (float)kRulerH + (1.f - jlimit(0.f, 1.f, v01)) * contentH;
}

float EEAutomationGrid::yToVal(float py) const
{
    const float contentH = (float)(getHeight() - kRulerH);
    if (contentH <= 0.f) return 0.5f;
    return 1.f - jlimit(0.f, 1.f, (py - (float)kRulerH) / contentH);
}

float EEAutomationGrid::snapBeat(float beat, bool noSnap) const
{
    if (noSnap) return beat;
    float gridSize = 1.f / (float)mSnapSub;
    return std::round(beat / gridSize) * gridSize;
}

int EEAutomationGrid::hitTestPt(float px, float py) const
{
    const auto* lane = lanePtr();
    if (!lane) return -1;
    float bestDist = kNodeR * 2.5f;
    int   bestIdx  = -1;
    for (int i = 0; i < (int)lane->points.size(); ++i)
    {
        float nx = beatToX(lane->points[i].timeTicks * mTotalBeats);
        float ny = valToY(lane->points[i].value01);
        float d  = std::sqrt((nx - px) * (nx - px) + (ny - py) * (ny - py));
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }
    return bestIdx;
}

float EEAutomationGrid::rangeStartVal() const
{
    const auto* lane = lanePtr();
    if (!lane || lane->points.empty()) return 0.5f;
    float minT = lane->points[0].timeTicks;
    float val  = lane->points[0].value01;
    for (const auto& pt : lane->points)
        if (pt.timeTicks < minT) { minT = pt.timeTicks; val = pt.value01; }
    return val;
}

AutomationLane* EEAutomationGrid::lanePtr() const
{
    if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return nullptr;
    return &mPM->getBlock(mBlockIdx).automationLane;
}

void EEAutomationGrid::commitEdit(const String& label,
                                   const AutomationLane& before,
                                   const AutomationLane& after)
{
    if (!mPM || mBlockIdx < 0) return;
    PatternManager* pm       = mPM;
    int             blockIdx = mBlockIdx;

    mUM.beginNewTransaction(label);
    mUM.perform(new AutomationLaneEditAction(
        label, before, after,
        [pm, blockIdx](const AutomationLane& lane) {
            if (blockIdx < pm->getNumBlocks())
                pm->getBlock(blockIdx).automationLane = lane;
        }));
}

// ── resized ────────────────────────────────────────────────────────────────

void EEAutomationGrid::resized() {}

// ── paint ──────────────────────────────────────────────────────────────────

void EEAutomationGrid::paint(Graphics& g)
{
    g.fillAll(kGridBgEE);
    drawValueLabels(g);
    drawRuler(g);
    drawGrid(g);
    if (mLFOMode)
        drawLFOWaveform(g);
    else
    {
        drawCurve(g);
        drawCurveHandles(g);   // diamonds between adjacent non-Stepped points
    }
    drawNodes(g);
    if (mMarqueeActive) drawMarquee(g);
}

void EEAutomationGrid::drawValueLabels(Graphics& g) const
{
    // Left margin: value labels at 0.0, 0.25, 0.50, 0.75, 1.00
    g.setColour(Colour(0xff141618));
    g.fillRect(0, kRulerH, kValLabelW, getHeight() - kRulerH);
    g.setColour(kTextGray.withAlpha(0.7f));
    g.setFont(Font(9.f));
    const float vals[] = { 1.f, 0.75f, 0.5f, 0.25f, 0.f };
    for (float v : vals)
    {
        float y = valToY(v);
        String lbl = String(v, 2);
        g.drawText(lbl, 0, (int)(y - 7.f), kValLabelW - 3, 14, Justification::centredRight, false);
    }
    // Separator line
    g.setColour(kGridLineMjEE);
    g.drawVerticalLine(kValLabelW - 1, (float)kRulerH, (float)getHeight());
}

void EEAutomationGrid::drawRuler(Graphics& g) const
{
    // Ruler background
    g.setColour(Colour(0xff141618));
    g.fillRect(0, 0, getWidth(), kRulerH);
    g.setColour(kGridLineMjEE);
    g.drawHorizontalLine(kRulerH - 1, 0.f, (float)getWidth());

    // Bar numbers and tick marks
    g.setFont(Font(9.f));
    float barsTotal = mTotalBeats / (float)mBeatsPerBar;
    for (int bar = 0; bar <= (int)std::ceil(barsTotal); ++bar)
    {
        float beat = (float)bar * (float)mBeatsPerBar;
        float x    = beatToX(beat);
        if (x < kValLabelW - 1 || x > getWidth() + 1) continue;
        // Major bar tick
        g.setColour(kGridLineMjEE);
        g.drawVerticalLine((int)x, (float)(kRulerH - 7), (float)kRulerH);
        // Bar label
        g.setColour(kTextGray.withAlpha(0.85f));
        g.drawText(String(bar + 1), (int)x + 2, 0, 30, kRulerH - 3, Justification::centredLeft, false);
        // Beat subdivisions
        for (int beat2 = 1; beat2 < mBeatsPerBar; ++beat2)
        {
            float x2 = beatToX((float)bar * mBeatsPerBar + beat2);
            if (x2 < kValLabelW || x2 > getWidth()) continue;
            g.setColour(kGridLineEE);
            g.drawVerticalLine((int)x2, (float)(kRulerH - 4), (float)kRulerH);
        }
    }
}

void EEAutomationGrid::drawGrid(Graphics& g) const
{
    const int contentW = getWidth() - kValLabelW;
    const int contentH = getHeight() - kRulerH;
    const float barsTotal = mTotalBeats / (float)mBeatsPerBar;

    // ── Vertical zebra stripes (per bar) ────────────────────────────────────
    for (int bar = 0; bar < (int)std::ceil(barsTotal); ++bar)
    {
        if (bar % 2 != 0) continue;
        float x1 = beatToX((float)bar * mBeatsPerBar);
        float x2 = beatToX((float)(bar + 1) * mBeatsPerBar);
        x1 = jmax(x1, (float)kValLabelW);
        x2 = jmin(x2, (float)getWidth());
        g.setColour(kGridZebraEE);
        g.fillRect(x1, (float)kRulerH, x2 - x1, (float)contentH);
    }

    // ── Horizontal value guides (0.25, 0.5, 0.75) ───────────────────────────
    for (float v : { 0.25f, 0.5f, 0.75f })
    {
        float y = valToY(v);
        if (v == 0.5f)
        {
            g.setColour(kGridLineMjEE);
            g.drawHorizontalLine((int)y, (float)kValLabelW, (float)getWidth());
        }
        else
        {
            g.setColour(kGridLineEE);
            g.drawHorizontalLine((int)y, (float)kValLabelW, (float)getWidth());
        }
    }
    // Outer horizontal lines (value 0 and 1)
    g.setColour(kGridLineMjEE);
    g.drawHorizontalLine(kRulerH, (float)kValLabelW, (float)getWidth());
    g.drawHorizontalLine(getHeight() - 1, (float)kValLabelW, (float)getWidth());

    // ── Vertical beat + sub-beat lines ──────────────────────────────────────
    float totalBeatsFull = mTotalBeats;
    int   totalBeatCount = (int)std::ceil(totalBeatsFull);
    for (int b = 0; b <= totalBeatCount; ++b)
    {
        float x = beatToX((float)b);
        if (x < kValLabelW || x > getWidth()) continue;
        bool isMajor = (b % mBeatsPerBar == 0);
        g.setColour(isMajor ? kGridLineMjEE : kGridLineEE);
        g.drawVerticalLine((int)x, (float)kRulerH, (float)getHeight());

        // Sub-beat lines (mSnapSub > 1)
        if (mSnapSub > 1 && b < totalBeatCount)
        {
            for (int sub = 1; sub < mSnapSub; ++sub)
            {
                float xs = beatToX((float)b + (float)sub / (float)mSnapSub);
                if (xs < kValLabelW || xs > getWidth()) continue;
                g.setColour(kGridLineEE.withAlpha(0.5f));
                g.drawVerticalLine((int)xs, (float)kRulerH, (float)getHeight());
            }
        }
    }

    // Right edge line
    g.setColour(kGridLineMjEE);
    g.drawVerticalLine(getWidth() - 1, (float)kRulerH, (float)getHeight());
}

void EEAutomationGrid::buildCurvePath(Path& path) const
{
    const auto* lane = lanePtr();
    if (!lane || lane->points.empty()) return;

    // Sort points by time
    std::vector<ControlPoint> pts = lane->points;
    std::sort(pts.begin(), pts.end(),
        [](const ControlPoint& a, const ControlPoint& b) { return a.timeTicks < b.timeTicks; });

    // Start the path from left edge at first point's value
    float firstBeat = pts[0].timeTicks * mTotalBeats;
    float firstY    = valToY(pts[0].value01);
    path.startNewSubPath(beatToX(firstBeat), firstY);

    for (int i = 1; i < (int)pts.size(); ++i)
    {
        float x1 = beatToX(pts[i - 1].timeTicks * mTotalBeats);
        float y1 = valToY(pts[i - 1].value01);
        float x2 = beatToX(pts[i].timeTicks * mTotalBeats);
        float y2 = valToY(pts[i].value01);

        const CurveType ctype = pts[i - 1].curveType;
        if (ctype == CurveType::Stepped)
        {
            path.lineTo(x2, y1);   // horizontal
            path.lineTo(x2, y2);   // then vertical drop
        }
        else if (ctype == CurveType::Spline && (int)pts.size() >= 2)
        {
            // Catmull-Rom segment: use neighbouring points as tangent guides
            const ControlPoint& p0 = (i >= 2)        ? pts[i - 2] : pts[i - 1];
            const ControlPoint& p1 = pts[i - 1];
            const ControlPoint& p2 = pts[i];
            const ControlPoint& p3 = (i + 1 < (int)pts.size()) ? pts[i + 1] : pts[i];

            float x0r = beatToX(p0.timeTicks * mTotalBeats), y0r = valToY(p0.value01);
            float x3r = beatToX(p3.timeTicks * mTotalBeats), y3r = valToY(p3.value01);

            const int kSeg = 20;
            for (int s = 1; s <= kSeg; ++s)
            {
                float t  = (float)s / (float)kSeg;
                float t2 = t * t, t3 = t2 * t;
                float cx = 0.5f * ((2.f * x1) + (-x0r + x2) * t
                                  + (2.f * x0r - 5.f * x1 + 4.f * x2 - x3r) * t2
                                  + (-x0r + 3.f * x1 - 3.f * x2 + x3r) * t3);
                float cy = 0.5f * ((2.f * y1) + (-y0r + y2) * t
                                  + (2.f * y0r - 5.f * y1 + 4.f * y2 - y3r) * t2
                                  + (-y0r + 3.f * y1 - 3.f * y2 + y3r) * t3);
                path.lineTo(cx, cy);
            }
        }
        else
        {
            // FL Studio Scaled Exponential Transfer Function: y = (t_factor^x - 1)/(t_factor - 1)
            // Per-pixel sampling (every 2px) keeps curve sharp at any segment width.
            float T = juce::jlimit(-0.999f, 0.999f, pts[i - 1].tension);

            auto evalFL = [](float x, float Tv) -> float {
                if (std::abs(Tv) < 0.001f) return x;
                float denom    = 0.5f * Tv + 0.5f;
                float t_factor = 1.0f - Tv / (denom * denom);
                if (std::abs(t_factor - 1.0f) < 0.0001f) return x;
                return juce::jlimit(0.0f, 1.0f,
                    (std::pow(std::abs(t_factor), x) - 1.0f) / (t_factor - 1.0f));
            };

            float segW = x2 - x1;
            if (segW > 0.5f)
            {
                constexpr float kStep = 2.0f;  // one vertex per 2 pixels
                for (float px = kStep; px < segW; px += kStep)
                {
                    float xn = px / segW;
                    float yn = evalFL(xn, T);
                    path.lineTo(x1 + px, y1 + (y2 - y1) * yn);
                }
            }
            path.lineTo(x2, y2);
        }
    }
}

void EEAutomationGrid::drawCurve(Graphics& g) const
{
    const auto* lane = lanePtr();
    if (!lane) return;

    if (lane->points.empty())
    {
        // Dashed guide at midpoint
        float midY = valToY(0.5f);
        g.setColour(kTeal.withAlpha(0.25f));
        float dx = (float)kValLabelW;
        while (dx < getWidth())
        {
            g.drawLine(dx, midY, jmin(dx + 5.f, (float)getWidth()), midY, 1.f);
            dx += 10.f;
        }
        return;
    }

    Path curvePath;
    buildCurvePath(curvePath);

    // Extend into fill path
    Path fillPath = curvePath;
    // Close to bottom
    fillPath.lineTo(beatToX(mTotalBeats), (float)getHeight());
    fillPath.lineTo((float)kValLabelW, (float)getHeight());
    fillPath.closeSubPath();

    // Fill
    g.setColour(kTeal.withAlpha(0.12f));
    g.fillPath(fillPath);

    // Glow (wide, low alpha)
    g.setColour(kTeal.withAlpha(0.18f));
    g.strokePath(curvePath, PathStrokeType(5.f, PathStrokeType::curved, PathStrokeType::rounded));

    // Main stroke
    g.setColour(kTeal.withAlpha(0.9f));
    g.strokePath(curvePath, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}

void EEAutomationGrid::drawNodes(Graphics& g) const
{
    const auto* lane = lanePtr();
    if (!lane) return;

    for (int i = 0; i < (int)lane->points.size(); ++i)
    {
        float x = beatToX(lane->points[i].timeTicks * mTotalBeats);
        float y = valToY(lane->points[i].value01);
        bool  hov = (i == mHoverPt);
        bool  sel = std::find(mSelection.begin(), mSelection.end(), i) != mSelection.end();
        bool  drag = (i == mDragPt && mDragging);

        float r = kNodeR + (hov || drag ? 1.5f : 0.f);

        // Outer ring
        g.setColour(kTeal.withAlpha(hov || drag ? 1.f : 0.8f));
        g.drawEllipse(x - r, y - r, r * 2.f, r * 2.f, 1.5f);

        if (sel)
        {
            // Filled for selected
            g.setColour(kTeal.withAlpha(0.5f));
            g.fillEllipse(x - r + 2.f, y - r + 2.f, (r - 2.f) * 2.f, (r - 2.f) * 2.f);
        }
        else
        {
            // Hollow - filled with dark bg
            g.setColour(kGridBgEE);
            g.fillEllipse(x - r + 1.5f, y - r + 1.5f, (r - 1.5f) * 2.f, (r - 1.5f) * 2.f);
        }
    }
}

// ── Curve handle helpers ──────────────────────────────────────────────────────
// Returns a sorted (by timeTicks) copy of lane->points paired with original indices.
static std::vector<std::pair<ControlPoint,int>> sortedWithIdx(const AutomationLane* lane)
{
    std::vector<std::pair<ControlPoint,int>> sv;
    if (!lane) return sv;
    sv.reserve(lane->points.size());
    for (int i = 0; i < (int)lane->points.size(); ++i)
        sv.push_back({ lane->points[i], i });
    std::sort(sv.begin(), sv.end(),
        [](const auto& a, const auto& b){ return a.first.timeTicks < b.first.timeTicks; });
    return sv;
}

int EEAutomationGrid::hitTestCurveHandle(float px, float py) const
{
    const auto* lane = lanePtr();
    if (!lane || lane->points.size() < 2) return -1;

    auto sv = sortedWithIdx(lane);
    for (int i = 0; i < (int)sv.size() - 1; ++i)
    {
        const ControlPoint& p0 = sv[i].first;
        const ControlPoint& p1 = sv[i + 1].first;
        if (p0.curveType == CurveType::Stepped) continue;
        if (std::abs(p0.value01 - p1.value01) < 0.02f) continue; // flat - no handle

        {
            float span = p1.timeTicks - p0.timeTicks;
            float T    = juce::jlimit(-0.999f, 0.999f, p0.tension);

            auto evalFL = [](float x, float Tv) -> float {
                if (std::abs(Tv) < 0.001f) return x;
                float denom    = 0.5f * Tv + 0.5f;
                float t_factor = 1.0f - Tv / (denom * denom);
                if (std::abs(t_factor - 1.0f) < 0.0001f) return x;
                return juce::jlimit(0.0f, 1.0f,
                    (std::pow(std::abs(t_factor), x) - 1.0f) / (t_factor - 1.0f));
            };

            // Diamond at x=0.5 (horizontal midpoint of segment)
            float mx   = beatToX((p0.timeTicks + span * 0.5f) * mTotalBeats);
            float midV = p0.value01 + evalFL(0.5f, T) * (p1.value01 - p0.value01);
            float my   = valToY(midV);

            if (std::abs(px - mx) < 7.f && std::abs(py - my) < 7.f)
                return sv[i].second;
        }
    }
    return -1;
}

void EEAutomationGrid::drawCurveHandles(Graphics& g) const
{
    const auto* lane = lanePtr();
    if (!lane || lane->points.size() < 2) return;

    auto sv = sortedWithIdx(lane);
    const juce::Colour kHandleCol = juce::Colour(0xff18c8a0); // slightly cyan-shifted from node teal

    for (int i = 0; i < (int)sv.size() - 1; ++i)
    {
        const ControlPoint& p0 = sv[i].first;
        const ControlPoint& p1 = sv[i + 1].first;
        int origIdx = sv[i].second;

        if (p0.curveType == CurveType::Stepped) continue;
        if (std::abs(p0.value01 - p1.value01) < 0.02f) continue;

        {
            float span = p1.timeTicks - p0.timeTicks;
            float T    = juce::jlimit(-0.999f, 0.999f, p0.tension);

            auto evalFL = [](float x, float Tv) -> float {
                if (std::abs(Tv) < 0.001f) return x;
                float denom    = 0.5f * Tv + 0.5f;
                float t_factor = 1.0f - Tv / (denom * denom);
                if (std::abs(t_factor - 1.0f) < 0.0001f) return x;
                return juce::jlimit(0.0f, 1.0f,
                    (std::pow(std::abs(t_factor), x) - 1.0f) / (t_factor - 1.0f));
            };

            // Diamond at x=0.5 (horizontal midpoint of segment)
            float mx   = beatToX((p0.timeTicks + span * 0.5f) * mTotalBeats);
            float midV = p0.value01 + evalFL(0.5f, T) * (p1.value01 - p0.value01);
            float my   = valToY(midV);

            bool active = (origIdx == mCurveHandleDrag || origIdx == mHoverCurveHandle);
            float r = active ? 5.f : 4.f;

            // Dashed connector line from mid-segment straight line to actual handle
            float lineMidY = (valToY(p0.value01) + valToY(p1.value01)) * 0.5f;
            if (std::abs(my - lineMidY) > 2.f)
            {
                g.setColour(kHandleCol.withAlpha(0.3f));
                g.drawLine(mx, lineMidY, mx, my, 1.f);
            }

            // Diamond shape
            juce::Path diamond;
            diamond.startNewSubPath(mx,     my - r);
            diamond.lineTo         (mx + r, my    );
            diamond.lineTo         (mx,     my + r);
            diamond.lineTo         (mx - r, my    );
            diamond.closeSubPath();

            g.setColour(kHandleCol.withAlpha(active ? 0.25f : 0.12f));
            g.fillPath(diamond);
            g.setColour(kHandleCol.withAlpha(active ? 1.f : 0.6f));
            g.strokePath(diamond, juce::PathStrokeType(1.2f));
        }
    }
}

void EEAutomationGrid::drawLFOWaveform(Graphics& g) const
{
    const auto* lane = lanePtr();
    if (!lane) return;

    const float lfoMin  = lane->lfoMin;
    const float lfoMax  = lane->lfoMax;
    const float range   = lfoMax - lfoMin;
    const float rate    = jmax(0.001f, lane->lfoRate);  // cycles per bar
    const int   shape   = lane->lfoShape;
    const int   steps   = jmax(4, getWidth() * 2);
    const float twoPi   = MathConstants<float>::twoPi;

    Path p;
    for (int i = 0; i <= steps; ++i)
    {
        float norm  = (float)i / (float)steps;
        float beat  = norm * mTotalBeats;
        float bar   = beat / (float)mBeatsPerBar;
        float phase = bar * rate * twoPi;
        float t01   = std::fmod(phase / twoPi, 1.f);
        if (t01 < 0.f) t01 += 1.f;

        float val;
        switch (shape)
        {
            case 0: val = 0.5f + 0.5f * std::sin(phase);                 break; // sine
            case 1: val = (t01 < 0.5f) ? 2.f * t01 : 2.f * (1.f - t01); break; // triangle
            case 2: val = t01;                                             break; // saw up
            case 3: val = (std::sin(phase) >= 0.f) ? 1.f : 0.f;          break; // square
            default: val = 0.5f; break;
        }
        val = jlimit(0.f, 1.f, lfoMin + val * range);

        float px = beatToX(beat);
        float py = valToY(val);
        if (i == 0) p.startNewSubPath(px, py);
        else        p.lineTo(px, py);
    }

    // Glow
    g.setColour(kTeal.withAlpha(0.2f));
    g.strokePath(p, PathStrokeType(5.f));
    // Main line
    g.setColour(kTeal.withAlpha(0.75f));
    g.strokePath(p, PathStrokeType(1.5f));
}

void EEAutomationGrid::drawMarquee(Graphics& g) const
{
    if (!mMarqueeActive) return;
    auto r = Rectangle<int>(mMarqueeStart, mMarqueeCurrent).toFloat();
    g.setColour(kTeal.withAlpha(0.15f));
    g.fillRect(r);
    g.setColour(kTeal.withAlpha(0.7f));
    g.drawRect(r, 1.f);
}

// ── Mouse ──────────────────────────────────────────────────────────────────

void EEAutomationGrid::mouseMove(const MouseEvent& e)
{
    int prev   = mHoverPt;
    int prevCH = mHoverCurveHandle;
    mHoverPt = hitTestPt((float)e.x, (float)e.y);
    // Only hover curve handles when not hovering a node
    mHoverCurveHandle = (mHoverPt < 0) ? hitTestCurveHandle((float)e.x, (float)e.y) : -1;
    if (mHoverPt != prev || mHoverCurveHandle != prevCH) repaint();
    updateCursor();

    // 5F-5: fire hover-changed callback for the status bar
    if (onHoverChanged && e.y >= kRulerH)
    {
        float beat = juce::jlimit(0.f, mTotalBeats, xToBeat((float)e.x));
        float val  = juce::jlimit(0.f, 1.f, yToVal((float)e.y));
        onHoverChanged(beat, val);
    }
}

void EEAutomationGrid::promptSetPointValue(int hit)
{
    auto* l = lanePtr();
    if (!l || hit < 0 || hit >= (int)l->points.size()) return;

    const juce::String paramId = l->paramId;
    const float        cur01   = l->points[hit].value01;
    const juce::String preset  = (onFormatValue && paramId.isNotEmpty())
                                     ? onFormatValue (paramId, cur01)
                                     : juce::String (cur01, 3);

    auto* aw = new juce::AlertWindow ("Set Value",
                                       "Enter a value for this point:",
                                       juce::AlertWindow::NoIcon);
    aw->addTextEditor ("value", preset);
    aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<EEAutomationGrid> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, hit, paramId] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1 || ! safeThis) return;
            auto* g  = safeThis.getComponent();
            auto* l2 = g->lanePtr();
            if (!l2 || hit < 0 || hit >= (int)l2->points.size()) return;
            const juce::String txt = aw->getTextEditorContents ("value").trim();
            float new01 = (g->onParseValue && paramId.isNotEmpty())
                             ? g->onParseValue (paramId, txt)
                             : txt.getFloatValue();
            new01 = juce::jlimit (0.0f, 1.0f, new01);
            AutomationLane before = *l2;
            l2->points[hit].value01 = new01;
            g->commitEdit ("Set Point Value", before, *l2);
            if (g->onChanged) g->onChanged();
            g->repaint();
        }), false);
}

void EEAutomationGrid::mouseDown(const MouseEvent& e)
{
    auto* lane = lanePtr();
    if (!lane) return;

    if (e.y < kRulerH) return;

    const float px   = (float)e.x;
    const float py   = (float)e.y;
    bool noSnap      = e.mods.isAltDown();
    float beat       = jlimit(0.f, mTotalBeats, snapBeat(xToBeat(px), noSnap));
    float val        = jlimit(0.f, 1.f, yToVal(py));

    // ── Right-click on control point → Reset / Delete menu ────────────────────
    if (e.mods.isRightButtonDown())
    {
        int hit = hitTestPt(px, py);
        if (hit >= 0)
        {
            juce::PopupMenu m;
            m.addItem(3, "Set Value...");
            m.addSeparator();
            m.addItem(1, "Reset to midpoint");
            m.addItem(2, "Delete");
            m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),
                [this, hit](int result)
                {
                    if (result == 3) { promptSetPointValue(hit); return; }   // QA-Ed Problem 1: type-in
                    auto* l = lanePtr();
                    if (!l || hit < 0 || hit >= (int)l->points.size()) return;
                    AutomationLane before = *l;
                    if (result == 1)
                    {
                        l->points[hit].value01 = 0.5f;
                        commitEdit("Reset to Midpoint", before, *l);
                    }
                    else if (result == 2)
                    {
                        l->points.erase(l->points.begin() + hit);
                        commitEdit("Delete Point", before, *l);
                    }
                    if (onChanged) onChanged();
                    repaint();
                });
        }
        return;
    }

    // ── Curve handle drag (between two adjacent points) ────────────────────────
    if (mTool == EETool::Draw)
    {
        int chHit = hitTestCurveHandle(px, py);
        if (chHit >= 0)
        {
            mCurveHandleDrag       = chHit;
            mCurveHandleLaneBefore = *lane;
            return;
        }
    }

    if (mTool == EETool::Draw)
    {
        int hit = hitTestPt(px, py);
        mLaneBefore = *lane;

        if (hit >= 0)
        {
            mDragPt    = hit;
            mDragging  = true;
            mDragIsNew = false;
        }
        else
        {
            // Add new point
            ControlPoint pt;
            pt.timeTicks = jlimit(0.f, 1.f, beat / mTotalBeats);
            pt.value01   = val;
            pt.curveType = CurveType::Linear;
            lane->points.push_back(pt);
            std::sort(lane->points.begin(), lane->points.end(),
                [](const ControlPoint& a, const ControlPoint& b) { return a.timeTicks < b.timeTicks; });
            // Find the newly inserted point
            mDragPt = 0;
            for (int i = 0; i < (int)lane->points.size(); ++i)
                if (std::abs(lane->points[i].timeTicks - pt.timeTicks) < 0.001f
                    && std::abs(lane->points[i].value01 - val) < 0.001f)
                    { mDragPt = i; break; }
            mDragging  = true;
            mDragIsNew = true;
        }
        repaint();
    }
    else if (mTool == EETool::Paint)
    {
        // Continuous brush: same as Draw initial click
        mLaneBefore = *lane;
        ControlPoint pt;
        pt.timeTicks = jlimit(0.f, 1.f, beat / mTotalBeats);
        pt.value01   = val;
        pt.curveType = CurveType::Linear;
        lane->points.push_back(pt);
        std::sort(lane->points.begin(), lane->points.end(),
            [](const ControlPoint& a, const ControlPoint& b) { return a.timeTicks < b.timeTicks; });
        mDragging = true;
        mDragPt   = -1;
        repaint();
        if (onChanged) onChanged();
    }
    else if (mTool == EETool::Erase)
    {
        int hit = hitTestPt(px, py);
        if (hit >= 0)
        {
            AutomationLane before = *lane;
            // Erase: reset to range-start value rather than delete
            lane->points[hit].value01 = rangeStartVal();
            AutomationLane after = *lane;
            commitEdit("Erase Point", before, after);
            if (onChanged) onChanged();
            repaint();
        }
    }
    else if (mTool == EETool::Interpolate)
    {
        // Find the segment clicked (between two surrounding points)
        // Sort points, find which segment px falls between
        std::vector<ControlPoint>* pts = &lane->points;
        if (pts->empty()) return;
        std::sort(pts->begin(), pts->end(),
            [](const ControlPoint& a, const ControlPoint& b) { return a.timeTicks < b.timeTicks; });

        int segPt = -1;
        // Check if clicking on a node first → cycle that node's curve type
        int hit = hitTestPt(px, py);
        if (hit >= 0)
        {
            segPt = hit;
        }
        else
        {
            // Find the segment: point to the left of click
            for (int i = 0; i + 1 < (int)pts->size(); ++i)
            {
                float x1 = beatToX((*pts)[i].timeTicks * mTotalBeats);
                float x2 = beatToX((*pts)[i + 1].timeTicks * mTotalBeats);
                if (px >= x1 && px <= x2) { segPt = i; break; }
            }
        }
        if (segPt >= 0)
        {
            AutomationLane before = *lane;
            auto& ct = (*pts)[segPt].curveType;
            if      (ct == CurveType::Linear)  ct = CurveType::Stepped;
            else if (ct == CurveType::Stepped) ct = CurveType::Spline;
            else                               ct = CurveType::Linear;
            AutomationLane after = *lane;
            commitEdit("Interpolate", before, after);
            if (onChanged) onChanged();
            repaint();
        }
    }
    else if (mTool == EETool::Select)
    {
        int hit = hitTestPt(px, py);
        if (hit >= 0)
        {
            if (!e.mods.isCtrlDown()) mSelection.clear();
            auto it = std::find(mSelection.begin(), mSelection.end(), hit);
            if (it == mSelection.end()) mSelection.push_back(hit);
            else                        mSelection.erase(it);
        }
        else
        {
            if (!e.mods.isCtrlDown()) mSelection.clear();
            mMarqueeActive  = true;
            mMarqueeStart   = e.getPosition();
            mMarqueeCurrent = e.getPosition();
        }
        repaint();
    }
}

void EEAutomationGrid::mouseDrag(const MouseEvent& e)
{
    auto* lane = lanePtr();
    if (!lane) return;

    if (e.y < kRulerH && mTool != EETool::Draw) return;

    const float px  = (float)e.x;
    const float py  = (float)e.y;
    bool noSnap     = e.mods.isAltDown();

    // ── Curve handle drag ──────────────────────────────────────────────────────
    if (mCurveHandleDrag >= 0 && mCurveHandleDrag < (int)lane->points.size())
    {
        auto& p0 = lane->points[mCurveHandleDrag];
        // Find the next sorted point after p0
        float nextTick = 2.f;
        float nextVal  = p0.value01;
        for (const auto& pt : lane->points)
            if (pt.timeTicks > p0.timeTicks && pt.timeTicks < nextTick)
                { nextTick = pt.timeTicks; nextVal = pt.value01; }

        float dv = nextVal - p0.value01;
        if (nextTick <= 1.f && std::abs(dv) >= 0.02f)
        {
            float dragVal = jlimit(0.f, 1.f, yToVal(py));
            // FL formula inverse at x=0.5: evalFL(0.5, T) = 0.5*T + 0.5 => T = 2*y - 1
            float y_mid   = (dragVal - p0.value01) / dv;
            p0.tension    = jlimit(-0.999f, 0.999f, 2.f * y_mid - 1.f);
        }
        repaint();
        if (onChanged) onChanged();
        return;
    }

    if ((mTool == EETool::Draw) && mDragging && mDragPt >= 0
        && mDragPt < (int)lane->points.size())
    {
        float beat = jlimit(0.f, mTotalBeats, snapBeat(xToBeat(px), noSnap));
        // Apply midpoint detent: snap value01 to 0.5 within ±3%
        float val  = jlimit(0.f, 1.f, yToVal(py));
        if (std::abs(val - 0.5f) < 0.03f) val = 0.5f;
        lane->points[mDragPt].timeTicks = beat / mTotalBeats;
        lane->points[mDragPt].value01   = val;
        // Re-sort to keep points ordered
        std::sort(lane->points.begin(), lane->points.end(),
            [](const ControlPoint& a, const ControlPoint& b) { return a.timeTicks < b.timeTicks; });
        // Re-find our dragged point (may have moved index)
        for (int i = 0; i < (int)lane->points.size(); ++i)
            if (std::abs(lane->points[i].timeTicks - beat / mTotalBeats) < 0.002f
                && std::abs(lane->points[i].value01 - val) < 0.002f)
                { mDragPt = i; break; }
        repaint();
        if (onChanged) onChanged();
    }
    else if (mTool == EETool::Paint && mDragging)
    {
        float beat = jlimit(0.f, mTotalBeats, snapBeat(xToBeat(px), noSnap));
        float val  = jlimit(0.f, 1.f, yToVal(py));
        // Remove any existing point near this beat, then add
        float beatFrac = beat / mTotalBeats;
        float thresh   = 1.f / jmax(1, getWidth() - kValLabelW); // ~1px
        for (int i = (int)lane->points.size() - 1; i >= 0; --i)
            if (std::abs(lane->points[i].timeTicks - beatFrac) < thresh)
                lane->points.erase(lane->points.begin() + i);
        ControlPoint pt;
        pt.timeTicks = jlimit(0.f, 1.f, beatFrac);
        pt.value01   = val;
        pt.curveType = CurveType::Linear;
        lane->points.push_back(pt);
        std::sort(lane->points.begin(), lane->points.end(),
            [](const ControlPoint& a, const ControlPoint& b) { return a.timeTicks < b.timeTicks; });
        repaint();
        if (onChanged) onChanged();
    }
    else if (mTool == EETool::Select && mMarqueeActive)
    {
        mMarqueeCurrent = e.getPosition();
        repaint();
    }
}

void EEAutomationGrid::mouseUp(const MouseEvent& /*e*/)
{
    auto* lane = lanePtr();

    // ── Curve handle drag commit ───────────────────────────────────────────────
    if (mCurveHandleDrag >= 0)
    {
        if (lane)
            commitEdit("Adjust Curve", mCurveHandleLaneBefore, *lane);
        mCurveHandleDrag = -1;
        return;
    }

    if ((mTool == EETool::Draw || mTool == EETool::Paint) && mDragging)
    {
        if (lane)
        {
            AutomationLane after = *lane;
            commitEdit(mDragIsNew ? "Add Point" : "Move Point", mLaneBefore, after);
        }
        mDragging  = false;
        mDragPt    = -1;
        mDragIsNew = false;
    }
    else if (mTool == EETool::Select && mMarqueeActive)
    {
        // Finalise marquee selection
        if (lane)
        {
            auto r = Rectangle<int>(mMarqueeStart, mMarqueeCurrent).toFloat();
            for (int i = 0; i < (int)lane->points.size(); ++i)
            {
                float x = beatToX(lane->points[i].timeTicks * mTotalBeats);
                float y = valToY(lane->points[i].value01);
                if (r.contains(x, y))
                    if (std::find(mSelection.begin(), mSelection.end(), i) == mSelection.end())
                        mSelection.push_back(i);
            }
        }
        mMarqueeActive = false;
        repaint();
    }
}

void EEAutomationGrid::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        mPPBeat = jlimit(4.f, 400.f, mPPBeat * factor);
        repaint();
    }
}

void EEAutomationGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (mTool != EETool::Draw) return;
    const float px = (float)e.x;
    const float py = (float)e.y;

    // Double-click on curve handle diamond → reset tension to 0
    int chHit = hitTestCurveHandle(px, py);
    if (chHit >= 0)
    {
        auto* lane = lanePtr();
        if (!lane || chHit >= (int)lane->points.size()) return;
        AutomationLane before = *lane;
        lane->points[chHit].tension = 0.f;
        commitEdit("Reset Curve Tension", before, *lane);
        if (onChanged) onChanged();
        repaint();
    }
}

void EEAutomationGrid::updateCursor()
{
    switch (mTool)
    {
        case EETool::Draw:        setMouseCursor(MouseCursor::CrosshairCursor);  break;
        case EETool::Paint:       setMouseCursor(MouseCursor::CrosshairCursor);  break;
        case EETool::Erase:       setMouseCursor(MouseCursor::CrosshairCursor);  break;
        case EETool::Interpolate: setMouseCursor(MouseCursor::PointingHandCursor); break;
        case EETool::Select:      setMouseCursor(MouseCursor::NormalCursor);     break;
        case EETool::Zoom:        setMouseCursor(MouseCursor::PointingHandCursor); break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AutomationBrowserPane
// ─────────────────────────────────────────────────────────────────────────────

AutomationBrowserPane::AutomationBrowserPane()
{
    mList.setModel(this);
    mList.setRowHeight(22);
    mList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff141618));
    mList.setColour(juce::ListBox::outlineColourId,    juce::Colour(0xff2a2c2e));
    addAndMakeVisible(mList);
}

void AutomationBrowserPane::resized()
{
    mList.setBounds(0, 22, getWidth(), jmax(0, getHeight() - 22));
}

void AutomationBrowserPane::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff0d0e10));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);
    g.setColour(juce::Colour(0xff2a2c2e));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.f, 1.f);

    g.setColour(juce::Colour(0xff1e2022));
    g.fillRect(0, 0, getWidth(), 22);
    g.setColour(kTextGray);
    g.setFont(juce::Font(10.f, juce::Font::bold));
    g.drawText("Automation Clips", 8, 0, getWidth() - 8, 22,
               juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xff2a2c2e));
    g.drawHorizontalLine(22, 0.f, (float)getWidth());
}

void AutomationBrowserPane::refresh(PatternManager* pm)
{
    mRows.clear();
    if (pm)
    {
        for (int i = 0; i < pm->getNumBlocks(); ++i)
        {
            const auto& b = pm->getBlock(i);
            if (b.clipType == ClipType::Automation)
            {
                Row r;
                r.blockIdx = i;
                // Prefer the display-name resolver when the Event Editor has
                // one wired up (covers userDisplayName + effect-swap tracking).
                juce::String label;
                if (onResolveDisplayName) label = onResolveDisplayName(b.automationLane);
                if (label.isEmpty()) label = b.automationLane.paramId;
                if (label.isEmpty()) label = juce::String("Block ") + juce::String(i);
                r.label = label;
                // Batch E #3: flag rows whose target param has been deleted.
                r.stale = (onIsParamStale && b.automationLane.paramId.isNotEmpty()
                           && onIsParamStale(b.automationLane.paramId));
                mRows.push_back(r);
            }
        }
    }
    mList.updateContent();
    mList.repaint();
}

void AutomationBrowserPane::selectBlock(int blockIdx)
{
    for (int i = 0; i < (int)mRows.size(); ++i)
        if (mRows[i].blockIdx == blockIdx) { mList.selectRow(i, false, true); return; }
}

int AutomationBrowserPane::getNumRows() { return (int)mRows.size(); }

void AutomationBrowserPane::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel)
{
    if (row < 0 || row >= (int)mRows.size()) return;
    const bool isStale = mRows[row].stale;
    if (sel)
    {
        g.setColour(kSelBlue);
        g.fillRect(0, 0, w, h);
        g.setColour(isStale ? juce::Colour(0xffff8888) : juce::Colours::lightgrey);
    }
    else
    {
        g.setColour(row % 2 == 0 ? juce::Colour(0xff141618) : juce::Colour(0xff1a1c1e));
        g.fillRect(0, 0, w, h);
        // Batch E #3 (2026-05-01): stale rows -- target param deleted -- get
        // a dim red wash + red text so users see at a glance they're dead.
        if (isStale)
        {
            g.setColour(juce::Colour(0x33ff4040));
            g.fillRect(0, 0, w, h);
        }
        g.setColour(isStale ? juce::Colour(0xffff6060) : kTextGray);
    }
    g.setFont(juce::Font(11.f));
    juce::String text = isStale ? juce::String("[stale] ") + mRows[row].label
                                : mRows[row].label;
    g.drawText(text, 6, 0, w - 6, h, juce::Justification::centredLeft, true);
}

void AutomationBrowserPane::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int)mRows.size())
        if (onBlockSelected) onBlockSelected(mRows[row].blockIdx);
}

// ─────────────────────────────────────────────────────────────────────────────
// EventEditorContent
// ─────────────────────────────────────────────────────────────────────────────

EventEditorContent::EventEditorContent(VibeSynthProcessor& p, UndoManager& um)
    : mProcessor(p), mUM(um)
{
    // Menu bar
    mMenuBar = std::make_unique<MenuBarComponent>(this);
    addAndMakeVisible(*mMenuBar);

    // Mode tabs
    mAutoTab = std::make_unique<TextButton>("Auto");
    mLFOTab  = std::make_unique<TextButton>("LFO");
    mAutoTab->setClickingTogglesState(false);
    mLFOTab ->setClickingTogglesState(false);
    mAutoTab->onClick = [this] { switchMode(false); };
    mLFOTab ->onClick = [this] { switchMode(true);  };
    addAndMakeVisible(*mAutoTab);
    addAndMakeVisible(*mLFOTab);

    // LFO knobs
    mMinKnob  = std::make_unique<VKnob>("Min",  0.f);
    mMaxKnob  = std::make_unique<VKnob>("Max",  1.f);
    mTimeKnob = std::make_unique<VKnob>("Rate", 0.5f);
    mMinKnob ->slider.setRange(0.0, 1.0, 0.01);
    mMaxKnob ->slider.setRange(0.0, 1.0, 0.01);
    mTimeKnob->slider.setRange(0.1, 16.0, 0.1);   // bars per cycle
    mMinKnob ->slider.setValue(0.0, dontSendNotification);
    mMaxKnob ->slider.setValue(1.0, dontSendNotification);
    mTimeKnob->slider.setValue(4.0, dontSendNotification);
    mMinKnob ->slider.onValueChange = [this] { onKnobChanged(); };
    mMaxKnob ->slider.onValueChange = [this] { onKnobChanged(); };
    mTimeKnob->slider.onValueChange = [this] { onKnobChanged(); };
    addChildComponent(*mMinKnob);
    addChildComponent(*mMaxKnob);
    addChildComponent(*mTimeKnob);

    // Value display
    mValueDisplay = std::make_unique<Label>("val", "---");
    mValueDisplay->setColour(Label::backgroundColourId, kDeepRed);
    mValueDisplay->setColour(Label::textColourId, Colours::white.withAlpha(0.9f));
    mValueDisplay->setFont(Font(11.f, Font::bold));
    mValueDisplay->setJustificationType(Justification::centred);
    addAndMakeVisible(*mValueDisplay);

    // Snap combo
    mSnapLabel = std::make_unique<Label>("snap", "Snap:");
    mSnapLabel->setColour(Label::textColourId, kTextGray);
    mSnapLabel->setFont(Font(10.f));
    mSnapCombo = std::make_unique<ComboBox>("snap");
    mSnapCombo->addItem("Bar",    1);
    mSnapCombo->addItem("Beat",   2);
    mSnapCombo->addItem("1/8",    3);
    mSnapCombo->addItem("1/16",   4);
    mSnapCombo->addItem("1/32",   5);
    mSnapCombo->addItem("None",   6);
    mSnapCombo->setSelectedId(4, dontSendNotification);  // 1/16 default
    mSnapCombo->onChange = [this] {
        if (!mGrid) return;
        switch (mSnapCombo->getSelectedId())
        {
            case 1: mGrid->setSnapSub(1);  break;  // bar = 1/BPB per beat... keep at 1
            case 2: mGrid->setSnapSub(1);  break;  // beat
            case 3: mGrid->setSnapSub(2);  break;  // 1/8 = 2 per beat
            case 4: mGrid->setSnapSub(4);  break;  // 1/16
            case 5: mGrid->setSnapSub(8);  break;  // 1/32
            case 6: mGrid->setSnapSub(32); break;  // "None" = very fine
            default: break;
        }
    };
    addAndMakeVisible(*mSnapLabel);
    addAndMakeVisible(*mSnapCombo);

    // Automation grid
    mGrid = std::make_unique<EEAutomationGrid>(mUM);
    mGrid->onChanged = [this] { updateValueDisplay(); };
    addAndMakeVisible(*mGrid);

    // Browser pane - lists all automation blocks from the Builder grid
    mBrowserPane = std::make_unique<AutomationBrowserPane>();
    mBrowserPane->onBlockSelected = [this](int blockIdx) {
        if (!mPM || blockIdx < 0 || blockIdx >= mPM->getNumBlocks()) return;
        mBlockIdx = blockIdx;
        setBlock(mPM, blockIdx);
        // Update window title (uses display resolver for channel/effect/param).
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        {
            const auto& lane = mPM->getBlock(blockIdx).automationLane;
            juce::String pretty;
            if (onResolveDisplayName) pretty = onResolveDisplayName(lane);
            if (pretty.isEmpty())     pretty = lane.paramId;
            juce::String t = "Event Editor";
            if (pretty.isNotEmpty()) t += " - " + pretty;
            dw->setName(t);
        }
    };
    addAndMakeVisible(*mBrowserPane);

    // "New Automation Clip" button (left of snap combo)
    mNewAutoBtn = std::make_unique<juce::TextButton>("New Automation Clip");
    mNewAutoBtn->onClick = [this] { doNewAutomation(); };
    addAndMakeVisible(*mNewAutoBtn);

    // 5F-5: Title label (top-left of controls row)
    mTitleLabel = std::make_unique<juce::Label>("title", "Event Editor");
    mTitleLabel->setFont(juce::Font(13.f, juce::Font::bold));
    mTitleLabel->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));
    mTitleLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*mTitleLabel);

    // 5F-5: Delete automation button (right of "New Automation Clip")
    mDeleteBtn = std::make_unique<juce::TextButton>("Delete");
    mDeleteBtn->setTooltip("Clear all automation points in the current block");
    mDeleteBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff442222));
    mDeleteBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff8080));
    mDeleteBtn->onClick = [this] { doDeleteAutomationPoints(); };
    addAndMakeVisible(*mDeleteBtn);

    // 5F-5: Tool button strip (6 buttons below grid)
    {
        const std::array<juce::String, 6> labels    { "D", "P", "E", "I", "S", "Z" };
        const std::array<juce::String, 6> tooltips  {
            "Draw (P)", "Paint (B)", "Erase (D)", "Interpolate (I)", "Select (E)", "Zoom (Z)"
        };
        const std::array<EEAutomationGrid::EETool, 6> tools {
            EEAutomationGrid::EETool::Draw,
            EEAutomationGrid::EETool::Paint,
            EEAutomationGrid::EETool::Erase,
            EEAutomationGrid::EETool::Interpolate,
            EEAutomationGrid::EETool::Select,
            EEAutomationGrid::EETool::Zoom,
        };
        for (int i = 0; i < 6; ++i)
        {
            auto btn = std::make_unique<juce::TextButton>(labels[i]);
            btn->setTooltip(tooltips[i]);
            btn->setClickingTogglesState(true);
            btn->setRadioGroupId(42);   // all six mutually exclusive
            const auto tool = tools[i];
            btn->onClick = [this, tool] { setTool(tool); };
            addAndMakeVisible(*btn);
            mToolBtns[i] = std::move(btn);
        }
    }

    // 5F-5: Footer status bar
    mStatusBar = std::make_unique<juce::Label>("status", "Beat 0.00   Value 0.000");
    mStatusBar->setFont(juce::Font(10.f));
    mStatusBar->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
    mStatusBar->setColour(juce::Label::backgroundColourId, juce::Colour(0xff141618));
    mStatusBar->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*mStatusBar);

    // Wire grid hover → status bar
    mGrid->onHoverChanged = [this](float beat, float val01)
    {
        if (! mStatusBar) return;
        // QA-Ed (Problem 1): show the hovered value in the param's real units.
        juce::String pid;
        if (mPM && mBlockIdx >= 0 && mBlockIdx < mPM->getNumBlocks())
            pid = mPM->getBlock(mBlockIdx).automationLane.paramId;
        const juce::String vs = (mGrid->onFormatValue && pid.isNotEmpty())
                                    ? mGrid->onFormatValue(pid, val01)
                                    : juce::String(val01, 3);
        mStatusBar->setText("Beat " + juce::String(beat, 2) + "   " + vs,
                            juce::dontSendNotification);
    };

    updateTabStyles();
    updateToolButtonStates();
    // QA-Fd owner find (2026-07-11): undo/redo rewrote the lane but nothing
    // repainted the grid, so Ctrl+Z looked dead.  The UndoManager broadcasts
    // on perform/undo/redo from ANY window; repaint is the whole sync (the
    // grid draws straight from PatternManager).
    mUM.addChangeListener(this);

    startTimerHz(24);
}

EventEditorContent::~EventEditorContent()
{
    mUM.removeChangeListener(this);
    stopTimer();
}

void EventEditorContent::changeListenerCallback(ChangeBroadcaster*)
{
    if (mGrid) mGrid->repaint();
    updateValueDisplay();
}

void EventEditorContent::setBlock(PatternManager* pm, int blockIdx)
{
    mPM       = pm;
    mBlockIdx = blockIdx;

    if (pm && blockIdx >= 0 && blockIdx < pm->getNumBlocks())
    {
        const auto& block = pm->getBlock(blockIdx);
        const auto& lane  = block.automationLane;

        // 5F-5: update title label with current param id.
        // Prefer the display-name resolver (honours userDisplayName + current
        // rack state so swapping effects inside a slot updates the label).
        juce::String pretty;
        if (onResolveDisplayName) pretty = onResolveDisplayName(lane);
        if (pretty.isEmpty())     pretty = lane.paramId;

        if (mTitleLabel)
        {
            juce::String title = pretty;
            if (title.isEmpty()) title = "Event Editor";
            mTitleLabel->setText(title, juce::dontSendNotification);
        }

        // Also update the DocumentWindow caption (OS-drawn title bar) so it
        // stays in sync with the title label and with the row label selected
        // in the browser pane. Previously only the browser-pane click path
        // updated the caption; the initial ctor title + any external setBlock
        // call left the caption stale.
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        {
            juce::String cap = "Event Editor";
            if (pretty.isNotEmpty()) cap += " - " + pretty;
            dw->setName(cap);
        }

        // Calculate clip length in beats
        // Use 4 beats per bar as default if no time sig available
        float clipBeats = (float)block.lengthBars * 4.f;

        mGrid->setBlock(pm, blockIdx, clipBeats);

        // Highlight the current block in browser pane
        if (mBrowserPane) mBrowserPane->selectBlock(blockIdx);

        // Sync LFO knobs
        if (mInLFOMode)
        {
            mMinKnob ->slider.setValue(lane.lfoMin,  dontSendNotification);
            mMaxKnob ->slider.setValue(lane.lfoMax,  dontSendNotification);
            mTimeKnob->slider.setValue(lane.lfoRate, dontSendNotification);
        }

        // Refresh LFO mode from lane state
        if (lane.isLFO != mInLFOMode) switchMode(lane.isLFO);
    }

    if (mBrowserPane) mBrowserPane->refresh(mPM);
    updateValueDisplay();
    repaint();
}

void EventEditorContent::setTool(EEAutomationGrid::EETool t)
{
    if (mGrid) mGrid->setTool(t);
    updateToolButtonStates();   // 5F-5: sync button highlights
}

// 5F-5: sync tool button toggle states to the grid's active tool.
void EventEditorContent::updateToolButtonStates()
{
    if (!mGrid) return;
    const auto tool = mGrid->getTool();

    const std::array<EEAutomationGrid::EETool, 6> toolOrder {
        EEAutomationGrid::EETool::Draw,
        EEAutomationGrid::EETool::Paint,
        EEAutomationGrid::EETool::Erase,
        EEAutomationGrid::EETool::Interpolate,
        EEAutomationGrid::EETool::Select,
        EEAutomationGrid::EETool::Zoom,
    };
    for (int i = 0; i < 6; ++i)
        if (mToolBtns[i])
            mToolBtns[i]->setToggleState(toolOrder[i] == tool, juce::dontSendNotification);
}

// 5F-5: clear all control points in the current lane - single flat point at 0.5.
void EventEditorContent::doDeleteAutomationPoints()
{
    if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;

    auto& lane = mPM->getBlock(mBlockIdx).automationLane;
    AutomationLane before = lane;

    lane.points.clear();
    ControlPoint p0;
    p0.timeTicks = 0.f;
    p0.value01   = 0.5f;
    p0.curveType = CurveType::Linear;
    lane.points.push_back(p0);

    AutomationLane after = lane;

    mUM.perform(new AutomationLaneEditAction("Clear Automation",
        before, after,
        [this](const AutomationLane& s)
        {
            if (mPM && mBlockIdx >= 0 && mBlockIdx < mPM->getNumBlocks())
                mPM->getBlock(mBlockIdx).automationLane = s;
            if (mGrid) mGrid->repaint();
        }));

    if (mGrid) mGrid->repaint();
    updateValueDisplay();
}

EEAutomationGrid::EETool EventEditorContent::getTool() const
{
    return mGrid ? mGrid->getTool() : EEAutomationGrid::EETool::Draw;
}

void EventEditorContent::switchMode(bool lfo)
{
    mInLFOMode = lfo;

    if (mPM && mBlockIdx >= 0 && mBlockIdx < mPM->getNumBlocks())
        mPM->getBlock(mBlockIdx).automationLane.isLFO = lfo;

    mMinKnob ->setVisible(lfo);
    mMaxKnob ->setVisible(lfo);
    mTimeKnob->setVisible(lfo);

    if (mGrid) mGrid->setLFOMode(lfo);

    updateTabStyles();
    resized();
}

void EventEditorContent::updateTabStyles()
{
    // Auto tab
    mAutoTab->setColour(TextButton::buttonColourId,
        mInLFOMode ? Colour(0xff282c30) : Colour(0xff1a3a6a));
    mAutoTab->setColour(TextButton::textColourOffId,
        mInLFOMode ? kTextGray : Colours::white);
    // LFO tab
    mLFOTab->setColour(TextButton::buttonColourId,
        mInLFOMode ? Colour(0xff1a3a6a) : Colour(0xff282c30));
    mLFOTab->setColour(TextButton::textColourOffId,
        mInLFOMode ? Colours::white : kTextGray);
}

void EventEditorContent::updateValueDisplay()
{
    if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;
    const auto& lane = mPM->getBlock(mBlockIdx).automationLane;

    // Show value of first point, or midpoint if empty
    float val = lane.points.empty() ? 0.5f : lane.points[0].value01;
    // QA-Ed (Problem 1): show the param's real units (BPM / dB / Hz / ...) via the
    // grid's format hook; fall back to the raw 0..1 value when unwired.
    const juce::String valTxt = (mGrid && mGrid->onFormatValue && lane.paramId.isNotEmpty())
                                    ? mGrid->onFormatValue(lane.paramId, val)
                                    : String(val, 3);
    mValueDisplay->setText(valTxt, dontSendNotification);
}

void EventEditorContent::onKnobChanged()
{
    if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;
    auto& lane = mPM->getBlock(mBlockIdx).automationLane;
    lane.lfoMin  = (float)mMinKnob ->slider.getValue();
    lane.lfoMax  = (float)mMaxKnob ->slider.getValue();
    lane.lfoRate = (float)mTimeKnob->slider.getValue();
    if (mGrid) mGrid->repaint();
}

void EventEditorContent::timerCallback()
{
    updateValueDisplay();

    // Refresh browser pane if block count changed
    if (mPM && mBrowserPane)
    {
        int count = 0;
        for (int i = 0; i < mPM->getNumBlocks(); ++i)
            if (mPM->getBlock(i).clipType == ClipType::Automation) ++count;
        if (count != mLastBlockCount)
        {
            mLastBlockCount = count;
            mBrowserPane->refresh(mPM);
        }
    }
}

// ── resized ────────────────────────────────────────────────────────────────

void EventEditorContent::resized()
{
    auto bounds = getLocalBounds();

    // Menu bar (24px)
    if (mMenuBar) { mMenuBar->setBounds(bounds.removeFromTop(24)); }

    // 5F-5: Footer status bar (20px at the very bottom)
    if (mStatusBar)
        mStatusBar->setBounds(bounds.removeFromBottom(20));

    // Top controls row (34px)
    auto topRow = bounds.removeFromTop(34);
    {
        // 5F-5: Title label on the left (150px)
        if (mTitleLabel)
            mTitleLabel->setBounds(topRow.removeFromLeft(150).reduced(6, 4));

        // Mode tabs (60px each)
        mAutoTab->setBounds(topRow.removeFromLeft(60).reduced(2, 4));
        mLFOTab ->setBounds(topRow.removeFromLeft(60).reduced(2, 4));

        // LFO knobs (only visible in LFO mode) - 64px each
        if (mInLFOMode)
        {
            mMinKnob ->setBounds(topRow.removeFromLeft(64).reduced(2, 2));
            mMaxKnob ->setBounds(topRow.removeFromLeft(64).reduced(2, 2));
            mTimeKnob->setBounds(topRow.removeFromLeft(64).reduced(2, 2));
        }
        else
        {
            // Claim the space anyway (hidden components)
            mMinKnob ->setBounds(topRow.removeFromLeft(64).reduced(2, 2));
            mMaxKnob ->setBounds(topRow.removeFromLeft(64).reduced(2, 2));
            mTimeKnob->setBounds(topRow.removeFromLeft(64).reduced(2, 2));
        }

        // Value display (right-aligned, 72px)
        mValueDisplay->setBounds(topRow.removeFromRight(72).reduced(2, 4));

        // Snap controls (right of value display)
        mSnapCombo ->setBounds(topRow.removeFromRight(70).reduced(2, 6));
        mSnapLabel ->setBounds(topRow.removeFromRight(36).reduced(2, 6));

        // 5F-5: Delete button (right of snap)
        if (mDeleteBtn)
            mDeleteBtn->setBounds(topRow.removeFromRight(60).reduced(2, 6));

        // "New Automation Clip" button (left of Delete)
        if (mNewAutoBtn)
            mNewAutoBtn->setBounds(topRow.removeFromRight(130).reduced(2, 6));
    }

    // Separator line
    bounds.removeFromTop(1);

    // 5F-5: Tool button strip (22px row at bottom of main area)
    auto toolRow = bounds.removeFromBottom(22);
    {
        const int btnW = 28;
        int x = toolRow.getX() + 4;
        for (auto& btn : mToolBtns)
        {
            if (btn)
            {
                btn->setBounds(x, toolRow.getY() + 2, btnW, toolRow.getHeight() - 4);
                x += btnW + 2;
            }
        }
    }

    // Main area: grid | browser pane (180px right)
    auto mainArea  = bounds.reduced(0, 2);
    int  paneW     = 180;
    auto gridArea  = mainArea.withTrimmedRight(paneW + 4);
    auto paneArea  = mainArea.withTrimmedLeft(mainArea.getWidth() - paneW);

    if (mGrid)        mGrid->setBounds(gridArea);
    if (mBrowserPane) mBrowserPane->setBounds(paneArea);
}

void EventEditorContent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1c1e));

    // Separator line between controls and grid
    int sepY = 24 + 34 + 1;
    g.setColour(Colour(0xff2a2c2e));
    g.drawHorizontalLine(sepY, 0.f, (float)getWidth());
}

// ── MenuBarModel ───────────────────────────────────────────────────────────

StringArray EventEditorContent::getMenuBarNames()
{
    return { "File", "Edit", "Tools", "View", "Target Control", "Import MIDI" };
}

PopupMenu EventEditorContent::getMenuForIndex(int menuIndex, const String&)
{
    PopupMenu m;
    switch (menuIndex)
    {
        case 0: // File
            m.addItem(100, "New Event Editor");
            m.addItem(101, "Close");
            m.addSeparator();
            m.addItem(102, "Save Automation Data");
            break;
        case 1: // Edit
            m.addItem(200, "Undo (Ctrl+Z)",     mUM.canUndo());
            m.addItem(201, "Redo (Ctrl+Y)",     mUM.canRedo());
            m.addSeparator();
            m.addItem(202, "Select All (Ctrl+A)");
            m.addItem(203, "Copy (Ctrl+C)");
            m.addItem(204, "Paste (Ctrl+V)");
            m.addSeparator();
            m.addItem(205, "Erase to Range Start");
            m.addSeparator();
            m.addItem(206, "Convert to Clip (Douglas-Peucker)...");
            break;
        case 2: // Tools
            m.addItem(300, "Draw (P)",        true, getTool() == EEAutomationGrid::EETool::Draw);
            m.addItem(301, "Paint (B)",        true, getTool() == EEAutomationGrid::EETool::Paint);
            m.addItem(302, "Erase (D)",        true, getTool() == EEAutomationGrid::EETool::Erase);
            m.addItem(303, "Interpolate (I)",  true, getTool() == EEAutomationGrid::EETool::Interpolate);
            m.addItem(304, "Select (E)",       true, getTool() == EEAutomationGrid::EETool::Select);
            m.addItem(305, "Zoom (Z)",         true, getTool() == EEAutomationGrid::EETool::Zoom);
            break;
        case 3: // View
        {
            // Snap submenu
            PopupMenu snap;
            snap.addItem(400, "Bar");
            snap.addItem(401, "Beat");
            snap.addItem(402, "1/8");
            snap.addItem(403, "1/16");
            snap.addItem(404, "1/32");
            snap.addItem(405, "None");
            m.addSubMenu("Snap", snap);
            m.addSeparator();
            m.addItem(410, "Toggle LFO Mode");
            m.addItem(411, "Sync Zoom with Piano Roll");
            break;
        }
        case 4: // Target Control
            m.addItem(500, "Add Target...");
            m.addItem(501, "Remove Target");
            m.addItem(502, "Edit Targets...");
            m.addItem(503, "Animate Target");
            m.addItem(504, "Locate in Mixer");
            break;
        case 5: // Import MIDI (Ctrl+M)
            m.addItem(600, "Import MIDI CC Data... (Ctrl+M)");
            break;
        default: break;
    }
    return m;
}

void EventEditorContent::menuItemSelected(int id, int /*topIdx*/)
{
    switch (id)
    {
        case 101: // Close - find parent EventEditor window
            if (auto* ew = findParentComponentOfClass<EventEditor>())
                ew->closeButtonPressed();
            break;
        case 200: mUM.undo(); break;
        case 201: mUM.redo(); break;
        case 202: doSelectAll();    break;
        case 205: // Erase to range start
            if (mGrid && mPM && mBlockIdx >= 0 && mBlockIdx < mPM->getNumBlocks())
            {
                auto& lane = mPM->getBlock(mBlockIdx).automationLane;
                AutomationLane before = lane;
                float rv = 0.5f;
                if (!lane.points.empty()) rv = lane.points[0].value01;
                for (auto& pt : lane.points) pt.value01 = rv;
                AutomationLane after = lane;
                PatternManager* pm = mPM; int bi = mBlockIdx;
                mUM.beginNewTransaction("Erase to Range Start");
                mUM.perform(new AutomationLaneEditAction(
                    "Erase to Range Start", before, after,
                    [pm, bi](const AutomationLane& l) {
                        if (bi < pm->getNumBlocks())
                            pm->getBlock(bi).automationLane = l;
                    }));
                if (mGrid) mGrid->repaint();
            }
            break;
        case 206: doConvertToClip(); break;
        case 300: setTool(EEAutomationGrid::EETool::Draw);        break;
        case 301: setTool(EEAutomationGrid::EETool::Paint);       break;
        case 302: setTool(EEAutomationGrid::EETool::Erase);       break;
        case 303: setTool(EEAutomationGrid::EETool::Interpolate); break;
        case 304: setTool(EEAutomationGrid::EETool::Select);      break;
        case 305: setTool(EEAutomationGrid::EETool::Zoom);        break;
        case 400: if (mGrid) mGrid->setSnapSub(1);  break; // bar
        case 401: if (mGrid) mGrid->setSnapSub(1);  break; // beat
        case 402: if (mGrid) mGrid->setSnapSub(2);  break; // 1/8
        case 403: if (mGrid) mGrid->setSnapSub(4);  break; // 1/16
        case 404: if (mGrid) mGrid->setSnapSub(8);  break; // 1/32
        case 405: if (mGrid) mGrid->setSnapSub(32); break; // none
        case 410: switchMode(!mInLFOMode); break;
        case 600: doImportMidi(); break;
        default: break;
    }
}

void EventEditorContent::doSelectAll()
{
    if (mGrid) mGrid->selectAll();
}

void EventEditorContent::doNewAutomation()
{
    if (!onGetParamList || !onCreateAutomation) return;

    juce::StringArray params = onGetParamList();
    if (params.isEmpty()) return;

    juce::PopupMenu m;
    for (int i = 0; i < params.size(); ++i)
        m.addItem(i + 1, params[i]);

    m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(mNewAutoBtn.get()),
        [this, params](int result)
        {
            if (result <= 0 || result > params.size()) return;
            juce::String paramId = params[result - 1];
            int newIdx = onCreateAutomation(paramId);
            if (newIdx >= 0 && mPM)
            {
                mBlockIdx = newIdx;
                setBlock(mPM, newIdx);
            }
        });
}

bool EventEditorContent::keyPressed(const KeyPress& key, Component*)
{
    // Tool shortcuts (Windows: getKeyCode() returns uppercase ASCII)
    const int kc = key.getKeyCode();

    if (!key.getModifiers().isAnyModifierKeyDown())
    {
        if (kc == 'P') { setTool(EEAutomationGrid::EETool::Draw);        return true; }
        if (kc == 'B') { setTool(EEAutomationGrid::EETool::Paint);       return true; }
        if (kc == 'D') { setTool(EEAutomationGrid::EETool::Erase);       return true; }
        if (kc == 'I') { setTool(EEAutomationGrid::EETool::Interpolate); return true; }
        if (kc == 'E') { setTool(EEAutomationGrid::EETool::Select);      return true; }
        if (kc == 'Z') { setTool(EEAutomationGrid::EETool::Zoom);        return true; }

        if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
        {
            if (mGrid) mGrid->deleteSelected();
            return true;
        }
    }

    if (key.getModifiers().isCtrlDown())
    {
        if (kc == 'Z') { mUM.undo(); return true; }
        if (kc == 'Y') { mUM.redo(); return true; }
        if (kc == 'A') { doSelectAll(); return true; }
        if (kc == 'M') { doImportMidi(); return true; }
    }

    return false;
}

void EventEditorContent::doImportMidi()
{
    if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;

    auto chooser = std::make_shared<FileChooser>(
        "Import MIDI CC Data", File::getSpecialLocation(File::userDocumentsDirectory),
        "*.mid;*.midi");
    chooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
        [this, chooser](const FileChooser& fc) {
            auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;

            // Parse MIDI file for CC data on any channel
            FileInputStream stream(file);
            if (!stream.openedOk()) return;

            MidiFile midiFile;
            if (!midiFile.readFrom(stream)) return;

            midiFile.convertTimestampTicksToSeconds();

            // Determine clip duration in seconds (use first track's length)
            double clipDuration = 0.0;
            for (int t = 0; t < midiFile.getNumTracks(); ++t)
            {
                const MidiMessageSequence* seq = midiFile.getTrack(t);
                if (seq && seq->getEndTime() > clipDuration)
                    clipDuration = seq->getEndTime();
            }
            if (clipDuration < 0.001) clipDuration = 1.0;

            // Collect all CC messages (any CC number, any channel)
            // Group by CC number - ask user which to import
            std::map<int, std::vector<std::pair<double,int>>> ccData; // cc# → [(time, value)]
            for (int t = 0; t < midiFile.getNumTracks(); ++t)
            {
                const MidiMessageSequence* seq = midiFile.getTrack(t);
                if (!seq) continue;
                for (int i = 0; i < seq->getNumEvents(); ++i)
                {
                    const auto& msg = seq->getEventPointer(i)->message;
                    if (msg.isController())
                        ccData[msg.getControllerNumber()].push_back(
                            { msg.getTimeStamp(), msg.getControllerValue() });
                }
            }

            if (ccData.empty())
            {
                AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
                    "Import MIDI CC", "No CC data found in this MIDI file.");
                return;
            }

            // Build menu of available CC numbers
            PopupMenu m;
            std::vector<int> ccNums;
            for (auto& kv : ccData)
            {
                ccNums.push_back(kv.first);
                m.addItem((int)ccNums.size(),
                    "CC " + String(kv.first) + " (" + String(kv.second.size()) + " events)");
            }

            m.showMenuAsync(PopupMenu::Options(), [this, ccNums, ccData, clipDuration](int result) {
                if (result <= 0 || result > (int)ccNums.size()) return;
                if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;

                int ccNum = ccNums[result - 1];
                const auto& events = ccData.at(ccNum);

                auto& lane = mPM->getBlock(mBlockIdx).automationLane;
                AutomationLane before = lane;

                lane.points.clear();
                for (auto& [t, v] : events)
                {
                    ControlPoint pt;
                    pt.timeTicks = jlimit(0.f, 1.f, (float)(t / clipDuration));
                    pt.value01   = jlimit(0.f, 1.f, (float)v / 127.f);
                    pt.curveType = CurveType::Linear;
                    lane.points.push_back(pt);
                }
                std::sort(lane.points.begin(), lane.points.end(),
                    [](const ControlPoint& a, const ControlPoint& b)
                        { return a.timeTicks < b.timeTicks; });

                AutomationLane after = lane;
                PatternManager* pm = mPM; int bi = mBlockIdx;
                mUM.beginNewTransaction("Import MIDI CC");
                mUM.perform(new AutomationLaneEditAction(
                    "Import MIDI CC", before, after,
                    [pm, bi](const AutomationLane& l) {
                        if (bi < pm->getNumBlocks())
                            pm->getBlock(bi).automationLane = l;
                    }));

                if (mGrid) mGrid->repaint();
                updateValueDisplay();
            });
        });
}

// ── Douglas-Peucker line simplification ────────────────────────────────────
// Reduces a set of ControlPoints to a simpler curve with at most `epsilon`
// perpendicular deviation. Operates in normalised (timeTicks, value01) space.
static void douglasPeuckerImpl(const std::vector<ControlPoint>& pts,
                                int start, int end, float epsilon,
                                std::vector<bool>& keep)
{
    if (end <= start + 1) return;

    float ax = pts[start].timeTicks, ay = pts[start].value01;
    float bx = pts[end].timeTicks,   by = pts[end].value01;
    float dx = bx - ax, dy = by - ay;
    float len = std::sqrt(dx * dx + dy * dy);

    float maxDist = 0.f;
    int   maxIdx  = start;

    for (int i = start + 1; i < end; ++i)
    {
        float px = pts[i].timeTicks, py = pts[i].value01;
        float dist;
        if (len < 1e-8f)
            dist = std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
        else
            dist = std::abs(dy * px - dx * py + bx * ay - by * ax) / len;

        if (dist > maxDist) { maxDist = dist; maxIdx = i; }
    }

    if (maxDist > epsilon)
    {
        keep[maxIdx] = true;
        douglasPeuckerImpl(pts, start, maxIdx, epsilon, keep);
        douglasPeuckerImpl(pts, maxIdx, end,   epsilon, keep);
    }
}

void EventEditorContent::doConvertToClip()
{
    if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;
    auto& lane = mPM->getBlock(mBlockIdx).automationLane;
    if (lane.points.size() < 2)
    {
        AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
            "Convert to Clip", "Need at least 2 points to simplify.");
        return;
    }

    // Show sensitivity input (epsilon in normalised units, 0.001–0.05)
    auto* aw = new AlertWindow("Convert to Clip",
                               "Set Douglas-Peucker sensitivity (0.001 - 0.05).\n"
                               "Higher = fewer points (more simplified).",
                               AlertWindow::NoIcon);
    aw->addTextEditor("eps", "0.01", "Sensitivity:");
    aw->addButton("Simplify", 1);
    aw->addButton("Cancel",   0);

    aw->enterModalState(true, ModalCallbackFunction::create(
        [this, aw](int r) {
            if (r != 1) return;
            if (!mPM || mBlockIdx < 0 || mBlockIdx >= mPM->getNumBlocks()) return;

            float eps = jlimit(0.001f, 0.05f,
                               aw->getTextEditorContents("eps").getFloatValue());

            auto& lane = mPM->getBlock(mBlockIdx).automationLane;
            AutomationLane before = lane;

            // Sort points
            std::sort(lane.points.begin(), lane.points.end(),
                [](const ControlPoint& a, const ControlPoint& b)
                    { return a.timeTicks < b.timeTicks; });

            const int n = (int)lane.points.size();
            std::vector<bool> keep(n, false);
            keep[0] = true; keep[n - 1] = true;

            douglasPeuckerImpl(lane.points, 0, n - 1, eps, keep);

            std::vector<ControlPoint> simplified;
            for (int i = 0; i < n; ++i)
                if (keep[i]) simplified.push_back(lane.points[i]);

            lane.points = simplified;
            AutomationLane after = lane;

            PatternManager* pm = mPM; int bi = mBlockIdx;
            mUM.beginNewTransaction("Simplify Curve");
            mUM.perform(new AutomationLaneEditAction(
                "Simplify Curve", before, after,
                [pm, bi](const AutomationLane& l) {
                    if (bi < pm->getNumBlocks())
                        pm->getBlock(bi).automationLane = l;
                }));

            if (mGrid) mGrid->repaint();
            updateValueDisplay();

            AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
                "Simplified",
                String(n) + " points -> " + String((int)simplified.size()) + " points.");
        }), true);
}

// ─────────────────────────────────────────────────────────────────────────────
// EventEditor
// ─────────────────────────────────────────────────────────────────────────────

EventEditor::EventEditor(VibeSynthProcessor& p, UndoManager& um,
                         PatternManager* pm, int blockIdx,
                         const String& title)
    : DocumentWindow(title, Colour(0xff1a1c1e), DocumentWindow::allButtons)
    , mBlockIdx(blockIdx)
{
    auto content = std::make_unique<EventEditorContent>(p, um);
    content->setBlock(pm, blockIdx);
    content->setSize(900, 520);

    setContentOwned(content.release(), true);  // DocumentWindow takes ownership
    setResizable(true, true);
    setResizeLimits(600, 360, 2000, 1200);

    centreWithSize(900, 520);
    setVisible(true);
    setAlwaysOnTop(false);
    toFront(true);

    // Register content as key listener on this window
    if (auto* c = getContent())
        addKeyListener(c);
}

EventEditor::~EventEditor() = default;

void EventEditor::closeButtonPressed()
{
    // Copy callback before deletion (onClosed is a member that gets destroyed
    // when OwnedArray deletes us inside the callback).
    auto cb = onClosed;
    if (cb) cb(this);     // owner calls mEventEditors.removeObject(this, true) → deletes us
    else    delete this;  // no owner: safe self-delete (no member access after this line)
}

void EventEditor::setTool(EEAutomationGrid::EETool t)
{
    if (auto* c = getContent()) c->setTool(t);
}
