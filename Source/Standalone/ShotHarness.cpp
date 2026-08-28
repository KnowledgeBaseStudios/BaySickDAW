// BaySickDAW - the manual screenshot harness (QA-ManualPress M-1).
// See ShotHarness.h for the contract.  Structure mirrors kbs_shot: one boot,
// one save path, a registry of figure functions.  Headless rules that shape
// everything here: timers never fire (any polled state is set by direct
// calls), no component ever reaches the desktop (WorkspaceWindow only grows
// a native peer in attachTo, which is never called), and the default
// LookAndFeel must be installed by hand or every widget renders stock JUCE.
#include "ShotHarness.h"
#include "ShotFactories.h"
#include "ShotMenuHook.h"

#include "../PluginProcessor.h"
#include "../EngineRig.h"
#include "../EffectRack.h"
#include "StandaloneApp.h"
#include "SharedUI.h"
#include "WorkspaceWindow.h"
#include "WindowChrome.h"
#include "GlobalTransportBar.h"
#include "RibbonTabBar.h"
#include "KeyBindsWindow.h"
#include "KeyBindings.h"
#include "RustyDrumsMapWindow.h"
#include "BuilderPage.h"
#include "MixerPage.h"
#include "LayersPage.h"
#include "BassPage.h"
#include "DrumPage.h"
#include "PluginsPage.h"
#include "../Vox/VoxPage.h"
#include "MetroPanel.h"
#include "PagePresetIO.h"
#include "SlotComponent.h"
#include "StandaloneEditor.h"
#include "EffectsPage.h"
#include "TrackSelectionManager.h"
#include "EffectWindows.h"
#include "UndoHistoryWindow.h"
#include "EventEditor.h"
#include "PianoRollPage.h"
#include "DrumKitGrid.h"
#include "BaySickRustyDrumsPage.h"
#include "AriaControlPanel.h"
#include "MasterAnalyzerWindow.h"
#include "PluginsManagerWindow.h"
#include "../Inst/InstPage.h"
#include "../SampleLibrary.h"
#include "../BaySickPlayer/BaySickPlayerEditor.h"
#include "../BaySickVocal/BaySickVocalProcessor.h"
#include "../BaySickVocal/BaySickVocalEditor.h"
#include "../BaySickNAMIR/BaySickNAMIREditor.h"
#include "../BaySickSynth/BaySickSynthEditor.h"
#include "../Harmless/HarmlessEditor.h"
#include "../BaySickBass/BaySickBassEditor.h"
#include "../BaySickPedals/BaySickPedalsProcessor.h"
#include "../BaySickPedals/BaySickPedalsEditor.h"
#include "../AppPaths.h"

#include <fontaudio/fontaudio.h>

#include <iostream>

namespace shots
{

std::function<void (const juce::PopupMenu&,
                    const juce::PopupMenu::Options&)> gMenuCapture;

bool maybeCapture (const juce::PopupMenu& m, const juce::PopupMenu::Options& o)
{
    if (gMenuCapture) { gMenuCapture (m, o); return true; }
    return false;
}

namespace
{

// ── the world every figure shoots against ─────────────────────────────────
struct World
{
    std::unique_ptr<BaySickDAWProcessor>     proc;
    std::unique_ptr<StandalonePlayHead>      playHead;
    std::unique_ptr<PatternManager>          patterns;
    std::unique_ptr<juce::AudioDeviceManager> deviceMgr;   // never initialise()d
    int blocksAdded = 0;   // every harness addBlock counts, so indices hold
};

juce::File     gOutDir;
juce::StringArray gWanted;
bool           gDocsMode = false;
juce::File     gDocsFile;
juce::DynamicObject::Ptr gDocs;
BaySickDAWProcessor*     gDocsProc = nullptr;
float          gScaleOverride = 0.0f;   // 0 = per-figure default
int            gWritten = 0, gFailed = 0;

bool want (const juce::String& name)
{
    return gWanted.isEmpty() || gWanted.contains (name, true);
}

// ── the --docs dump (QA-ManualPress M-3) ──────────────────────────────────
// Control tables are the half of the manual that rots: every number in them
// must come from the code.  While shooting, the walk below collects every
// stamped control (componentID carries the param id; VKnob carries paramId)
// plus its slider-rendered range strings - the SAME formatting the user sees
// on screen - and resolves the id against the owning APVTS for the
// registered name, default, skew and choice names.

juce::String stripLanePrefix (const juce::String& id)
{
    for (const char* fam : { "vox", "inst", "plugtab" })
    {
        const juce::String f (fam);
        if (! id.startsWith (f)) continue;
        int i = f.length();
        int digits = 0;
        while (i < id.length() && juce::CharacterFunctions::isDigit (id[i]))
        { ++i; ++digits; }
        if (digits > 0 && i < id.length() && id[i] == '_')
            return id.substring (i + 1);
    }
    return id;
}

juce::RangedAudioParameter* resolveDocParam (BaySickDAWProcessor& proc,
                                             const juce::String& rawId)
{
    const auto id = stripLanePrefix (rawId);
    if (auto* p = proc.apvts.getParameter (id)) return p;

    static const TabKind kKinds[] = {
        TabKind::Layers, TabKind::Bass, TabKind::Drums, TabKind::Clips,
        TabKind::Vox, TabKind::Inst, TabKind::Plugins,
    };
    auto& rig = proc.engineRig();
    for (const auto kind : kKinds)
        for (int i = 0; i < EngineRig::capacityOf (kind); ++i)
        {
            const auto* t = rig.findTab (kind, i);
            if (t == nullptr) continue;
            if (auto* ap = EngineRig::apvtsOf (t->engine.get()))
                if (auto* p = ap->getParameter (id)) return p;
            if (t->namIr != nullptr)
                if (auto* ap = EngineRig::apvtsOf (t->namIr))
                    if (auto* p = ap->getParameter (id)) return p;
            if (t->pedals != nullptr)
                if (auto* ap = EngineRig::apvtsOf (t->pedals))
                    if (auto* p = ap->getParameter (id)) return p;
            if (kind == TabKind::Vox)
                if (auto* v = dynamic_cast<BaySickVocalProcessor*> (t->engine.get()))
                    if (auto* ap = EngineRig::apvtsOf (&v->getNamIrProcessor()))
                        if (auto* p = ap->getParameter (id)) return p;
        }
    return nullptr;
}

juce::var docRowForParam (BaySickDAWProcessor& proc, const juce::String& id)
{
    auto* p = resolveDocParam (proc, id);
    if (p == nullptr) return {};
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("id", id);
    o->setProperty ("name", p->getName (64));
    const auto& r = p->getNormalisableRange();
    o->setProperty ("min", r.start);
    o->setProperty ("max", r.end);
    o->setProperty ("interval", r.interval);
    o->setProperty ("skew", r.skew);
    o->setProperty ("default", r.convertFrom0to1 (p->getDefaultValue()));
    o->setProperty ("defaultText", p->getText (p->getDefaultValue(), 24));
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (p))
    {
        juce::Array<juce::var> names;
        for (const auto& s : ch->choices) names.add (s);
        o->setProperty ("choices", names);
        o->setProperty ("kind", "choice");
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
        o->setProperty ("kind", "toggle");
    else
        o->setProperty ("kind", "value");
    return juce::var (o.get());
}

void collectDocRows (juce::Component& root, juce::Component& c,
                     juce::Rectangle<int> area, juce::Array<juce::var>& out)
{
    // The root itself never gets setVisible(true) headless (snapshots paint
    // regardless); only CHILD visibility separates shown from hidden state.
    if (&c != &root && ! c.isVisible()) return;
    if (dynamic_cast<PageMenuBar*> (&c) != nullptr) return;   // window chrome

    juce::String id, label, tip;
    juce::Slider* slider = nullptr;
    juce::ComboBox* combo = nullptr;
    const char* kind = nullptr;
    bool emitted = false;

    if (auto* vk = dynamic_cast<VKnob*> (&c))
    {
        id = vk->paramId;
        label = vk->label.getText();
        tip = vk->slider.getTooltip();
        slider = &vk->slider;
        kind = "knob";
    }
    else if ((slider = dynamic_cast<juce::Slider*> (&c)) != nullptr)
    {
        if ((bool) c.getProperties()["vknob_slider"]) return;  // the VKnob emits
        id = c.getComponentID();
        tip = slider->getTooltip();
        kind = "slider";
        if (id.isEmpty()) slider = nullptr;   // nameless row = useless row
    }
    else if ((combo = dynamic_cast<juce::ComboBox*> (&c)) != nullptr)
    {
        id = c.getComponentID();
        kind = "combo";
    }
    else if (auto* btn = dynamic_cast<juce::Button*> (&c))
    {
        id = c.getComponentID();
        label = btn->getButtonText();
        tip = btn->getTooltip();
        kind = "toggle";
    }

    const bool wantRow = (kind != nullptr)
                         && (id.isNotEmpty()
                             || (slider != nullptr && juce::String (kind) == "knob"));
    if (wantRow)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("id", id);
        o->setProperty ("kind", kind);
        if (label.isNotEmpty()) o->setProperty ("label", label);
        if (tip.isNotEmpty())   o->setProperty ("tip", tip);

        const auto b = root.getLocalArea (&c, c.getLocalBounds());
        if (area.getWidth() > 0 && area.getHeight() > 0)
        {
            juce::Array<juce::var> pct;
            pct.add (100.0 * (b.getX() - area.getX()) / area.getWidth());
            pct.add (100.0 * (b.getY() - area.getY()) / area.getHeight());
            pct.add (100.0 * b.getWidth()  / area.getWidth());
            pct.add (100.0 * b.getHeight() / area.getHeight());
            o->setProperty ("bounds", pct);
        }

        if (slider != nullptr)
        {
            o->setProperty ("min", slider->getMinimum());
            o->setProperty ("max", slider->getMaximum());
            o->setProperty ("interval", slider->getInterval());
            o->setProperty ("minText", slider->getTextFromValue (slider->getMinimum()));
            o->setProperty ("maxText", slider->getTextFromValue (slider->getMaximum()));
            o->setProperty ("valueText", slider->getTextFromValue (slider->getValue()));
            if (slider->isDoubleClickReturnEnabled())
                o->setProperty ("defText",
                    slider->getTextFromValue (slider->getDoubleClickReturnValue()));
        }
        if (combo != nullptr)
        {
            juce::Array<juce::var> items;
            for (int i = 0; i < combo->getNumItems(); ++i)
                items.add (combo->getItemText (i));
            o->setProperty ("choices", items);
        }
        if (id.isNotEmpty() && gDocsProc != nullptr)
            if (auto pv = docRowForParam (*gDocsProc, id); ! pv.isVoid())
                o->setProperty ("param", pv);

        out.add (juce::var (o.get()));
        emitted = true;
    }

