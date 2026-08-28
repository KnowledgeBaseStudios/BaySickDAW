// BaySickDAW - the manual screenshot harness (QA-ManualPress M-1).
// See ShotHarness.h for the contract.  Structure mirrors kbs_shot: one boot,
// one save path, a registry of figure functions.  Headless rules that shape
// everything here: timers never fire (any polled state is set by direct
// calls), no component ever reaches the desktop (WorkspaceWindow only grows
// a native peer in attachTo, which is never called), and the default
// LookAndFeel must be installed by hand or every widget renders stock JUCE.
#include "ShotHarness.h"
#include "ShotFactories.h"

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
namespace
{

// ── the world every figure shoots against ─────────────────────────────────
struct World
{
    std::unique_ptr<BaySickDAWProcessor> proc;
    std::unique_ptr<StandalonePlayHead>  playHead;
    std::unique_ptr<PatternManager>      patterns;
};

juce::File     gOutDir;
juce::StringArray gWanted;
float          gScaleOverride = 0.0f;   // 0 = per-figure default
int            gWritten = 0, gFailed = 0;

bool want (const juce::String& name)
{
    return gWanted.isEmpty() || gWanted.contains (name, true);
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
                              juce::DocumentWindow::closeButton);
    WindowChrome::applyToDesktopWindow (win);
    win.setContentOwned (new RustyDrumsMapTable (nullptr), false);
    win.setSize (704, 1029);
    save (win, "Rusty Keys", 1.0f);
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
        w.proc->setPlayConfigDetails (2, 2, 48000.0, 512);
        w.proc->prepareToPlay (48000.0, 512);
        // Headless there is no audio thread to acknowledge a settle, and a
        // prepared flag makes every engine create/teardown timeout-spin.
        // releaseResources only clears that flag - topology, bus nodes and
        // ensured params all persist.
        w.proc->releaseResources();

        for (const auto& f : kFigures)
            f.fn (w);
    }

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    std::cout << gWritten << " written, " << gFailed << " failed" << std::endl;
    return gFailed > 0 ? 1 : 0;
}

} // namespace shots
