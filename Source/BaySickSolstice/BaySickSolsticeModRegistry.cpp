#include "BaySickSolsticeModRegistry.h"

namespace
{
    // ValueTree identifiers. Kept short to keep serialised XML compact.
    const juce::Identifier kRoot       { "harmlessMod" };
    const juce::Identifier kTarget     { "target" };
    const juce::Identifier kSource     { "source" };
    const juce::Identifier kTab        { "tab" };
    const juce::Identifier kPoint      { "pt" };

    const juce::Identifier kAttrId          { "id" };
    const juce::Identifier kAttrActiveSrc   { "activeSource" };
    const juce::Identifier kAttrType        { "type" };
    const juce::Identifier kAttrDepth       { "depth" };
    const juce::Identifier kAttrLength      { "length" };
    const juce::Identifier kAttrTempoSync   { "tempoSync" };
    const juce::Identifier kAttrGlobalAB    { "globalAB" };
    // kAttrLfoRate dropped 2026-04-20: LFO rate is unified with ModSourceState::length.
    const juce::Identifier kAttrLfoShape    { "lfoShape" };
    const juce::Identifier kAttrActiveTab   { "activeTab" };
    const juce::Identifier kAttrIndex       { "index" };
    const juce::Identifier kAttrSustainTime { "sustainTime" };
    const juce::Identifier kAttrSpd         { "spd" };
    const juce::Identifier kAttrTns         { "tns" };
    const juce::Identifier kAttrSkew        { "skew" };
    const juce::Identifier kAttrPw          { "pw" };
    const juce::Identifier kAttrT           { "t" };
    const juce::Identifier kAttrV           { "v" };
    const juce::Identifier kAttrC           { "c" };
}

BaySickSolsticeModRegistry::BaySickSolsticeModRegistry() = default;

const juce::Identifier& BaySickSolsticeModRegistry::rootId() noexcept { return kRoot; }

void BaySickSolsticeModRegistry::registerTarget (juce::String paramId, juce::String displayName)
{
    if (findTarget (paramId) != nullptr) return;   // idempotent
    auto t = std::make_unique<ModTarget>();
    t->paramId     = std::move (paramId);
    t->displayName = std::move (displayName);
    mTargets.push_back (std::move (t));
}

ModTarget* BaySickSolsticeModRegistry::findTarget (const juce::String& paramId) noexcept
{
    for (auto& t : mTargets)
        if (t->paramId == paramId) return t.get();
    return nullptr;
}

const ModTarget* BaySickSolsticeModRegistry::findTarget (const juce::String& paramId) const noexcept
{
    for (auto& t : mTargets)
        if (t->paramId == paramId) return t.get();
    return nullptr;
}

juce::ValueTree BaySickSolsticeModRegistry::toValueTree() const
{
    juce::ValueTree root (kRoot);

    for (const auto& t : mTargets)
    {
        juce::ValueTree tv (kTarget);
        tv.setProperty (kAttrId, t->paramId, nullptr);
        tv.setProperty (kAttrActiveSrc, (int) t->activeSource, nullptr);

        for (int si = 0; si < (int) ModSource::NumSources; ++si)
        {
            const auto& src = t->sources[si];
            juce::ValueTree sv (kSource);
            sv.setProperty (kAttrType,      si,             nullptr);
            sv.setProperty (kAttrDepth,     src.depth,      nullptr);
            sv.setProperty (kAttrLength,    src.length,     nullptr);
            sv.setProperty (kAttrTempoSync, src.tempoSync,  nullptr);
            sv.setProperty (kAttrGlobalAB,  src.globalAB,   nullptr);
            sv.setProperty (kAttrLfoShape,  src.lfoShape,   nullptr);
            sv.setProperty (kAttrActiveTab, src.activeTab,  nullptr);

            for (int ti = 0; ti < (int) ModTab::NumTabs; ++ti)
            {
                const auto& tab = src.tabs[ti];
                juce::ValueTree tbv (kTab);
                tbv.setProperty (kAttrIndex,       ti,               nullptr);
                tbv.setProperty (kAttrSustainTime, tab.sustainTime,  nullptr);
                tbv.setProperty (kAttrSpd,  tab.spd,  nullptr);
                tbv.setProperty (kAttrTns,  tab.tns,  nullptr);
                tbv.setProperty (kAttrSkew, tab.skew, nullptr);
                tbv.setProperty (kAttrPw,   tab.pw,   nullptr);

                for (const auto& p : tab.points)
                {
                    juce::ValueTree pv (kPoint);
                    pv.setProperty (kAttrT, p.time,      nullptr);
                    pv.setProperty (kAttrV, p.value,     nullptr);
                    pv.setProperty (kAttrC, p.curveType, nullptr);
                    tbv.appendChild (pv, nullptr);
                }

                sv.appendChild (tbv, nullptr);
            }

            tv.appendChild (sv, nullptr);
        }

        root.appendChild (tv, nullptr);
    }

    return root;
}