    if (! emitted)
        for (auto* child : c.getChildren())
            if (child != nullptr)
                collectDocRows (root, *child, area, out);
}

// Params the walk cannot see (controls that never got a componentID) but the
// figure plainly shows - documented by id, numbers still resolved from code.
struct ExtraDoc { const char* figure; const char* id; };
const ExtraDoc kExtraDocs[] = {
    { "Mixer", "mixer_master_width" },
    { "Mixer", "mixer_master_mute" },
    { "Mixer", "mixer_master_solo" },
    { "EQ",    "mixer_master_preeq_b1Freq" },
    { "EQ",    "mixer_master_preeq_b1Q" },
    { "EQ",    "mixer_master_preeq_b1Type" },
    { "EQ",    "mixer_master_preeq_b1Slope" },
};

// QA-ManualPress M-4 option C (Jeff, 2026-08-28): a component declares which
// manual callout it anchors -
//     comp.getProperties().set (kDotAnchor, "BSSBOSC-4");
// and the dot places itself from the live layout forever after.  A PROPERTY,
// not a componentID: the target is often a knob that already carries its
// param id, and region containers (an OSC group, a caption row) carry no id
// at all.  Unstamped callouts keep their hand coordinate and are reported.
void collectDotAnchors (juce::Component& root, juce::Component& c,
                        juce::Rectangle<int> area, juce::Array<juce::var>& out)
{
    if (&c != &root && ! c.isVisible()) return;

    const auto v = c.getProperties()[kDotAnchor];
    if (! v.isVoid() && area.getWidth() > 0 && area.getHeight() > 0)
    {
        // "CODE-n" anchors the whole component; "CODE-n@x,y,w,h" anchors a
        // sub-rect in the component's own coordinates, for callouts whose
        // target is PAINTED rather than a child component (a bypass LED, a
        // meter bar).  Semicolons separate several on one component.
        juce::StringArray decls;
        decls.addTokens (v.toString(), ";", "");
        for (auto decl : decls)
        {
            decl = decl.trim();
            if (decl.isEmpty()) continue;

            juce::Rectangle<int> local = c.getLocalBounds();
            juce::String id = decl;
            if (decl.contains ("@"))
            {
                id = decl.upToFirstOccurrenceOf ("@", false, false).trim();
                juce::StringArray n;
                n.addTokens (decl.fromFirstOccurrenceOf ("@", false, false), ",", "");
                if (n.size() == 4)
                    local = juce::Rectangle<int> (n[0].getIntValue(), n[1].getIntValue(),
                                                  n[2].getIntValue(), n[3].getIntValue());
            }
            if (local.isEmpty()) continue;

            const auto b = root.getLocalArea (&c, local);
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("anchor", id);
            juce::Array<juce::var> r;
            r.add (100.0 * (b.getX()     - area.getX()) / area.getWidth());
            r.add (100.0 * (b.getY()     - area.getY()) / area.getHeight());
            r.add (100.0 * b.getWidth()  / area.getWidth());
            r.add (100.0 * b.getHeight() / area.getHeight());
            o->setProperty ("bounds", r);
            out.add (juce::var (o.get()));
        }
    }

    for (auto* child : c.getChildren())
        if (child != nullptr)
            collectDotAnchors (root, *child, area, out);
}

bool gInMenuSave = false;   // saveMenu arms this: a menu figure has no table

void docsAdd (juce::Component& c, const juce::String& name,
              juce::Rectangle<int> area)
{
    if (! gDocsMode || gDocs == nullptr || gInMenuSave) return;

    juce::Array<juce::var> rows;
    collectDocRows (c, c, area, rows);
    for (const auto& x : kExtraDocs)
        if (name == x.figure && gDocsProc != nullptr)
            if (auto pv = docRowForParam (*gDocsProc, x.id); ! pv.isVoid())
            {
                juce::DynamicObject::Ptr o = new juce::DynamicObject();
                o->setProperty ("id", juce::String (x.id));
                o->setProperty ("kind", "supplement");
                o->setProperty ("param", pv);
                rows.add (juce::var (o.get()));
            }
    juce::Array<juce::var> anchors;
    collectDotAnchors (c, c, area, anchors);
    if (rows.isEmpty() && anchors.isEmpty()) return;

    juce::DynamicObject::Ptr fig = new juce::DynamicObject();
    if (! rows.isEmpty())    fig->setProperty ("controls", rows);
    if (! anchors.isEmpty()) fig->setProperty ("anchors", anchors);
    gDocs->setProperty (name, juce::var (fig.get()));
}

// Per-figure scale defaults to 1.25: the shipped masters were hand-captured
// on a 125% desktop, the manual generator renders crops at NATIVE master
// pixels, and the marker coordinates live in master-percent - so parity
// with the old masters keeps the whole page layout stable.  DELIBERATE
// DEVIATION from the plan's "2x" (recorded in the running notes): 2x needs
// the generator to learn scaled rendering first; --scale=2 exists for the
// day it does.
void save (juce::Component& c, const juce::String& name, float scale,
           juce::Rectangle<int> area = {})
{
    if (gScaleOverride > 0.0f) scale = gScaleOverride;
    if (area.isEmpty()) area = c.getLocalBounds();
    docsAdd (c, name, area);

    const auto shot = c.createComponentSnapshot (area, false, scale);
    const auto file = gOutDir.getChildFile (name + ".png");
    file.deleteFile();

    juce::FileOutputStream out (file);
    if (shot.isValid() && out.openedOk()
        && juce::PNGImageFormat().writeImageToStream (shot, out))
    {
        ++gWritten;
        std::cout << "  " << file.getFileName() << "  "
                  << shot.getWidth() << "x" << shot.getHeight() << std::endl;
    }
    else
    {
        ++gFailed;
        std::cout << "  FAILED " << file.getFileName() << std::endl;
    }
}

// A dummy swing binding: non-empty getters make the title-strip knob RENDER
// (an empty getMix hides it, and the masters show it), parked at zero.
void dressSwingKnob (PageMenuBar& bar)
{
    bar.setSwingKnobSlot ([] { return 0.0f; }, [] (float) {},
                          [] { return false; }, [] (bool) {});
}

// ── the menu composer ─────────────────────────────────────────────────────
// A PopupMenu cannot be shown here (JUCE_MODAL_LOOPS_PERMITTED=0 removes
// show(), MenuWindow is private, and a real menu would need the desktop),
// so this reproduces MenuWindow's single-column layout and paints through
// the SAME LookAndFeel virtuals the real window calls - the metrics below
// (measured text carries the shortcut suffix, heights clamp at 600, a
// trailing separator is dropped, the border pads only vertically) are
// copied from juce_PopupMenu.cpp so drawn == shown.
struct MenuCanvas : public juce::Component
{
    struct Row { juce::PopupMenu::Item* item; int h = 0; };

    MenuCanvas (juce::PopupMenu& m, const juce::PopupMenu::Options& o,
                juce::LookAndFeel& laf)
        : opts (o)
    {
        setLookAndFeel (&laf);
        for (juce::PopupMenu::MenuItemIterator it (m); it.next();)
            rows.push_back ({ &it.getItem(), 0 });
        while (! rows.empty() && rows.back().item->isSeparator)
            rows.pop_back();

        const int border = laf.getPopupMenuBorderSizeWithOptions (opts);
        int colW = opts.getStandardItemHeight();
        int totalH = border;
        for (auto& r : rows)
        {
            auto& it = *r.item;
            fillShortcutFromCommandManager (it);
            int iw = 80, ih = 16;
            if (it.customComponent != nullptr)
                it.customComponent->getIdealSize (iw, ih);
            else if (it.isSectionHeader)
                laf.getIdealPopupMenuSectionHeaderSizeWithOptions (it.text, -1,
                                                                   iw, ih, opts);
            else
            {
                const auto txt = it.shortcutKeyDescription.isNotEmpty()
                                   ? it.text + "   " + it.shortcutKeyDescription
                                   : it.text;
                laf.getIdealPopupMenuItemSizeWithOptions (txt, it.isSeparator,
                                                          opts.getStandardItemHeight(),
                                                          iw, ih, opts);
            }
            r.h = juce::jlimit (1, 600, ih);
            colW = juce::jmax (colW, iw);
            totalH += r.h;
        }
        const int w = juce::jmax (opts.getMinimumWidth(), colW + border * 2);
        setSize (w, totalH + border);

        int cy = border;
        for (auto& r : rows)
        {
            if (auto* cc = r.item->customComponent.get())
            {
                addAndMakeVisible (cc);
                cc->setBounds (0, cy, w, r.h);
            }
            cy += r.h;
        }
    }

    ~MenuCanvas() override { setLookAndFeel (nullptr); }

    static void fillShortcutFromCommandManager (juce::PopupMenu::Item& it)
    {
        if (it.commandManager == nullptr || it.itemID == 0
            || it.shortcutKeyDescription.isNotEmpty())
            return;
        juce::String s;
        for (auto& kp : it.commandManager->getKeyMappings()
                          ->getKeyPressesAssignedToCommand (it.itemID))
        {
            const auto key = kp.getTextDescriptionWithIcons();
            if (s.isNotEmpty()) s << ", ";
            if (key.length() == 1 && key[0] < 128)
                s << "shortcut: '" << key << '\'';
            else
                s << key;
        }
        it.shortcutKeyDescription = s.trim();
    }

    void paint (juce::Graphics& g) override
    {
        auto& laf = getLookAndFeel();
        laf.drawPopupMenuBackgroundWithOptions (g, getWidth(), getHeight(), opts);
        int y = laf.getPopupMenuBorderSizeWithOptions (opts);
        for (auto& r : rows)
        {
            const juce::Rectangle<int> area (0, y, getWidth(), r.h);
            if (r.item->customComponent == nullptr)
            {
                if (r.item->isSectionHeader)
                    laf.drawPopupMenuSectionHeaderWithOptions (g, area,
                                                               r.item->text, opts);
                else
                    laf.drawPopupMenuItemWithOptions (g, area, false,
                                                      *r.item, opts);
            }
            y += r.h;
        }
    }

    juce::PopupMenu::Options opts;
    std::vector<Row> rows;
};

void saveMenu (juce::PopupMenu menu, const juce::String& name,
               juce::LookAndFeel* laf = nullptr,
               const juce::PopupMenu::Options& opts = {})
{
    MenuCanvas canvas (menu, opts,
                       laf != nullptr ? *laf
                                      : juce::LookAndFeel::getDefaultLookAndFeel());

    // Task 12: per-row dot anchors.  Menu callouts number down the rows by
    // convention, so the generator can rebuild a menu figure's dot map from
    // these - the coordinates track the menu, not a hand measurement.
    if (gDocsMode && gDocs != nullptr)
    {
        juce::Array<juce::var> rowsOut;
        const int border = canvas.getLookAndFeel()
                               .getPopupMenuBorderSizeWithOptions (canvas.opts);
        const float w = (float) canvas.getWidth();
        const float h = (float) canvas.getHeight();
        int y = border;
        for (const auto& r : canvas.rows)
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("text", r.item->text);
            o->setProperty ("sep", r.item->isSeparator);
            o->setProperty ("header", r.item->isSectionHeader);
            juce::Array<juce::var> dot;
            dot.add (100.0 * 11.2 / w);                    // the hand gutter, ~14px native
            dot.add (100.0 * ((float) y + r.h * 0.5f) / h);
            o->setProperty ("dot", dot);
            rowsOut.add (juce::var (o.get()));
            y += r.h;
        }
        juce::DynamicObject::Ptr fig = new juce::DynamicObject();
        fig->setProperty ("menurows", rowsOut);
        gDocs->setProperty (name, juce::var (fig.get()));
    }

    gInMenuSave = true;
    save (canvas, name, 1.25f);
    gInMenuSave = false;
}

// Arms the capture hook, runs the gesture that would have shown a menu, and
// saves what the site built - the site's own code assembles the items, so a
// figure can never drift from the app.
bool captureMenu (const std::function<void()>& trigger, const juce::String& name,
                  juce::LookAndFeel* laf = nullptr)
{
    std::unique_ptr<juce::PopupMenu> got;
    juce::PopupMenu::Options gotOpts;
    gMenuCapture = [&got, &gotOpts] (const juce::PopupMenu& m,
                                     const juce::PopupMenu::Options& o)
    {
        got = std::make_unique<juce::PopupMenu> (m);
        gotOpts = o;
    };
    trigger();
    gMenuCapture = nullptr;
    if (got == nullptr)
    {
        ++gFailed;
        std::cout << "  FAILED " << name << " (menu never built)" << std::endl;
        return false;
    }
    saveMenu (*got, name, laf, gotOpts);
    return true;
}

// ── engine windows: BaySickSynth (6 tabs), BaySickBass, Harmless ──────────
void shootSynthFamily (World& w)
{
    struct TabShot { const char* fig; int tab; };
    static const TabShot kTabs[] = {
        { "BaySickSynth OSC",     0 }, { "BaySickSynth OSC ENV", 1 },
        { "BaySickSynth FLT",     2 }, { "BaySickSynth FLT ENV", 3 },
        { "BaySickSynth LFO",     4 }, { "BaySickSynth MOD",     5 },
    };
    bool any = false;
    for (const auto& t : kTabs) any = any || want (t.fig);
    if (any)
    {
        auto& rig = w.proc->engineRig();
        rig.addTab (TabKind::Layers, 0);
        if (auto* eng = rig.setEngineType (TabKind::Layers, 0, "BaySickSynth"))
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (eng->createEditor());
            auto* synthEd = dynamic_cast<BaySickSynthEditor*> (ed.get());
            WorkspaceWindow win ("shot:synth", "");
            win.setContentNonOwned (ed.get());
            auto* bar = win.getPageMenu();
            bar->setCenterTitle (BaySickSynthEditor::getEngineTitle(),
                                 BaySickSynthEditor::getEngineAccent());
            if (synthEd)
                bar->addExtraRightComponent (synthEd->getTitleStripPresetButton(), 88);
            dressSwingKnob (*bar);
            win.setSize (555, 454);
            for (const auto& t : kTabs)
            {
                if (! want (t.fig)) continue;
                if (synthEd) synthEd->selectTabForShot (t.tab);
                save (win, t.fig, 1.25f);
            }
        }
        else { ++gFailed; std::cout << "  FAILED BaySickSynth engine" << std::endl; }
    }

    if (want ("BaySickBass Titlebar Crop"))
    {
        auto& rig = w.proc->engineRig();
        rig.addTab (TabKind::Bass, 0);
        if (auto* eng = rig.setEngineType (TabKind::Bass, 0, "BaySickBass"))
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (eng->createEditor());
            auto* bassEd = dynamic_cast<BaySickBassEditor*> (ed.get());
            WorkspaceWindow win ("shot:bass", "");
            win.setContentNonOwned (ed.get());
            auto* bar = win.getPageMenu();
            bar->setCenterTitle (BaySickBassEditor::getEngineTitle(),
                                 BaySickBassEditor::getEngineAccent());
            if (bassEd)
                bar->addExtraRightComponent (bassEd->getTitleStripPresetButton(), 88);
            dressSwingKnob (*bar);
            win.setSize (521, 353);
            save (win, "BaySickBass Titlebar Crop", 1.25f);
        }
        else { ++gFailed; std::cout << "  FAILED BaySickBass engine" << std::endl; }
    }
}

void shootHarmless (World& w)
{
    if (! want ("Harmless")) return;
    auto& rig = w.proc->engineRig();
    rig.addTab (TabKind::Layers, 1);
    if (auto* eng = rig.setEngineType (TabKind::Layers, 1, "Harmless"))
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (eng->createEditor());
        auto* harmEd = dynamic_cast<HarmlessEditor*> (ed.get());
        WorkspaceWindow win ("shot:harmless", "");
        win.setContentNonOwned (ed.get());
        auto* bar = win.getPageMenu();
        bar->setCenterTitle (HarmlessEditor::getEngineTitle(),
                             HarmlessEditor::getEngineAccent());
        if (harmEd)
            bar->addExtraRightComponent (harmEd->getTitleStripPresetButton(), 88);
        dressSwingKnob (*bar);
        win.setSize (1045, 453);
        save (win, "Harmless", 1.25f);
    }
    else { ++gFailed; std::cout << "  FAILED Harmless engine" << std::endl; }
}