void BaySickSolsticeModRegistry::fromValueTree (const juce::ValueTree& root)
{
    if (! root.hasType (kRoot)) return;

    const juce::SpinLock::ScopedLockType lock (mEditLock);

    for (int i = 0; i < root.getNumChildren(); ++i)
    {
        auto tv = root.getChild (i);
        if (! tv.hasType (kTarget)) continue;

        const juce::String id = tv.getProperty (kAttrId).toString();
        auto* target = findTarget (id);
        if (target == nullptr) continue;   // unknown / removed target

        target->activeSource = (ModSource) (int) tv.getProperty (kAttrActiveSrc, 0);

        for (int j = 0; j < tv.getNumChildren(); ++j)
        {
            auto sv = tv.getChild (j);
            if (! sv.hasType (kSource)) continue;

            const int si = (int) sv.getProperty (kAttrType, -1);
            if (si < 0 || si >= (int) ModSource::NumSources) continue;

            auto& src = target->sources[si];
            src.depth     = (float) sv.getProperty (kAttrDepth,     src.depth);
            src.length    = (float) sv.getProperty (kAttrLength,    src.length);
            src.tempoSync =         sv.getProperty (kAttrTempoSync, src.tempoSync);
            src.globalAB  =         sv.getProperty (kAttrGlobalAB,  src.globalAB);
            src.lfoShape  = (int)   sv.getProperty (kAttrLfoShape,  src.lfoShape);
            src.activeTab = (int)   sv.getProperty (kAttrActiveTab, src.activeTab);

            for (int k = 0; k < sv.getNumChildren(); ++k)
            {
                auto tbv = sv.getChild (k);
                if (! tbv.hasType (kTab)) continue;

                const int ti = (int) tbv.getProperty (kAttrIndex, -1);
                if (ti < 0 || ti >= (int) ModTab::NumTabs) continue;

                auto& tab = src.tabs[ti];
                tab.sustainTime = (float) tbv.getProperty (kAttrSustainTime, tab.sustainTime);
                tab.spd  = (float) tbv.getProperty (kAttrSpd,  tab.spd);
                tab.tns  = (float) tbv.getProperty (kAttrTns,  tab.tns);
                tab.skew = (float) tbv.getProperty (kAttrSkew, tab.skew);
                tab.pw   = (float) tbv.getProperty (kAttrPw,   tab.pw);

                tab.points.clear();
                for (int m = 0; m < tbv.getNumChildren(); ++m)
                {
                    auto pv = tbv.getChild (m);
                    if (! pv.hasType (kPoint)) continue;
                    BaySickSolsticeCurvePoint p;
                    p.time      = (float) pv.getProperty (kAttrT, 0.0f);
                    p.value     = (float) pv.getProperty (kAttrV, 0.5f);
                    p.curveType = (int)   pv.getProperty (kAttrC, 1);
                    tab.points.push_back (p);
                }

                // Ensure at least one point so sampling never hits an empty vector.
                if (tab.points.empty())
                    tab = ModCurveState::makeDefault();
            }
        }
    }

    publishSnapshot();
}