// ── shell: transport bar + ribbon ─────────────────────────────────────────
void shootTransportBar (World& w)
{
    if (! want ("Transport Bar")) return;
    // The figure is the app's top bar left of the ribbon: the transport
    // controls plus two editor-owned satellites placed at the editor's own
    // coordinates (StandaloneEditor::resized).
    juce::Component host;
    host.setSize (1920, 40);
    GlobalTransportBar bar (*w.playHead);
    host.addAndMakeVisible (bar);
    bar.setBounds (0, 0, 1920, 40);
    juce::TextButton patBtn;
    patBtn.setButtonText ("Pattern 1  " + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")));
    host.addAndMakeVisible (patBtn);
    patBtn.setBounds (528, 6, 176, 28);
    TransportPositionReadout pos;
    pos.setShowTime (true);
    host.addAndMakeVisible (pos);
    pos.setBounds (712, 6, 100, 28);
    save (host, "Transport Bar", 1.25f, { 0, 0, 816, 40 });
}

void shootRibbon (World&)
{
    if (! want ("Ribbon Tab Bar")) return;
    juce::Component host;
    host.setSize (792, 40);
    RibbonTabBar ribbon;
    host.addAndMakeVisible (ribbon);
    ribbon.setBounds (0, 0, 672, 40);           // room for the trailing "+" slot
    ribbon.addTab (RibbonTabBar::TabType::Bass, "Bass 1");
    ribbon.selectTab (2);                       // Effects, like the master
    TransportPerfReadout perf;
    host.addAndMakeVisible (perf);
    perf.setBounds (684, 0, TransportPerfReadout::kWidth, 40);
    perf.update ("SYS 10%", "DSP 3%", "MEM 144  LAT 0", "UND 0  PF 0.0",
                 VC::Green, VC::Green);
    save (host, "Ribbon Tab Bar", 1.25f);
}

// ── dialogs ───────────────────────────────────────────────────────────────
void shootKeybinds (World&)
{
    if (! want ("Keybinds")) return;
    // The editor is the app's command target, but its getCommandInfo is a
    // passthrough of the static catalog - registering the catalog directly
    // shows the same rows with the same default keys.
    juce::ApplicationCommandManager mgr;
    for (const auto& ci : BSCommands::getAllCommands())
    {
        juce::ApplicationCommandInfo info (ci.id);
        info.setInfo (ci.name, ci.tooltip, BSCommands::categoryName (ci.category), 0);
        if (ci.defaultKey.isValid())
            info.addDefaultKeypress (ci.defaultKey.getKeyCode(),
                                     ci.defaultKey.getModifiers());
        mgr.registerCommand (info);
    }
    mgr.getKeyMappings()->resetToDefaultMappings();
    {
        KeyBindsContent content (mgr);
        content.setSize (880, 1027);
        save (content, "Keybinds", 1.0f);
    }   // content dies before mgr - its tabs listen on the key mappings
}

void shootFileSettings (World&)
{
    if (! want ("File Settings")) return;
    auto c = makeFileSettingsComponent();       // self-sizes 400x416
    save (*c, "File Settings", 1.25f);
}

void shootExportDialogs (World& w)
{
    if (want ("Export Audio"))
    {
        BuilderPage builder (*w.proc, *w.patterns);
        {
            auto dlg = makeExportAudioDialog (builder, *w.proc);
            save (*dlg, "Export Audio", 1.25f);
        }   // dialog dies before the BuilderPage it references
    }

    if (want ("Export Project Bundle"))
    {
        // The app assembles this dialog inline from a stock AlertWindow
        // (StandaloneEditor::doExportProjectBundle) - replicated verbatim,
        // never shown, never modal.  Keep the literals matched to source.
        juce::AlertWindow aw ("Export Project Bundle", {},
                              juce::MessageBoxIconType::NoIcon);
        aw.addComboBox ("mode", { "Single .zip file", "Plain folder" }, "Bundle as");
        aw.addComboBox ("scope", { "Project files only (smallest)",
                                   "Include my samples + outside files" }, "Contents");
        aw.addButton ("Export", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        save (aw, "Export Project Bundle", 1.25f);
    }
}

// ── pedals ────────────────────────────────────────────────────────────────
void shootPedals (World&)
{
    const bool wantStd = want ("BaySickPedals");
    const bool wantCmp = want ("BaySickPedals Compact View");
    if (! wantStd && ! wantCmp) return;

    // Self-contained: the pedals processor owns its own APVTS and pre-loads
    // Tuner + Graphic EQ; one High-Gain in slot 1 matches the master.
    BaySickPedalsProcessor pedals (nullptr);
    pedals.loadEffect (1, EffectType::HighGainStyle);

    auto shootView = [&] (bool compact, const juce::String& fig)
    {
        BaySickPedalsEditor ed (pedals);
        WorkspaceWindow win ("shot:pedals", "");
        win.setContentNonOwned (&ed);
        auto* bar = win.getPageMenu();
        bar->setCenterTitle (BaySickPedalsEditor::getEngineTitle(),
                             BaySickPedalsEditor::getEngineAccent());
        bar->setViewMenu ({ "Standard", "Compact" },
                          [compact] { return compact ? 1 : 0; }, [] (int) {});
        bar->addExtraRightComponent (ed.getPedalboardPresetButton(),
                                     compact ? 60 : 88);
        dressSwingKnob (*bar);
        if (compact)
        {
            ed.setViewMode (BaySickPedalsEditor::ViewMode::Compact, false);
            // The compact slot picker is private; drive the one direct-child
            // ComboBox the mode just built, sync so onChange runs pump-free.
            for (auto* child : ed.getChildren())
                if (auto* combo = dynamic_cast<juce::ComboBox*> (child))
                { combo->setSelectedId (2, juce::sendNotificationSync); break; }
        }
        const auto sz = BaySickPedalsEditor::windowSizeFor (
            compact ? BaySickPedalsEditor::ViewMode::Compact
                    : BaySickPedalsEditor::ViewMode::Standard);
        win.setSize (sz.x, sz.y);
        save (win, fig, 1.25f);
    };  // window dies before editor in each call - the bar reparents the preset button

    if (wantStd) shootView (false, "BaySickPedals");
    if (wantCmp) shootView (true,  "BaySickPedals Compact View");
}

// ── Rusty note map ────────────────────────────────────────────────────────
void shootRustyKeys (World&)
{
    if (! want ("Rusty Keys")) return;
    // Bare DocumentWindow, never made visible, so no desktop peer.  A null
    // engine makes the table parse the installed Core Library kit from disk;
    // no Core Library = empty rows and the diff sheet will say so.
    juce::DocumentWindow win ("Rusty Drums Map", VC::Bg,
                              juce::DocumentWindow::closeButton,
                              /*addToDesktop*/ false);
    WindowChrome::applyToDesktopWindow (win);
    win.setContentOwned (new RustyDrumsMapTable (nullptr), false);
    win.setSize (704, 1029);
    save (win, "Rusty Keys", 1.0f);
}

// ── Task 2: effects rack + panels ─────────────────────────────────────────
void shootEffectsCluster (World& w)
{
    const bool wantRack = want ("Effects Rack");
    const bool wantPanel = want ("Effects Panel");
    const bool wantVis = want ("Effects Panel with Visual");
    if (! wantRack && ! wantPanel && ! wantVis) return;

    auto nameMaster = [] (int) { return juce::String ("Master"); };
    auto* rack = EffectsPage::rackForChannelId (w.proc->mVibeGraph, 4);
    if (rack == nullptr)
    { ++gFailed; std::cout << "  FAILED Master rack resolve" << std::endl; return; }

    // The rack figure shows six EMPTY rows - shoot it before anything loads.
    if (wantRack)
    {
        TrackSelectionManager tsm;
        EffectsPage page (tsm, *w.proc);
        WorkspaceWindow win ("shot:fxrack", "Effects");
        win.setContentNonOwned (&page);
        win.getPageMenu()->setMenuBuilder ([] (juce::Component*) {});
        win.setSize (492, 250);
        save (win, "Effects Rack", 1.25f);
    }

    if (wantPanel || wantVis)
    {
        rack->loadEffect (0, EffectType::DeEsser);
        rack->loadEffect (1, EffectType::Reverb);
        // Basic is the panel's default and what the figure documents;
        // the per-effect Advanced views are their own figures.
    }

    if (wantPanel)
    {
        EffectSlotWindow content (*w.proc, 4, rack->getSlotUuid (0), nameMaster,
                                  UndoContext {});
        WorkspaceWindow win ("shot:fxpanel", content.windowTitle());
        win.setContentNonOwned (&content);
        content.configureTitleStrip (*win.getPageMenu());
        win.setSize (686, 263);
        save (win, "Effects Panel", 1.25f);
    }

    if (wantVis)
    {
        // Two windows composed on one host, matching the shipped montage.
        juce::Component host;
        host.setSize (686, 484);
        EffectSlotWindow top (*w.proc, 4, rack->getSlotUuid (1), nameMaster,
                              UndoContext {});
        WorkspaceWindow topWin ("shot:fxv1", top.windowTitle());
        topWin.setContentNonOwned (&top);
        top.configureTitleStrip (*topWin.getPageMenu());
        host.addAndMakeVisible (topWin);
        topWin.setBounds (0, 0, 686, 265);
        EffectVisualWindow vis (*w.proc, 4, rack->getSlotUuid (1), nameMaster);
        WorkspaceWindow visWin ("shot:fxv2", vis.windowTitle());
        visWin.setContentNonOwned (&vis);
        vis.configureTitleStrip (*visWin.getPageMenu());
        host.addAndMakeVisible (visWin);
        visWin.setBounds (0, 268, 686, 216);
        save (host, "Effects Panel with Visual", 1.25f);
    }
}

// ── Task 2: mixer + strip crop ────────────────────────────────────────────
// ── every rack effect, Basic and Advanced (Jeff, 2026-08-28) ──────────────
// In View documents the panel frame once; In Depth documents EVERY effect in
// both views with its own generated control table.  Basic is each panel's
// default; Advanced is the same panel with hasAdvancedControls() honoured.
// Effects with no advanced set render identically - the duplicate is dropped
// after the run and the chapter shows one view.
void shootEffectPanels (World& w)
{
    struct Fx { const char* name; EffectType type; };
    static const Fx kFx[] = {
        { "Compressor",       EffectType::Compressor },
        { "De-esser",         EffectType::DeEsser },
        { "Gate",             EffectType::Gate },
        { "Limiter",          EffectType::Limiter },
        { "Transient Shaper", EffectType::TransientShaper },
        { "Overdrive",        EffectType::Overdrive },
        { "Saturation",       EffectType::Saturation },
        { "Chorus",           EffectType::Chorus },
        { "Flanger",          EffectType::Flanger },
        { "Phaser",           EffectType::Phaser },
        { "De-reverb",        EffectType::DeReverb },
        { "Delay",            EffectType::Delay },
        { "Reverb",           EffectType::Reverb },
    };

    auto nameMaster = [] (int) { return juce::String ("Master"); };
    auto* rack = EffectsPage::rackForChannelId (w.proc->mVibeGraph, 4);
    if (rack == nullptr)
    { ++gFailed; std::cout << "  FAILED effect panels (no rack)" << std::endl; return; }

    for (const auto& fx : kFx)
    {
        const juce::String base = juce::String ("FX Panel ") + fx.name;
        for (int adv = 0; adv < 2; ++adv)
        {
            const juce::String fig = base + (adv ? " Advanced" : " Basic");
            if (! want (fig)) continue;

            rack->loadEffect (3, fx.type);
            rack->setSlotBasicMode (3, adv == 0);

            EffectSlotWindow content (*w.proc, 4, rack->getSlotUuid (3), nameMaster,
                                      UndoContext {});
            WorkspaceWindow win (juce::String ("shot:fxp") + juce::String (adv),
                                 content.windowTitle());
            win.setContentNonOwned (&content);
            content.configureTitleStrip (*win.getPageMenu());
            win.setSize (686, 263);
            save (win, fig, 1.25f);
        }
    }
    rack->loadEffect (3, EffectType::None);
}

void shootMixerCluster (World& w)
{
    const bool wantMix = want ("Mixer");
    const bool wantCrop = want ("Mixer Strip Crop");
    if (! wantMix && ! wantCrop) return;

    auto& rig = w.proc->engineRig();
    rig.addTab (TabKind::Bass, 0);
    rig.setEngineType (TabKind::Bass, 0, "BaySickBass");   // idempotent w/ Task 1

    MixerPage page (*w.proc, *w.patterns);
    page.addBassChannel (0, "Bass 1");
    // The routing graph the cable overlay paints from is otherwise rebuilt
    // only inside processBlock, which never runs headless.
    w.proc->mVibeGraph.rebuildRoutingFromApvts();

    WorkspaceWindow win ("shot:mixer", "Mixer");
    win.setContentNonOwned (&page);
    win.getPageMenu()->setMenuBuilder ([] (juce::Component*) {});
    win.getPageMenu()->setAddMenuBuilder ([] (juce::Component*) {});
    win.setSize (1530, 720);
    if (wantMix) save (win, "Mixer", 1.25f);

    if (wantCrop)
    {
        page.addVoxChannelAtIndex (0);      // self-ensures params + node
        page.addInstChannelAtIndex (0);
        w.proc->mVibeGraph.rebuildRoutingFromApvts();
        win.setSize (1532, 724);
        save (win, "Mixer Strip Crop", 1.25f);
    }
}

void shootVuMeter (World&)
{
    if (! want ("VU Meter")) return;
    struct Host : juce::Component
    {
        VUMeter meter { VUMeter::Vertical };
        Host() { addAndMakeVisible (meter); }
        void paint (juce::Graphics& g) override { g.fillAll (VC::Bg); }
        void resized() override { meter.setBounds (getLocalBounds().reduced (6)); }
    };
    Host host;                              // defaults (-20 / rest) ARE the master
    WorkspaceWindow win ("shot:vu", "VU Meter");
    win.setContentNonOwned (&host);
    win.setSize (180, 200);
    save (win, "VU Meter", 1.25f);
}

// ── Task 2: the pages ─────────────────────────────────────────────────────
void shootBuilder (World& w)
{
    if (! want ("Builder")) return;
    auto& pm = *w.patterns;
    const int pBass = pm.addPattern ("Bass Line");
    const int pLead = pm.addPattern ("Lead");
    pm.setRowName (0, "Drums");
    pm.setRowName (1, "Bass");
    pm.setRowName (2, "Lead");
    auto place = [&] (int row, int pat, double startBeats, int bars)
    {
        ArrangementBlock b;
        b.trackRow = row; b.patternIndex = pat;
        b.startBeats = startBeats; b.lengthBars = bars;
        pm.addBlock (b);
        ++w.blocksAdded;
    };
    place (0, 0, 0.0, 4);
    place (1, pBass, 0.0, 8);
    place (0, 0, 16.0, 4);
    place (2, pLead, 16.0, 4);
    pm.addTimeMarker (10, "Drop");
    pm.addTimeSigChange (11, 4, 4);
    pm.addTempoChange (12, 128.0);
    pm.setCurrentPattern (0);
    pm.notifyContentChanged();

    // One performed transaction so Undo renders enabled, Redo greyed.
    struct NoopAction : juce::UndoableAction
    {
        bool perform() override { return true; }
        bool undo() override { return true; }
    };
    juce::UndoManager um;
    UndoContext ctx;
    ctx.manager = &um;
    ctx.perform = [&um] (juce::UndoableAction* a, const juce::String& label)
    { um.beginNewTransaction (label); return um.perform (a); };
    ctx.undo = [&um] { um.undo(); };
    ctx.redo = [&um] { um.redo(); };
    ctx.perform (new NoopAction(), "Place Pattern Clip");

    BuilderPage page (*w.proc, pm);         // reads the content at ctor
    page.setUndoContext (ctx);
    WorkspaceWindow win ("shot:builder", "Builder");
    win.setContentNonOwned (&page);
    auto* bar = win.getPageMenu();
    bar->setMenuBuilder ([] (juce::Component*) {});
    bar->setExtraHeadings ({ "Edit", "View" }, [] (int, juce::Component*) {});
    win.setSize (1527, 721);
    if (auto* grid = page.getGrid()) grid->setPlayheadBar (11.6);
    save (win, "Builder", 1.25f);
}

void shootPianoRoll (World& w)
{
    if (! want ("Piano Roll")) return;
    auto& pm = *w.patterns;
    auto& roll = pm.getPattern (0).bassRoll[0];
    auto addNote = [&roll] (int midi, double start, double len)
    {
        PianoNote n;
        n.midiNote = midi; n.startBeat = start; n.durationBeats = len;
        n.velocity = 0.8f;
        roll.notes.push_back (n);
    };
    addNote (77, 0.0, 1.0); addNote (79, 1.0, 0.5); addNote (81, 1.5, 0.5);
    addNote (84, 2.0, 1.0); addNote (79, 3.0, 1.0);
    pm.notifyContentChanged();

    PianoRollPage page;
    PianoRollConnection conn;
    conn.dataAccessor = [&pm] { return &pm.currentPattern().bassRoll[0]; };
    conn.patternTimeSigProvider = [] (int& n, int& d) { n = 4; d = 4; };
    conn.noteColor = VC::BassCol[0];
    conn.displayName = "Bass 1";
    conn.engineType = "BaySickBass";
    page.registerEngine ({ EngineKind::Bass, 0 }, conn);
    page.selectEngine ({ EngineKind::Bass, 0 });

    WorkspaceWindow win ("shot:proll", "Piano Roll");
    win.setContentNonOwned (&page);
    win.getPageMenu()->setTabSlots (
        { juce::String ("Bass 1  ") + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")),
          "Player Page", "FX Rack" },
        [] (int) {}, 0, juce::Colours::white);
    win.setSize (1529, 719);
    save (win, "Piano Roll", 1.25f);
}

void shootEventEditor (World& w)
{
    if (! want ("Event Editor")) return;
    auto& pm = *w.patterns;
    ArrangementBlock b;
    b.trackRow = 5;
    b.startBeats = 0.0;
    b.lengthBars = 1;
    b.clipType = ClipType::Automation;
    b.layerTrack = false;
    b.automationLane.paramId = "mixer_master_fader";
    b.automationLane.points.push_back ({ 0.0f, 0.915f });
    b.automationLane.points.push_back ({ 1.0f, 0.915f });
    pm.addBlock (b);
    const int blockIdx = w.blocksAdded;
    ++w.blocksAdded;

    juce::UndoManager um;
    juce::DocumentWindow win ("Event Editor - Mx Master - Fader",
                              juce::Colour (0xff1a1c1e),
                              juce::DocumentWindow::allButtons,
                              /*addToDesktop*/ false);
    WindowChrome::applyToDesktopWindow (win);
    {
        auto content = std::make_unique<EventEditorContent> (*w.proc, um);
        content->getBrowserPane()->onResolveDisplayName =
            [] (const AutomationLane&) { return juce::String ("Mx Master - Fader"); };
        content->setBlock (w.patterns.get(), blockIdx);
        content->setSize (900, 520);
        win.setContentOwned (content.release(), true);
    }
    win.setSize (900, 520);
    save (win, "Event Editor", 1.25f);
    win.clearContentComponent();            // content dies before um
}

void shootUndoHistory (World&)
{
    if (! want ("UndoHistory")) return;
    juce::UndoManager um;
    std::deque<juce::String> labels = { "Add Pattern", "Place Pattern Clip",
                                        "Draw", "Move", "Rename Track", "Resize" };
    int cursor = 4;                         // 4 done, 2 redoable
    UndoHistoryWindow win (um, labels, cursor, [] {}, [] {},
                           /*showOnConstruct*/ false);
    save (win, "UndoHistory", 1.0f);
}

// ── Task 2: players ───────────────────────────────────────────────────────
void shootPlayer (World& w)
{
    if (! want ("BaySickPlayer")) return;
    auto& rig = w.proc->engineRig();
    rig.addTab (TabKind::Layers, 2);
    if (auto* eng = rig.setEngineType (TabKind::Layers, 2, "BaySickPlayer"))
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (eng->createEditor());
        auto* pe = dynamic_cast<BaySickPlayerEditor*> (ed.get());
        WorkspaceWindow win ("shot:player", "");
        win.setContentNonOwned (ed.get());
        auto* bar = win.getPageMenu();
        bar->setCenterTitle (BaySickPlayerEditor::getEngineTitle(),
                             BaySickPlayerEditor::getEngineAccent());
        if (pe)
            bar->addExtraRightComponent (pe->getTitleStripPresetButton(), 88);
        dressSwingKnob (*bar);
        win.setSize (487, 454);
        save (win, "BaySickPlayer", 1.25f);
    }
    else { ++gFailed; std::cout << "  FAILED BaySickPlayer engine" << std::endl; }
}

void shootVoxFamily (World& w)
{
    const bool wantAny = want ("BaySickVocals") || want ("Vocal Chain")
                      || want ("BaySickPitch") || want ("BaySickAlign")
                      || want ("BaySickNAMIR");
    if (! wantAny) return;

    auto& rig = w.proc->engineRig();
    rig.addTab (TabKind::Vox, 0);
    auto* eng = rig.setEngineType (TabKind::Vox, 0, "BaySickVocal");
    auto* bv = dynamic_cast<BaySickVocalProcessor*> (eng);
    if (bv == nullptr)
    { ++gFailed; std::cout << "  FAILED BaySickVocal engine" << std::endl; return; }

    // The pitch panel's analyze runs at host time; with these hooks in and an
    // empty composite it lands the master's exact "no audio clips" failure.
    bv->setOwnChannelId (MixerChannelIds::voxInsert (0));
    bv->onRenderComposite = [] (int, double& beat, juce::int64& smp, double& sr)
        -> juce::AudioBuffer<float> { beat = 0.0; smp = 0; sr = 44100.0; return {}; };
    bv->onChannelClipSignature = [] (int) { return (juce::int64) 0; };

    // Through createEditor, not by value: the editor's panel types are only
    // complete in its own TU, so its destructor must run there too.
    std::unique_ptr<juce::AudioProcessorEditor> edOwn (bv->createEditor());
    auto* edp = dynamic_cast<BaySickVocalEditor*> (edOwn.get());
    if (edp == nullptr)
    { ++gFailed; std::cout << "  FAILED BaySickVocal editor" << std::endl; return; }
    auto& ed = *edp;
    const juce::Colour teal (0xFF0FAFA5);

    WorkspaceWindow wMain ("shot:vox", "");
    wMain.setContentNonOwned (&ed);
    wMain.getPageMenu()->setCenterTitle ("BaySickVocals", teal);
    wMain.setSize (518, 354);

    WorkspaceWindow wChain ("shot:voxchain", "Vox 1 - Vocal Chain");
    wChain.setContentNonOwned (ed.getVocalChainPanel());
    wChain.setSize (515, 724);

    WorkspaceWindow wPitch ("shot:pitch", "");
    wPitch.setContentNonOwned (ed.getPitchPanel());
    wPitch.getPageMenu()->setCenterTitle ("BaySickPitch", teal);
    wPitch.setSize (1530, 721);

    WorkspaceWindow wAlign ("shot:align", "");
    wAlign.setContentNonOwned (ed.getAlignPanel());
    wAlign.getPageMenu()->setCenterTitle ("BaySickAlign", teal);
    wAlign.setSize (1013, 350);

    WorkspaceWindow wNam ("shot:namir", "");
    wNam.setContentNonOwned (ed.getNamIrPanel());
    wNam.getPageMenu()->setCenterTitle (BaySickNAMIREditor::getEngineTitle(),
                                        BaySickNAMIREditor::getEngineAccent());
    wNam.setSize (518, 724);

    // The one timer pump in the harness: the pitch analyze rides a 30 ms
    // callAfterDelay, the vocals readout a 10 Hz tick, the align preset
    // mirror a 500 ms tick.  runDispatchLoopUntil is compiled out in this
    // project (JUCE_MODAL_LOOPS_PERMITTED=0); the TimerThread still counts
    // down, and callPendingTimersSynchronously fires everything due.
    juce::Thread::sleep (550);
    juce::Timer::callPendingTimersSynchronously();

    if (want ("BaySickVocals")) save (wMain, "BaySickVocals", 1.25f);
    if (want ("Vocal Chain"))   save (wChain, "Vocal Chain", 1.25f);
    if (want ("BaySickPitch"))  save (wPitch, "BaySickPitch", 1.25f);
    if (want ("BaySickAlign"))  save (wAlign, "BaySickAlign", 1.25f);
    if (want ("BaySickNAMIR"))  save (wNam, "BaySickNAMIR", 1.25f);
}

// ── Task 2: the sfizz family ──────────────────────────────────────────────
void shootRustyFamily (World& w)
{
    struct TabShot { const char* fig; int tab; };
    static const TabShot kTabs[] = {
        { "BaySickRustyDrums Main", 0 },   { "BaySickRustyDrums Kick", 1 },
        { "BaySickRustyDrums Snare", 2 },  { "BaySickRustyDrums Toms", 3 },
        { "BaySickRustyDrums Hi-Hat", 4 }, { "BaySickRustyDrums Cymbals", 5 },
        { "BaySickRustyDrums Noises and Clicks", 6 },
    };
    bool any = want ("BaySickRustyDrums Drum Kit");
    for (const auto& t : kTabs) any = any || want (t.fig);
    if (! any) return;

    const auto sfz = SampleLibrary::getCoreLibraryDir()
                        .getChildFile ("Big Rusty Drums")
                        .getChildFile ("Programs")
                        .getChildFile ("01-full.sfz");
    BaySickRustyDrumsPage page (*w.proc);
    if (! page.reloadForProjectRestore (sfz))    // synchronous sfizz parse (slow)
    { ++gFailed; std::cout << "  FAILED Rusty kit load" << std::endl; return; }

    WorkspaceWindow win ("shot:rusty", "");
    win.setContentNonOwned (&page);
    auto* bar = win.getPageMenu();
    bar->setCenterTitle ("BaySickRustyDrums", juce::Colour (0xFFCC2222));
    bar->setMidSideVisible (false);
    bar->addExtraRightComponent (page.getPlayerPresetButton(), 76);
    bar->addExtraRightComponent (page.getProgramCombo(), 80);
    dressSwingKnob (*bar);
    win.setSize (519, 351);

    page.switchTab (1);                          // Player (ARIA panel)
    if (auto* panel = page.getAriaPanelForShot())
        for (const auto& t : kTabs)
        {
            if (! want (t.fig)) continue;
            panel->selectTabForShot (t.tab);
            save (win, t.fig, 1.25f);
        }
    if (want ("BaySickRustyDrums Drum Kit"))
    {
        page.switchTab (0);                      // the kit graphic
        save (win, "BaySickRustyDrums Drum Kit", 1.25f);
    }
}

void shootDrumKitGrid (World& w)
{
    if (! want ("Drum Kit")) return;
    DrumKitContainer kit;
    kit.setPatternManager (w.patterns.get());
    WorkspaceWindow win ("shot:dkit", "");
    win.setContentNonOwned (&kit);
    win.getPageMenu()->setTabSlots (
        { juce::String ("Drum Kit  ") + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")),
          "Player Page", "FX Rack" },
        [] (int) {}, 0, juce::Colours::white);
    win.setSize (1531, 722);
    save (win, "Drum Kit", 1.25f);
}

void shootInstKit (World& w, bool guitars)
{
    const char* fig = guitars ? "BaySickGuitars" : "BaySickBasses";
    if (! want (fig)) return;
    const int idx = guitars ? 0 : 1;             // both coexist
    const auto core = SampleLibrary::getCoreLibraryDir();
    const bool ok = guitars
        ? w.proc->loadBaySickGuitarsKit (idx, core.getChildFile ("Black&Green Guitars")
                                                 .getChildFile ("Programs")
                                                 .getChildFile ("01-green_keyswitch.sfz"))
        : w.proc->loadBaySickBassesKit (idx, core.getChildFile ("Black&Blue Basses")
                                                .getChildFile ("Programs")
                                                .getChildFile ("01-darkblack_keysw.sfz"));
    if (! ok)
    { ++gFailed; std::cout << "  FAILED " << fig << " kit load" << std::endl; return; }

    InstPage page (*w.proc, idx);
    page.setSource (guitars ? InstPage::Source::BaySickGuitars
                            : InstPage::Source::BaySickBasses);
    WorkspaceWindow win (juce::String ("shot:inst") + juce::String (idx), "");
    win.setContentNonOwned (&page);
    auto* bar = win.getPageMenu();
    bar->setCenterTitle (fig, juce::Colour (0xFF1C3A8A));
    bar->setMidSideVisible (false);
    bar->addExtraRightComponent (page.getClipFileLabel(), 133);
    bar->addExtraRightComponent (page.getProgramButton(), 87);
    dressSwingKnob (*bar);
    win.setSize (1045, 454);
    save (win, fig, 1.25f);
}

void shootGuitars (World& w) { shootInstKit (w, true); }
void shootBasses  (World& w) { shootInstKit (w, false); }

// ── Task 2: the EQ + instances ────────────────────────────────────────────
void shootEqCluster (World& w)
{
    const bool wantEq = want ("EQ");
    const bool wantInst = want ("EQ Instances");
    const bool wantBandMenu = want ("EQ Band Menu");
    if (! wantEq && ! wantInst && ! wantBandMenu) return;

    auto nameFor = [] (int id)
    {
        return id == 4 ? juce::String ("Master")
             : id == 1 ? juce::String ("Layers Bus")
             : id == 2 ? juce::String ("Bass Bus")
                       : juce::String ("Bus ") + juce::String (id);
    };
    w.proc->ensureStripEqParams (EffectsPage::mixerPrefixForChannelId (4));
    auto* eqPre = EffectsPage::preEqForChannelId (w.proc->mVibeGraph, 4);
    if (eqPre == nullptr)
    { ++gFailed; std::cout << "  FAILED Master pre EQ resolve" << std::endl; return; }

    auto band = [] (kbs::EqType t, float f, float g, float q)
    {
        kbs::EqBandParams p;
        p.on = true; p.type = t; p.freqHz = f; p.gainDb = g; p.q = q;
        return p;
    };
    eqPre->pushBand (0, band (kbs::EqType::bell, 95.0f, 3.5f, 1.0f));
    auto dyn = band (kbs::EqType::bell, 950.0f, 0.0f, 1.4f);
    dyn.dynamic = true; dyn.thresholdDb = -30.0f; dyn.rangeDb = -12.0f; dyn.ratio = 4.0f;
    eqPre->pushBand (1, dyn);
    auto hp = band (kbs::EqType::highPass, 45.0f, 0.0f, 0.7071f);
    hp.slope = 36.0f;
    eqPre->pushBand (2, hp);
    auto sideB = band (kbs::EqType::bell, 3200.0f, 2.5f, 2.0f);
    sideB.channel = kbs::EqChannel::side;
    eqPre->pushBand (3, sideB);
    auto leftB = band (kbs::EqType::bell, 7500.0f, -2.0f, 1.2f);
    leftB.channel = kbs::EqChannel::left;
    eqPre->pushBand (4, leftB);
    eqPre->pushBand (5, band (kbs::EqType::highShelf, 11000.0f, 2.0f, 0.7071f));

    EffectEqWindow content (*w.proc, 4, true, nameFor);
    WorkspaceWindow win ("shot:eq", content.windowTitle());
    win.setContentNonOwned (&content);
    content.configureTitleStrip (*win.getPageMenu());
    win.setSize (1046, 451);

    auto* graph = content.graphForShot();
    juce::AudioBuffer<float> buf (2, 512);
    juce::Random rng (42);
    auto processNoise = [&] (StripEq& eq, int blocks)
    {
        for (int bl = 0; bl < blocks; ++bl)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < 512; ++i)
                    d[i] = (rng.nextFloat() * 2.0f - 1.0f) * 0.30f;
            }
            eq.process (buf);
            if (&eq == eqPre && graph != nullptr && bl > 30 && (bl % 4) == 0)
                graph->pollNow();
        }
    };
    processNoise (*eqPre, 48);                  // analyser + live GR content
    if (graph != nullptr) { graph->selectBand (1); graph->pollNow(); }
    if (auto* rail = content.railForShot()) { rail->pollNow(); rail->pollNow(); }
    if (wantEq) save (win, "EQ", 1.25f);
    if (wantBandMenu && graph != nullptr)
        captureMenu ([&] { graph->showBandMenu (1); }, "EQ Band Menu");

    if (wantInst)
    {
        for (int chId : { 1, 2 })
        {
            w.proc->ensureStripEqParams (EffectsPage::mixerPrefixForChannelId (chId));
            EffectRack* r = nullptr;
            StripEq* post = nullptr;
            EffectsPage::resolveChannelDsp (w.proc->mVibeGraph, chId, r, post);
            if (post == nullptr) continue;
            post->pushBand (0, band (kbs::EqType::bell,
                                     chId == 1 ? 400.0f : 2000.0f,
                                     chId == 1 ? 4.0f : -5.0f, 1.0f));
            processNoise (*post, 40);
        }
        eqview::EqInstanceBrowser browser;
        browser.getChannels = []
        {
            return std::vector<std::pair<int, juce::String>> {
                { 4, "Master" }, { 1, "Layers Bus" }, { 2, "Bass Bus" } };
        };
        browser.resolvePoint = [&w] (int id, bool pre) -> StripEq*
        {
            if (pre) return EffectsPage::preEqForChannelId (w.proc->mVibeGraph, id);
            EffectRack* r = nullptr;
            StripEq* e = nullptr;
            EffectsPage::resolveChannelDsp (w.proc->mVibeGraph, id, r, e);
            return e;
        };
        browser.isCurrent = [] (int id, bool pre) { return id == 4 && pre; };
        browser.isMatchRef = [] (int, bool) { return false; };
        browser.isCollisionRef = [] (int, bool) { return false; };
        browser.sampleRate = [] { return 48000.0; };
        content.addAndMakeVisible (browser);
        if (graph != nullptr)
            browser.setBounds (graph->getX() + 10, graph->getY() + 10, 320,
                               juce::jmin (360, juce::jmax (120, graph->getHeight() - 20)));
        browser.pollForShot();
        browser.pollForShot();
        save (win, "EQ Instances", 1.25f);
        content.removeChildComponent (&browser);
    }
}

// ── Task 2: analyzer + plugin manager + audio settings ────────────────────
void shootAnalyzer (World& w)
{
    if (! want ("Analyzer")) return;
    MasterAnalyzerView view (*w.proc);          // idle Loudness view IS the master
    WorkspaceWindow win ("shot:analyzer", "Master Analyzer");
    win.setContentNonOwned (&view);
    win.setSize (1046, 455);
    save (win, "Analyzer", 1.25f);
}

void shootPluginSearch (World& w)
{
    if (! want ("Plugin Search")) return;
    juce::DocumentWindow win ("Plugins", VC::Bg,
                              juce::DocumentWindow::closeButton,
                              /*addToDesktop*/ false);
    WindowChrome::applyToDesktopWindow (win);
    win.setContentOwned (new PluginsManagerContent (w.proc->pluginManager()), false);
    win.setSize (920, 700);
    save (win, "Plugin Search", 1.25f);
}

void shootAudioSettings (World& w)
{
    if (! want ("Audio & Midi Settings")) return;
    auto c = makeAudioSettingsComponent (*w.deviceMgr);   // self-sizes
    save (*c, "Audio & Midi Settings", 1.25f);
}


// ── menu figures (Task 3) ─────────────────────────────────────────────────
// Every figure below runs the app's own builder - through a public build
// method where one exists, else through the site's real click path with the
// capture hook armed - so the imaged items can never drift from the app.

void shootPianoRollMenus (World& w)
{
    static const struct { const char* fig; int idx; const char* name; } kMenus[] = {
        { "Piano Roll Edit",   0, "Edit"   }, { "Piano Roll Tools", 1, "Tools" },
        { "Piano Roll Scale",  2, "Scale"  }, { "Piano Roll Chords", 3, "Chords" },
        { "Piano Roll View",   4, "View"   },
    };
    bool any = false;
    for (const auto& m : kMenus) any = any || want (m.fig);
    if (! any) return;

    auto& pm = *w.patterns;
    PianoRollPage page;
    PianoRollConnection conn;
    conn.dataAccessor = [&pm] { return &pm.currentPattern().bassRoll[0]; };
    conn.patternTimeSigProvider = [] (int& n, int& d) { n = 4; d = 4; };
    conn.noteColor = VC::BassCol[0];
    conn.displayName = "Bass 1";
    conn.engineType = "BaySickBass";
    page.registerEngine ({ EngineKind::Bass, 0 }, conn);
    page.selectEngine ({ EngineKind::Bass, 0 });

    auto* roll = page.getActivePianoRoll();
    if (roll == nullptr)
    { ++gFailed; std::cout << "  FAILED Piano Roll menus (no roll)" << std::endl; return; }

    PianoRollMenuBar model (*roll);
    for (const auto& m : kMenus)
        if (want (m.fig))
            saveMenu (model.getMenuForIndex (m.idx, m.name), m.fig);
}

void shootDrumKitMenus (World& w)
{
    static const struct { const char* fig; int idx; const char* name; } kMenus[] = {
        { "Drum Kit Edit", 0, "Edit" }, { "Drum Kit Tools", 1, "Tools" },
        { "Drum Kit View", 2, "View" },
    };
    bool any = false;
    for (const auto& m : kMenus) any = any || want (m.fig);
    if (! any) return;

    DrumKitContainer kit;
    kit.setPatternManager (w.patterns.get());
    DrumKitMenuBar model (kit);
    for (const auto& m : kMenus)
        if (want (m.fig))
            saveMenu (model.getMenuForIndex (m.idx, m.name), m.fig);
}

void shootEffectPickerMenu (World&)
{
    if (! want ("Effects Panel List")) return;
    captureMenu ([]
    {
        SlotComponent::showEffectPickerMenu ({ 0, 0 }, [] (EffectType) {},
                                             [] (const juce::PluginDescription&) {});
    }, "Effects Panel List");
}

void shootPedalsMenus (World&)
{
    const bool wantList = want ("BaySickPedals List");
    const bool wantView = want ("BaySickPedals View");
    if (! wantList && ! wantView) return;

    if (wantList)
    {
        BaySickPedalsProcessor pedals (nullptr);
        BaySickPedalsEditor ed (pedals);
        // Slot 2 is empty: the master shows the picker with Clear greyed.
        captureMenu ([&] { ed.showSwapMenuForShot (2); }, "BaySickPedals List");
    }

    if (wantView)
    {
        PageMenuBar bar;
        bar.setViewMenu ({ "Standard", "Compact" }, [] { return 0; }, [] (int) {});
        captureMenu ([&] { bar.triggerExtraHeadingForShot (0); },
                     "BaySickPedals View");
    }
}

void shootRecordMenu (World& w)
{
    if (! want ("Recording Menu")) return;
    GlobalTransportBar bar (*w.playHead);
    captureMenu ([&] { bar.showRecordMenuForShot(); }, "Recording Menu");
}


void shootBuilderMenus (World& w)
{
    static const char* const kFigs[] = { "Builder Menu", "Builder Edit", "Builder View" };
    bool any = false;
    for (auto* f : kFigs) any = any || want (f);
    if (! any) return;

    struct NoopAction : juce::UndoableAction
    {
        bool perform() override { return true; }
        bool undo() override { return true; }
    };
    juce::UndoManager um;
    UndoContext ctx;
    ctx.manager = &um;
    ctx.perform = [&um] (juce::UndoableAction* a, const juce::String& label)
    { um.beginNewTransaction (label); return um.perform (a); };
    ctx.undo = [&um] { um.undo(); };
    ctx.redo = [&um] { um.redo(); };

    BuilderPage page (*w.proc, *w.patterns);
    page.setUndoContext (ctx);
    ctx.perform (new NoopAction(), "Place Pattern Clip");

    if (want ("Builder Menu"))
    { juce::PopupMenu m; page.buildClipsMenu (m); saveMenu (m, "Builder Menu"); }
    if (want ("Builder Edit"))
    { juce::PopupMenu m; page.buildEditMenu (m); saveMenu (m, "Builder Edit"); }
    if (want ("Builder View"))
    { juce::PopupMenu m; page.buildViewMenu (m); saveMenu (m, "Builder View"); }
}

void shootMixerMenus (World& w)
{
    const bool wantTitle = want ("Mixer Menu");
    const bool wantAdd   = want ("Mixer Add");
    const bool wantSend  = want ("Send Menu");
    if (! wantTitle && ! wantAdd && ! wantSend) return;

    if (wantTitle)
        saveMenu (shots::buildMixerTitleMenu (*w.proc, *w.deviceMgr), "Mixer Menu");

    if (wantAdd || wantSend)
    {
        MixerPage page (*w.proc, *w.patterns);
        page.addBassChannel (0, "Bass 1");
        w.proc->mVibeGraph.rebuildRoutingFromApvts();
        if (wantAdd) saveMenu (page.buildAddMenu(), "Mixer Add");
        if (wantSend)
            captureMenu ([&] { page.showSendMenuForShot (MixerChannelIds::bassInsert (0)); },
                         "Send Menu");
    }
}

void shootEffectsRackMenu (World& w)
{
    if (! want ("Effects Rack Menu")) return;
    TrackSelectionManager tsm;
    EffectsPage page (tsm, *w.proc);
    juce::PopupMenu m;
    page.buildTitleMenu (m);
    saveMenu (m, "Effects Rack Menu");
}

void shootEffectPanelMenu (World& w)
{
    if (! want ("Effects Panel Menu")) return;
    auto nameMaster = [] (int) { return juce::String ("Master"); };
    auto* rack = EffectsPage::rackForChannelId (w.proc->mVibeGraph, 4);
    if (rack == nullptr)
    { ++gFailed; std::cout << "  FAILED Effects Panel Menu (no rack)" << std::endl; return; }

    rack->loadEffect (2, EffectType::Compressor);   // basic mode by default
    EffectSlotWindow content (*w.proc, 4, rack->getSlotUuid (2), nameMaster,
                              UndoContext {});
    WorkspaceWindow win ("shot:fxmenu", content.windowTitle());
    win.setContentNonOwned (&content);
    content.configureTitleStrip (*win.getPageMenu());
    win.setSize (686, 263);

    std::unique_ptr<juce::PopupMenu> got;
    juce::PopupMenu::Options gotOpts;
    gMenuCapture = [&got, &gotOpts] (const juce::PopupMenu& m,
                                     const juce::PopupMenu::Options& o)
    { got = std::make_unique<juce::PopupMenu> (m); gotOpts = o; };
    win.getPageMenu()->triggerMenuForShot();
    gMenuCapture = nullptr;
    if (got == nullptr)
    { ++gFailed; std::cout << "  FAILED Effects Panel Menu (no capture)" << std::endl; return; }

    // The master shows the strip's Menu heading with the popup dropped under
    // it, so the figure composes the window (clipped to the menu's width)
    // over the rendered menu.
    MenuCanvas canvas (*got, gotOpts, juce::LookAndFeel::getDefaultLookAndFeel());
    const int barH = win.getPageMenu()->getBottom();
    juce::Component host;
    host.setSize (canvas.getWidth(), barH + canvas.getHeight());
    host.addAndMakeVisible (win);
    win.setTopLeftPosition (0, 0);
    host.addAndMakeVisible (canvas);
    canvas.setTopLeftPosition (0, barH);

    if (gDocsMode && gDocs != nullptr)
    {
        juce::Array<juce::var> rowsOut;
        const int border = canvas.getLookAndFeel()
                               .getPopupMenuBorderSizeWithOptions (canvas.opts);
        const float w = (float) host.getWidth();
        const float h = (float) host.getHeight();
        int y = barH + border;
        for (const auto& r : canvas.rows)
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("text", r.item->text);
            o->setProperty ("sep", r.item->isSeparator);
            o->setProperty ("header", r.item->isSectionHeader);
            juce::Array<juce::var> dot;
            dot.add (100.0 * 11.2 / w);
            dot.add (100.0 * ((float) y + r.h * 0.5f) / h);
            o->setProperty ("dot", dot);
            rowsOut.add (juce::var (o.get()));
            y += r.h;
        }
        juce::DynamicObject::Ptr fig = new juce::DynamicObject();
        fig->setProperty ("menurows", rowsOut);
        gDocs->setProperty ("Effects Panel Menu", juce::var (fig.get()));
    }

    gInMenuSave = true;
    save (host, "Effects Panel Menu", 1.25f);
    gInMenuSave = false;
}

void shootEngineMenus (World& w)
{
    auto dressPlayerBar = [] (PageMenuBar& bar)
    {
        bar.setFxRackSlot ([] {});
        bar.setFreezeSlot ([] { return 0; }, [] (bool) {});
    };

    if (want ("BaySickSynth-BaySickPlayer-Harmless Menu"))
    {
        LayersPage page (*w.proc, *w.patterns, 3);
        page.selectEngine ("BaySickSynth");
        PageMenuBar bar;
        dressPlayerBar (bar);
        page.onBuildWindowNavMenu = [&page, &bar] (juce::PopupMenu& m)
        {
            m.addItem ("Player", true, page.getActiveTab() == 0, [] {});
            m.addItem ("Piano Roll", [] {});
            bar.appendStandardItems (m);
        };
        captureMenu ([&] { page.showPageActionsMenu (nullptr); },
                     "BaySickSynth-BaySickPlayer-Harmless Menu");
    }

    if (want ("BaySickBass Menu Updated"))
    {
        BassPage page (*w.proc, *w.patterns, 1);
        page.selectEngine ("BaySickBass");
        PageMenuBar bar;
        dressPlayerBar (bar);
        page.onBuildWindowNavMenu = [&page, &bar] (juce::PopupMenu& m)
        {
            m.addItem ("Player", true, page.getActiveTab() == 0, [] {});
            m.addItem ("Piano Roll", [] {});
            bar.appendStandardItems (m);
        };
        captureMenu ([&] { page.showPageActionsMenu (nullptr); },
                     "BaySickBass Menu Updated");
    }

    if (want ("BaySickVocals Menu"))
    {
        VoxPage page (*w.proc, 1);          // ctor creates its own engine
        PageMenuBar bar;
        dressPlayerBar (bar);
        page.onBuildWindowNavMenu = [&bar] (juce::PopupMenu& m)
        {
            m.addItem ("Vocal Chain",  [] {});
            m.addItem ("BaySickPitch", [] {});
            m.addItem ("BaySickAlign", [] {});
            m.addItem ("NAM/IR",       [] {});
            bar.appendStandardItems (m);
        };
        captureMenu ([&] { page.showPageActionsMenu (nullptr); },
                     "BaySickVocals Menu");
    }

    if (want ("BaySickDrum Menu"))
    {
        DrumPage page (*w.proc, *w.patterns, 1);
        page.selectEngine ("BaySickPlayer");
        PageMenuBar bar;
        dressPlayerBar (bar);
        page.onBuildWindowNavMenu = [&page, &bar] (juce::PopupMenu& m)
        {
            m.addItem ("Drum Kit", [] {});
            m.addItem ("Player", true, page.getActiveTab() == 1, [] {});
            m.addItem ("Piano Roll", [] {});
            bar.appendStandardItems (m);
        };
        juce::Component dummyAnchor;   // showContextMenu null-guards its anchor
        // fromKit: the master documents the kit pad right-click shape (per-drum
        // MIDI Note / MIDI Learn rows), not the window Menu dropdown.
        captureMenu ([&] { page.showContextMenu (&dummyAnchor, true); },
                     "BaySickDrum Menu");
    }

    if (want ("BaySickPlugins Menu"))
    {
        PluginsPage page (*w.proc, 1);
        PageMenuBar bar;
        dressPlayerBar (bar);
        page.onBuildWindowNavMenu = [&bar] (juce::PopupMenu& m)
        {
            m.addItem ("Piano Roll", [] {});
            bar.appendStandardItems (m);
        };
        captureMenu ([&] { page.showPageActionsMenu (nullptr); },
                     "BaySickPlugins Menu");
    }

    if (want ("BaySickGuitars & Basses Menu"))
    {
        InstPage page (*w.proc, 0);         // guitars kit rides shootGuitars
        page.setSource (InstPage::Source::BaySickGuitars);
        PageMenuBar bar;
        dressPlayerBar (bar);
        page.onBuildWindowNavMenu = [&page, &bar] (juce::PopupMenu& m)
        {
            m.addItem ("Pedals", [] {});
            m.addItem ("NAM/IR", [] {});
            if (page.getSource() != InstPage::Source::LiveInput)
                m.addItem ("Piano Roll", [] {});
            bar.appendStandardItems (m);
        };
        captureMenu ([&] { page.showPageActionsMenu (nullptr); },
                     "BaySickGuitars & Basses Menu");
    }

    if (want ("BaySickPedals Menu"))
    {
        // The pedals aux window frames the SAME InstPage of a LIVE-INPUT tab;
        // its bar carries no FX/freeze slots, so the standard tail is absent.
        InstPage page (*w.proc, 2);
        page.onBuildWindowNavMenu = [] (juce::PopupMenu& m)
        {
            m.addItem ("Pedals", [] {});
            m.addItem ("NAM/IR", [] {});
        };
        captureMenu ([&] { page.showPageActionsMenu (nullptr); },
                     "BaySickPedals Menu");
    }

    if (want ("BaySickRustyDrums Menu"))
    {
        // Replicated from the editor-resident inline builder (its items need
        // the live editor for their actions; the harness needs only the rows).
        // Rides after "rusty family" so the kit-loaded gate reads true.
        juce::PopupMenu m;
        m.addItem ("Drum Kit", true, true, [] {});
        m.addItem ("Player", true, false, [] {});
        m.addItem ("Piano Roll", [] {});
        m.addItem ("Rusty Drums Map...", [] {});
        PageMenuBar bar;
        bar.setFreezeSlot ([] { return 0; }, [] (bool) {});
        bar.appendStandardItems (m);
        m.addSeparator();
        m.addItem (100, "Save Page Preset As...", w.proc->hasBaySickRustyDrums());
        juce::PopupMenu loadSub;
        int nPresets = 0;
        const auto root = PagePresetIO::myPresetsDirForPageKind (
            PagePresetIO::PageKind::RustyDrums);
        if (root.isDirectory())
        {
            juce::Array<juce::File> files;
            root.findChildFiles (files, juce::File::findFiles, false, "*.xml");
            files.sort();
            for (auto& f : files)
                loadSub.addItem (1000 + nPresets++, f.getFileNameWithoutExtension());
        }
        if (nPresets == 0)
            loadSub.addItem (-1, "(no page presets saved)", false, false);
        m.addSubMenu ("Load Page Preset", loadSub);
        saveMenu (m, "BaySickRustyDrums Menu");
    }
}

void shootRibbonAddMenu (World&)
{
    if (! want ("Ribbon + Menu")) return;
    RibbonTabBar ribbon;
    saveMenu (ribbon.buildAddMenu(), "Ribbon + Menu");
}

void shootMetronomeMenu (World&)
{
    if (! want ("Metronome Menu")) return;
    juce::Component host;
    host.setSize (400, 260);
    MetroPanel panel;
    // Parented CallOutBox: no desktop, real frame + arrow, pointing up at the
    // transport's metro chevron like the app.
    juce::CallOutBox box (panel, { 190, 4, 1, 1 }, &host);
    save (box, "Metronome Menu", 1.25f);
}

void shootAnalyzerMenu (World& w)
{
    if (! want ("Analyzer Menu")) return;
    MasterAnalyzerView view (*w.proc);
    saveMenu (shots::buildAnalyzerMenu (view, nullptr), "Analyzer Menu");
}

void shootEditorMenus (World& w)
{
    static const struct { const char* fig; int idx; const char* name; } kMenus[] = {
        { "File Menu", 0, "File" },        { "Edit Menu", 1, "Edit" },
        { "Pattern Menu", 2, "Patterns" }, { "View Menu", 3, "View" },
        { "Options Menu", 4, "Options" },  { "Help Menu", 5, "Help" },
    };
    bool any = want ("RightClick Knob or Slider");
    for (const auto& m : kMenus) any = any || want (m.fig);
    if (! any) return;

    // The one figure group that builds the WHOLE editor: getMenuForIndex is a
    // member, and its state (recents, templates, the ribbon's New Tab
    // submenu, pattern count) is the editor's own.  MUST run last in the
    // registry - the ctor re-points the processor's PatternManager at the
    // editor's own and installs process-wide static hooks.
    StandaloneEditor ed (*w.proc, *w.playHead, *w.deviceMgr);
    for (const auto& m : kMenus)
        if (want (m.fig))
            saveMenu (ed.getMenuForIndex (m.idx, m.name), m.fig);

    if (want ("RightClick Knob or Slider"))
        saveMenu (GlobalAutoRightClick::buildControlMenu ("mixer_master_fader", true),
                  "RightClick Knob or Slider");
}

using ShootFn = void (*) (World&);
struct Figure { const char* group; ShootFn fn; };
const Figure kFigures[] = {
    { "synth family",  &shootSynthFamily },
    { "harmless",      &shootHarmless },
    { "transport",     &shootTransportBar },
    { "ribbon",        &shootRibbon },
    { "keybinds",      &shootKeybinds },
    { "file settings", &shootFileSettings },
    { "export",        &shootExportDialogs },
    { "pedals",        &shootPedals },
    { "rusty keys",    &shootRustyKeys },
    // Task 2 - state-rich figures.  Order carries state: the rack figure
    // shoots the Master rack EMPTY before the panel figures load it; the
    // mixer reuses Task 1's Bass engine; Builder seeds the pattern content
    // Piano Roll and the Event Editor read.
    { "effects",       &shootEffectsCluster },
    { "fx panels",     &shootEffectPanels },
    { "mixer",         &shootMixerCluster },
    { "vu meter",      &shootVuMeter },
    { "builder",       &shootBuilder },
    { "piano roll",    &shootPianoRoll },
    { "event editor",  &shootEventEditor },
    { "undo history",  &shootUndoHistory },
    { "player",        &shootPlayer },
    { "vox family",    &shootVoxFamily },
    { "rusty family",  &shootRustyFamily },
    { "drum kit grid", &shootDrumKitGrid },
    { "guitars",       &shootGuitars },
    { "basses",        &shootBasses },
    { "eq",            &shootEqCluster },
    { "analyzer",      &shootAnalyzer },
    { "plugin search", &shootPluginSearch },
    { "audio settings", &shootAudioSettings },
    // Task 3 - menu figures.  Engine menus ride earlier figures' state
    // (guitars kit, rusty kit); the editor group is LAST by contract.
    { "roll menus",    &shootPianoRollMenus },
    { "kit menus",     &shootDrumKitMenus },
    { "fx picker",     &shootEffectPickerMenu },
    { "pedals menus",  &shootPedalsMenus },
    { "record menu",   &shootRecordMenu },
    { "builder menus", &shootBuilderMenus },
    { "mixer menus",   &shootMixerMenus },
    { "fx rack menu",  &shootEffectsRackMenu },
    { "fx panel menu", &shootEffectPanelMenu },
    { "engine menus",  &shootEngineMenus },
    { "ribbon menu",   &shootRibbonAddMenu },
    { "metronome",     &shootMetronomeMenu },
    { "analyzer menu", &shootAnalyzerMenu },
    { "editor menus",  &shootEditorMenus },
};

} // namespace

int run (const juce::String& commandLine)
{
    // Quoted figure names survive tokenizing; strip the quotes after.
    juce::StringArray tokens;
    tokens.addTokens (commandLine, true);
    gOutDir = AppPaths::appRoot().getChildFile ("Manuals")
                                 .getChildFile ("shots-staging");
    for (const auto& raw : tokens)
    {
        const auto t = raw.unquoted();
        if (t == "--shot" || t.isEmpty()) continue;
        if (t.startsWith ("--out="))        gOutDir = juce::File (t.fromFirstOccurrenceOf ("=", false, false));
        else if (t.startsWith ("--scale=")) gScaleOverride = t.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        else if (t == "--docs")             gDocsMode = true;
        else if (t.startsWith ("--docs="))
        {
            gDocsMode = true;
            gDocsFile = juce::File (t.fromFirstOccurrenceOf ("=", false, false));
        }
        else                                gWanted.add (t);
    }
    gOutDir.createDirectory();
    std::cout << "bsd_shot -> " << gOutDir.getFullPathName() << std::endl;

    juce::LookAndFeel::setDefaultLookAndFeel (&BaySickLAF::get());

    // An embedded font can link perfectly and still draw nothing, and a
    // missing glyph reads as a layout bug in every shot that follows.
    {
        juce::SharedResourcePointer<fontaudio::IconHelper> icons;
        if (juce::GlyphArrangement::getStringWidth (icons->getFont (24.0f),
                                                    fontaudio::FilterBell) <= 0.0f)
        {
            std::cout << "FONT FAILED - fontaudio glyphs have no width" << std::endl;
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
            return 3;
        }
    }

    if (gDocsMode)
    {
        gDocs = new juce::DynamicObject();
        if (gDocsFile == juce::File())
            gDocsFile = AppPaths::appRoot().getChildFile ("Manuals")
                                           .getChildFile ("assets")
                                           .getChildFile ("bsd-docs.json");
    }

    {
        World w;
        w.proc     = std::make_unique<BaySickDAWProcessor>();
        w.playHead = std::make_unique<StandalonePlayHead>();
        w.proc->setPlayHead (w.playHead.get());
        w.proc->setSeekDiscontinuityFlag (w.playHead->getSeekDiscontinuityFlag());
        w.proc->setLoopWrappedFlag (w.playHead->getLoopWrappedFlag());
        w.playHead->advanceBlock (0, 48000.0);
        w.patterns = std::make_unique<PatternManager>();
        w.proc->setPatternManager (w.patterns.get());
        w.deviceMgr = std::make_unique<juce::AudioDeviceManager>();   // never initialise()d
        w.proc->setPlayConfigDetails (2, 2, 48000.0, 512);
        w.proc->prepareToPlay (48000.0, 512);
        // Headless there is no audio thread to acknowledge a settle, and a
        // prepared flag makes every engine create/teardown timeout-spin.
        // releaseResources only clears that flag - topology, bus nodes and
        // ensured params all persist.
        w.proc->releaseResources();

        gDocsProc = w.proc.get();
        for (const auto& f : kFigures)
            f.fn (w);
        gDocsProc = nullptr;
    }

    if (gDocsMode && gDocs != nullptr)
    {
        gDocsFile.deleteFile();
        juce::FileOutputStream out (gDocsFile);
        if (out.openedOk())
        {
            juce::JSON::writeToStream (out, juce::var (gDocs.get()));
            std::cout << "docs -> " << gDocsFile.getFullPathName() << std::endl;
        }
        else
        {
            ++gFailed;
            std::cout << "  FAILED docs write" << std::endl;
        }
        gDocs = nullptr;
    }

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    std::cout << gWritten << " written, " << gFailed << " failed" << std::endl;
    return gFailed > 0 ? 1 : 0;
}

} // namespace shots
