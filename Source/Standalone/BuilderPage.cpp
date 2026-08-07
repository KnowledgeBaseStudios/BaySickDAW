#include "BuilderPage.h"
#include "../AppPaths.h"         // TS7: freeze_timing.txt lands beside the app
#include "../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"   // TS7 §6.9 kit freeze
#include "TypingKeyboardMap.h"   // D-4: bypass tool keys while typing-keyboard mode is on
#include "../DSP/BpmDetect.h"    // QA-Ec G1-boundary: content tempo estimation (import + display)
#include "../DSP/DSPBase.h"      // QA-Fe2: isTransportPlaying stop-gate (Regenerate De-noise)
#include "PatternColorPicker.h"
#include "../SampleLibrary.h"     // doImportAudio start dir (My Samples)
#include <map>                    // G1 boundary: detected-tempo display cache
#include "../ClipDropDiag.h"        // QA-ClipDrop: diagnostic trap (2026-06-02)
#include "../G3PlayheadDiag.h"      // [G3 PLAYHEAD] G-9 reading (QA-G3Smoke Task 1); Debug-only
#include "../DSP/Mp3Writer.h"       // QA-Export: MP3 encoder front end
#include "../TempoMapRead.h"        // QA-Export: offline head reads the live tempo timeline
#include "../EngineRig.h"           // QA-ModelShell TS2: offline lane replay resolves engines model-side
#include "../DSP/EffectParamMap.h"  // QA-ModelShell TS2: rack-lane resolution (type, variant)
#include "../Hosting/HostedPluginEffect.h"  // QA-ModelShell TS6: hosted plugin lane branch
#include "../DSP/LufsMeterDSP.h"    // QA-ModelShell TS2: CL-227 backend / CL-045 measure pass
#include "../DSP/TruePeakMeter.h"   // QA-ModelShell TS7 BLU-108: real true peak, not the Lagrange estimate
#include "EffectsPage.h"            // QA-ModelShell TS2: channelPrefixForId / rackForChannelId statics
#include "../BaySickPedals/BaySickPedalsProcessor.h"   // QA-ModelShell TS3: offline pedal-board lanes
#include "../Harmless/HarmlessProcessor.h"             // BLU-344: offline mod-editor lanes
#include "../BaySickVocal/BaySickVocalProcessor.h"  // QA-ModelShell TS2: vox lane -> vocal + embedded NAM/IR

// Smoke #45: minimal user32 import for the right-Alt check -- deliberately
// NOT <windows.h>: its wingdi Rectangle() collides with juce::Rectangle
// under this file's `using namespace juce` (broke both configs).
#if JUCE_WINDOWS
 extern "C" __declspec(dllimport) short __stdcall GetKeyState (int nVirtKey);
#endif

using namespace juce;

// Smoke #45 (spec: Jeff): JUCE's ModifierKeys can't tell the two Alt keys
// apart, and the two carry DIFFERENT gestures here -- LEFT Alt = fine-move
// (no-snap) drag modifier, RIGHT Alt = block mute.  Windows-only app, so the
// Win32 key state is the discriminator (0xA5 = VK_RMENU).
static bool isRightAltKeyDown() noexcept
{
   #if JUCE_WINDOWS
    return (::GetKeyState (0xA5) & 0x8000) != 0;
   #else
    return false;
   #endif
}

// QA-E Task 7 (FILE-02): shared Audio Properties box.  PendingRoute is the
// menu-tree result; buildAudioPropsControls builds the box (definition is an
// anonymous-namespace function further down, next to
// ArrangementGrid::showAudioClipProperties).  BrowserPanel::
// showLibraryPropertiesDialog appears earlier in this file and also uses
// both, so they're declared up here.  (All unnamed namespaces in one TU are
// the same namespace, so these forward decls + the later definitions match.)
namespace {
    // Result of the "Routes to:" menu-tree button.  channelId is a resolved
    // MixerChannelIds id when chosen and not an Add-new sentinel; createKind
    // is -1 none / 0 Clip / 1 Vox / 2 Inst for the "Add a new ___ Page"
    // entries (resolved to a real channel by onCreateRoutablePage at Apply).
    // isCopy: true = Copy (duplicate file), false = Move (relocate entry).
    // The per-clip dialog only ever produces Copy; the browser dialog offers
    // both via per-target submenus.
    struct PendingRoute
    {
        bool chosen     { false };
        bool isCopy     { false };
        int  channelId  { -1 };
        int  createKind { -1 };
    };

    void buildAudioPropsControls (juce::AlertWindow&, float, float, bool,
                                  const juce::String&,
                                  const std::vector<RoutablePageInfo>&,
                                  bool, bool,
                                  std::shared_ptr<juce::TextButton>&,
                                  std::shared_ptr<PendingRoute>&,
                                  const juce::String& bpmDisplayOverride = {});
}

// ─────────────────────────────────────────────────────────────────────────────
// Local colour constants
// ─────────────────────────────────────────────────────────────────────────────
static const Colour kGridBg     { 0xff1a1a1e };
static const Colour kGridLine   { 0xff2a2a30 };
static const Colour kGridLineMj { 0xff3a3a44 };
static const Colour kHeaderBg   { 0xff222228 };
static const Colour kBrowserBg  { 0xff1c1c1e };
static const Colour kAutomCol   { 0xff40e0d0 };  // teal for automation clips

// Block colours per pattern index (cycles every 8)
static const Colour kBlockCols[8] = {
    Colour(0xff3a6fbf), Colour(0xff7a4fbf), Colour(0xffbf6f3a),
    Colour(0xff3abf6f), Colour(0xffbf3a6f), Colour(0xff6fbf3a),
    Colour(0xffbf9f3a), Colour(0xff3abfbf),
};

// ─────────────────────────────────────────────────────────────────────────────
// BrowserItem - single draggable box used for all 3 browser tabs
// ─────────────────────────────────────────────────────────────────────────────
BrowserItem::BrowserItem(Kind k, int index, const String& displayName)
    : mKind(k), mIndex(index), mName(displayName)
{
    setMouseCursor(MouseCursor::PointingHandCursor);
}

void BrowserItem::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.f);

    // Background gradient - accent-tinted, brighter at top.
    Colour bg = mAccent.darker(mSelected ? 0.0f : 0.35f);
    g.setGradientFill(ColourGradient(bg.brighter(0.25f), b.getX(), b.getY(),
                                     bg.darker(0.20f),    b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 4.f);

    // Border - brighter when selected.
    g.setColour(mSelected ? Colours::white.withAlpha(0.85f)
                          : mAccent.brighter(0.35f).withAlpha(0.6f));
    g.drawRoundedRectangle(b, 4.f, mSelected ? 1.5f : 1.f);


    // Drag-affordance dots (left edge) - hint that the item can be dragged.
    g.setColour(Colours::white.withAlpha(0.35f));
    const float gx = b.getX() + 4.f;
    for (int i = 0; i < 3; ++i)
    {
        const float gy = b.getCentreY() - 4.f + (float)i * 4.f;
        g.fillRect(gx, gy, 2.f, 2.f);
    }

    // Name text.
    g.setColour(Colours::white.withAlpha(0.92f));
    g.setFont(Font(11.f, Font::bold));
    g.drawText(mName, (int)(b.getX() + 12.f), (int)b.getY(),
               (int)(b.getWidth() - 14.f), (int)b.getHeight(),
               Justification::centredLeft, true);
}

void BrowserItem::mouseDown(const MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        if (onContextMenu) onContextMenu(e.getScreenPosition());
        return;
    }
    if (onClicked) onClicked();
}

void BrowserItem::mouseDrag(const MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) return;

    // Canonical JUCE drag-start pattern: walk up to find a container, and
    // only start a drag if one isn't already active on this container.
    auto* container = DragAndDropContainer::findParentDragContainerFor(this);
    if (container == nullptr || container->isDragAndDropActive()) return;

    // Require a small pixel threshold so single clicks don't kick off drags.
    if (e.getDistanceFromDragStart() < 3) return;

    const char* kindStr = (mKind == Kind::Pattern   ? "pattern"
                        :  mKind == Kind::Audio     ? "audio"
                        :                              "auto");
    var description(String(kindStr) + ":" + String(mIndex));

    // Component snapshot at natural scale; JUCE applies its own alpha on
    // top. `true` = transparent background.
    Image dragImage = createComponentSnapshot(getLocalBounds(), true, 1.0f);
    container->startDragging(description, this, ScaledImage(dragImage),
                             /*allowDraggingToOtherJuceWindows*/ true);
}

void BrowserItem::mouseUp(const MouseEvent&) {}

void BrowserItem::mouseDoubleClick(const MouseEvent&)
{
    if (onRenameRequested) onRenameRequested();
}

// ─────────────────────────────────────────────────────────────────────────────
// G-5 (2026-04-29): AudioBrowserItem + AudioCategoryItem TreeViewItem subclasses
// ─────────────────────────────────────────────────────────────────────────────
AudioBrowserItem::AudioBrowserItem (const CategorizedAudioEntry& e)
    : mEntry (e)
{
}

void AudioBrowserItem::paintItem (Graphics& g, int width, int height)
{
    // Match the flat-list BrowserItem aesthetic: dark fill + accent stripe on
    // the left + white text.  Selection highlight when this item is the
    // tree's selected item.
    auto r = Rectangle<int> (width, height).reduced (3, 1).toFloat();

    // Background
    g.setColour (isSelected() ? Colour (0xff404858) : Colour (0xff262a30));
    g.fillRoundedRectangle (r, 3.0f);

    // Accent stripe on left
    g.setColour (mEntry.accent);
    g.fillRoundedRectangle (r.withWidth (4.0f), 1.5f);

    // Label
    g.setColour (Colour (0xffe0e4ec));
    g.setFont (Font (12.0f));
    g.drawText (mEntry.displayName,
                r.withTrimmedLeft (10.0f).withTrimmedRight (4.0f).toNearestInt(),
                Justification::centredLeft, true);
}

void AudioBrowserItem::itemClicked (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onContextMenu) onContextMenu (e.getScreenPosition());
        return;
    }
    if (e.getNumberOfClicks() == 2)
    {
        if (onRenameRequested) onRenameRequested();
        return;
    }
    // Single left click: TreeView default selection behavior, plus the
    // QA-H (#20) drop-type notification.
    if (onSelected) onSelected();
}

AudioCategoryItem::AudioCategoryItem (const String& name, Colour accent)
    : mName (name), mAccent (accent)
{
    setOpen (true);   // expand by default
}

void AudioCategoryItem::paintItem (Graphics& g, int width, int height)
{
    auto r = Rectangle<int> (width, height).reduced (2, 1).toFloat();

    // Header background - slightly lighter than item background, with accent tint
    g.setColour (Colour (0xff1c2028));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (mAccent.withAlpha (0.3f));
    g.fillRoundedRectangle (r.withWidth (3.0f), 1.0f);

    // Section name + count
    const int   childCount = const_cast<AudioCategoryItem*> (this)->getNumSubItems();
    const String label = mName + (childCount > 0
                                       ? " (" + String (childCount) + ")"
                                       : " (none)");

    g.setColour (Colour (0xffc0c4cc));
    g.setFont (Font (12.0f, Font::bold));
    g.drawText (label,
                r.withTrimmedLeft (10.0f).withTrimmedRight (4.0f).toNearestInt(),
                Justification::centredLeft, true);
}

void AudioCategoryItem::itemClicked (const MouseEvent& e)
{
    // Toggle open state on label click for usability (in addition to the
    // triangle icon).  QA-Fe2: right-click opens the category menu
    // ("Create Group...").
    if (e.mods.isPopupMenu())
    {
        if (onContextMenu) onContextMenu (e.getScreenPosition());
        return;
    }
    setOpen (! isOpen());
}

// ── QA-Fe2: AudioGroupItem ───────────────────────────────────────────────────
AudioGroupItem::AudioGroupItem (const String& name, Colour accent, bool isAuto)
    : mName (name), mAccent (accent), mIsAuto (isAuto)
{
    setOpen (true);
}

void AudioGroupItem::paintItem (Graphics& g, int width, int height)
{
    auto r = Rectangle<int> (width, height).reduced (2, 1).toFloat();
    g.setColour (Colour (0xff20242c));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (mAccent.withAlpha (0.55f));
    g.fillRoundedRectangle (r.withWidth (3.0f), 1.0f);

    const int n = const_cast<AudioGroupItem*> (this)->getNumSubItems();
    g.setColour (Colour (0xffd0d4dc));
    g.setFont (Font (12.0f, Font::bold));
    g.drawText (mName + " (" + String (n) + ")",
                r.withTrimmedLeft (8.0f).withTrimmedRight (4.0f).toNearestInt(),
                Justification::centredLeft, true);
}

void AudioGroupItem::itemClicked (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onContextMenu) onContextMenu (e.getScreenPosition());
        return;
    }
    setOpen (! isOpen());
}

// ─────────────────────────────────────────────────────────────────────────────
// BrowserPanel
// ─────────────────────────────────────────────────────────────────────────────
BrowserPanel::BrowserPanel(PatternManager& pm,
                           AudioFormatManager& afm,
                           AudioThumbnailCache& thumbCache)
    : mPM(pm), mAFM(afm), mThumbCache(thumbCache)
{
    // TS7 (Jeff, 2026-07-29): sort order for the Exports / Reports sections.
    mSortBtn = std::make_unique<TextButton>("Sort");
    mSortBtn->setTooltip ("Sort the Exports and Reports lists");
    mSortBtn->setColour (TextButton::buttonColourId, VC::Surface);
    mSortBtn->setColour (TextButton::textColourOffId, VC::Text);
    mSortBtn->onClick = [this] { showSortMenu(); };
    addChildComponent(*mSortBtn);   // shown by switchTab on the Files tab only

    // TS7 §11.1: "Audio" becomes "Files" -- the section now carries the project's
    // exports and reports alongside its clip/vox/inst audio, so the old name
    // undersells what is in it.
    static const char* kTabLabels[] = { "Patterns", "Files", "Auto" };
    for (int t = 0; t < 3; ++t)
    {
        mTabBtns[t] = std::make_unique<TextButton>(kTabLabels[t]);
        mTabBtns[t]->setClickingTogglesState(false);
        const int ti = t;
        mTabBtns[t]->onClick = [this, ti] { switchTab(ti); };
        mTabBtns[t]->setColour(TextButton::buttonColourId,   VC::Surface);
        mTabBtns[t]->setColour(TextButton::buttonOnColourId, VC::Highlight);
        addAndMakeVisible(*mTabBtns[t]);
    }

    mAddBtn = std::make_unique<TextButton>("+ Add");
    mAddBtn->onClick = [this] {
        performPatternSliceOp ("Add Pattern", [this] {
            mPM.addPattern();
            mSelectedPat = mPM.getNumPatterns() - 1;
            mPM.setCurrentPattern(mSelectedPat);
        });
        rebuildPatternRows();
        if (onPatternSelected) onPatternSelected(mSelectedPat);
    };
    addAndMakeVisible(*mAddBtn);

    mDeleteBtn = std::make_unique<TextButton>("Delete");
    mDeleteBtn->onClick = [this] {
        if (mPM.getNumPatterns() > 1) {
            performPatternSliceOp ("Delete Pattern", [this] {
                mPM.removePattern(mSelectedPat);
                mSelectedPat = jlimit(0, mPM.getNumPatterns() - 1, mSelectedPat);
                mPM.setCurrentPattern(mSelectedPat);
            });
            rebuildPatternRows();
            if (onPatternSelected) onPatternSelected(mSelectedPat);
        }
    };
    addAndMakeVisible(*mDeleteBtn);

    // G-5 (2026-04-29): build the unified Audio tree.  Three category nodes
    // (Clips amber / Vox teal / Inst navy) under an invisible root.  Leaves
    // are populated lazily by rebuildAudioRows() via onEnumerateAudio.
    mAudioTree = std::make_unique<juce::TreeView>();
    mAudioTree->setIndentSize (12);
    mAudioTree->setMultiSelectEnabled (false);
    mAudioTree->setDefaultOpenness (true);
    mAudioTree->setColour (juce::TreeView::backgroundColourId, juce::Colour (0xff181c20));
    mAudioTree->setColour (juce::TreeView::linesColourId,      juce::Colour (0xff303640));
    addChildComponent (*mAudioTree);   // initial visibility off; switchTab(1) shows it

    mAudioRoot = std::make_unique<AudioRootItem>();   // invisible root holding the 3 categories
    auto clipsCat = std::make_unique<AudioCategoryItem> ("Clips", juce::Colour (0xffd4a017));
    auto voxCat   = std::make_unique<AudioCategoryItem> ("Vox",   juce::Colour (0xff0fafa5));
    auto instCat  = std::make_unique<AudioCategoryItem> ("Inst",  juce::Colour (0xff1c3a8a));
    mClipsCat = clipsCat.get();
    mVoxCat   = voxCat  .get();
    mInstCat  = instCat .get();
    // QA-Fe2: category headers get the group-creation menu (0 Clips / 1 Vox
    // / 2 Inst -- literal category indices, mirrors the browser convention).
    mClipsCat->onContextMenu = [this] (juce::Point<int> pt) { showCategoryContextMenu (0, pt); };
    mVoxCat  ->onContextMenu = [this] (juce::Point<int> pt) { showCategoryContextMenu (1, pt); };
    mInstCat ->onContextMenu = [this] (juce::Point<int> pt) { showCategoryContextMenu (2, pt); };

    // TS7 §11.2/§11.3: two more categories -- what the user has RENDERED.
    //
    // §11.4, INVARIANT AMENDED RATHER THAN BROKEN QUIETLY.  BuilderPage.h records
    // "there is NO 4th 'Imported' / 'Library' bucket per Jeff's invariant ('all
    // importable files become clips')".  That invariant governs IMPORT material,
    // and these are OUTPUT -- files the user made, which they should not have to
    // go hunting for.  The header comment is updated to say so.
    auto exportsCat = std::make_unique<AudioCategoryItem> ("Exports",
                                                           juce::Colour (0xff00fff2));
    auto reportsCat = std::make_unique<AudioCategoryItem> ("Reports",
                                                           juce::Colour (0xffff9100));
    mExportsCat = exportsCat.get();
    mReportsCat = reportsCat.get();

    mAudioRoot->addSubItem (clipsCat.release());
    mAudioRoot->addSubItem (voxCat  .release());
    mAudioRoot->addSubItem (instCat .release());
    mAudioRoot->addSubItem (exportsCat.release());
    mAudioRoot->addSubItem (reportsCat.release());
    mAudioTree->setRootItem (mAudioRoot.get());
    mAudioTree->setRootItemVisible (false);

    rebuildPatternRows();
    switchTab(0);
}

void BrowserPanel::setCollapsed(bool c)
{
    mCollapsed = c;
    for (auto& t : mTabBtns) t->setVisible(!c);
    for (auto& r : mPatItems)   r->setVisible(!c);
    for (auto& r : mAudioItems) r->setVisible(!c);
    for (auto& r : mAutomItems) r->setVisible(!c);
    mAddBtn->setVisible(!c);
    mDeleteBtn->setVisible(!c);
    // G-5: tree visibility tracks the collapsed state on the Audio tab.
    if (mAudioTree) mAudioTree->setVisible (! c && mActiveTab == 1);
    if (auto* p = getParentComponent()) p->resized();
    repaint();
}

void BrowserPanel::switchTab(int t)
{
    mActiveTab = t;
    bool isPat  = (t == 0);
    bool isAud  = (t == 1);
    bool isAuto = (t == 2);

    for (auto& r : mPatItems)   r->setVisible(isPat);
    for (auto& r : mAudioItems) r->setVisible(isAud);   // G-5: legacy flat list, kept empty
    for (auto& r : mAutomItems) r->setVisible(isAuto);
    // G-5 (2026-04-29): Audio tab now drives the unified tree, not the flat list.
    if (mAudioTree) mAudioTree->setVisible (isAud && ! mCollapsed);
    mAddBtn   ->setVisible(isPat);
    mDeleteBtn->setVisible(isPat);
    // TS7: Sort orders the Exports / Reports lists, which only exist on Files.
    if (mSortBtn) mSortBtn->setVisible (isAud && ! mCollapsed);

    for (int i = 0; i < 3; ++i)
        mTabBtns[i]->setToggleState(i == t, dontSendNotification);

    if (isAud)  rebuildAudioRows();
    if (isAuto) rebuildAutomationRows();
    resized();

    // QA-H Task 8 (#20): switching tabs re-arms the drop type with that
    // tab's remembered pick (-1 = nothing picked there yet).
    if (onDropTypeChanged)
        onDropTypeChanged (t, t == 0 ? mSelectedPat
                            : t == 1 ? mLastAudioSel : mLastAutomSel);
}

void BrowserPanel::selectPattern(int idx)
{
    mSelectedPat = idx;
    mPM.setCurrentPattern(idx);
    for (int i = 0; i < (int)mPatItems.size(); ++i)
        if (mPatItems[i]) mPatItems[i]->setSelected(i == idx);
    if (onPatternSelected)   onPatternSelected(idx);
    if (onDropTypeChanged)   onDropTypeChanged(0, idx);
}

// QA-H Task 8 (#20): automation rows were drag-only - a click now records
// the entry (selection highlight + drop-type callback).
void BrowserPanel::selectAutomationItem (int itemIdx)
{
    mLastAutomSel = (itemIdx >= 0 && itemIdx < (int) mAutomBlockIndices.size())
                  ? mAutomBlockIndices[(size_t) itemIdx] : -1;
    for (int i = 0; i < (int) mAutomItems.size(); ++i)
        if (mAutomItems[i]) mAutomItems[i]->setSelected (i == itemIdx);
    if (onDropTypeChanged) onDropTypeChanged (2, mLastAutomSel);
}

void BrowserPanel::rebuildPatternRows()
{
    for (auto& r : mPatItems) removeChildComponent(r.get());
    mPatItems.clear();

    for (int i = 0; i < mPM.getNumPatterns(); ++i)
    {
        const int pi = i;
        auto item = std::make_unique<BrowserItem>(BrowserItem::Kind::Pattern, pi,
                                                  mPM.getPattern(pi).name);
        item->setAccentColour(mPM.getPattern(pi).color);   // F-1 (2026-04-26)
        item->setSelected(pi == mSelectedPat);
        item->onClicked = [this, pi] { selectPattern(pi); };
        BrowserItem* raw = item.get();
        raw->onRenameRequested = [this, raw] { openRenamePopup(*raw); };
        raw->onContextMenu     = [this, raw](Point<int> pt) { showItemContextMenu(*raw, pt); };
        addAndMakeVisible(*item);
        mPatItems.push_back(std::move(item));
    }

    if (mActiveTab != 0) switchTab(mActiveTab);
    resized();
    repaint();
}

void BrowserPanel::rebuildAudioRows()
{
    // G-5 (2026-04-29): unified Audio tree.  Clear all leaves under each
    // category, re-enumerate via onEnumerateAudio (page-walk), bucket by
    // category, re-attach.  mAudioPaths still tracks position-by-libIdx so
    // the existing right-click "Remove" + drag descriptor flows keep working
    // (descriptor format is `audio:<libIdx>` - unchanged from flat-list era).
    mAudioPaths.clear();
    if (! mClipsCat || ! mVoxCat || ! mInstCat) return;

    mClipsCat->clearSubItems();
    mVoxCat  ->clearSubItems();
    mInstCat ->clearSubItems();

    // Track audioLibrary index → mAudioPaths position so existing index-based
    // lookups (Remove, drag descriptor lookup in ArrangementGrid::itemDropped)
    // resolve to the right path even though we're page-walking now.
    // We populate mAudioPaths in audioLibrary index order so that
    // mAudioPaths[i] == mPM.getAudioLibraryPath(i).
    for (int i = 0; i < mPM.getNumAudioLibrary(); ++i)
        mAudioPaths.add (mPM.getAudioLibraryPath (i));

    // Walk pages via the editor-supplied enumerator.  No callback wired =
    // empty tree (defensive).
    if (! onEnumerateAudio) return;

    auto entries = onEnumerateAudio();

    auto makeLeaf = [this] (const CategorizedAudioEntry& e) -> AudioBrowserItem*
    {
        auto* leaf = new AudioBrowserItem (e);
        leaf->onRenameRequested = [this, leaf]
        {
            // G-5: rename uses the audio library index, mirroring the flat-list
            // path.  Tree refresh happens via the existing refresh() timer.
            const int libIdx = leaf->getAudioLibIdx();
            const String current = leaf->getDisplayName();
            auto editor = std::make_unique<TextEditor>();
            editor->setText (current, false);
            editor->setFont (Font (13.f));
            editor->setSelectAllWhenFocused (true);
            editor->setSize (180, 26);
            editor->setEscapeAndReturnKeysConsumed (true);
            auto* rawEdit = editor.get();
            editor->onReturnKey = [this, libIdx, rawEdit]
            {
                const String t = rawEdit->getText().trim();
                if (t.isNotEmpty()) renameAudioAt (libIdx, t);
                if (auto* cb = rawEdit->findParentComponentOfClass<CallOutBox>())
                    cb->dismiss();
            };
            editor->onEscapeKey = [rawEdit]
            {
                if (auto* cb = rawEdit->findParentComponentOfClass<CallOutBox>())
                    cb->dismiss();
            };
            // Anchor the popup near the tree leaf.
            Rectangle<int> anchor;
            if (mAudioTree)
                anchor = mAudioTree->getScreenBounds().withHeight (28);
            CallOutBox::launchAsynchronously (std::move (editor), anchor, nullptr);
        };
        leaf->onContextMenu = [this, leaf](Point<int> pt)
        {
            showAudioTreeContextMenu (*leaf, pt);
        };
        leaf->onSelected = [this, leaf]
        {
            mLastAudioSel = leaf->getAudioLibIdx();
            if (onDropTypeChanged) onDropTypeChanged (1, mLastAudioSel);
        };
        return leaf;
    };

    // QA-Fe2 grouping: manual groupName wins; else Vox/Inst recordings
    // auto-group by take-tag base (CLEANED tags matched before plain --
    // suffix order is load-bearing).  Clips never auto-group (uploads, not
    // recordings).  Registry groups render even when empty.
    static const char* kTagSuffix[4] = { " - DRY CLEANED", " - WET CLEANED",
                                         " - DRY", " - WET" };
    static const char* kTagLabel [4] = { "Dry Cleaned", "Wet Cleaned",
                                         "Dry", "Wet" };
    const Colour catAccent[3] = { Colour (0xffd4a017), Colour (0xff0fafa5),
                                  Colour (0xff1c3a8a) };
    AudioCategoryItem* catNode[3] = { mClipsCat, mVoxCat, mInstCat };

    struct GroupBucket
    {
        bool isAuto { false };
        std::vector<std::pair<CategorizedAudioEntry, String>> members; // (entry, leaf-label override)
    };
    std::vector<std::pair<String, GroupBucket>> groups[3];
    std::vector<CategorizedAudioEntry>          flat[3];

    auto bucketFor = [&groups] (int ci, const String& key, bool isAuto) -> GroupBucket&
    {
        for (auto& [k, b] : groups[ci])
            if (k == key) return b;
        groups[ci].push_back ({ key, GroupBucket { isAuto, {} } });
        return groups[ci].back().second;
    };

    for (int ci = 0; ci < 3; ++ci)
        for (const auto& g : mPM.getManualAudioGroups (ci))
            bucketFor (ci, g, false);

    for (auto& e : entries)
    {
        const int ci = e.category == "Clips" ? 0
                     : e.category == "Vox"   ? 1
                     : e.category == "Inst"  ? 2 : -1;
        if (ci < 0) continue;

        if (e.groupName.isNotEmpty())
        {
            bucketFor (ci, e.groupName, false).members.push_back ({ e, {} });
            continue;
        }
        bool grouped = false;
        if (ci != 0)
        {
            const String stem = juce::File (e.fullPath).getFileNameWithoutExtension();
            for (int t = 0; t < 4 && ! grouped; ++t)
                if (stem.endsWith (kTagSuffix[t]))
                {
                    const String base = stem.upToLastOccurrenceOf (kTagSuffix[t], false, false);
                    bucketFor (ci, base, true).members.push_back ({ e, kTagLabel[t] });
                    grouped = true;
                }
        }
        if (! grouped) flat[ci].push_back (e);
    }

    for (int ci = 0; ci < 3; ++ci)
    {
        for (auto& gpair : groups[ci])
        {
            // Plain locals (not structured bindings): C++17 lambdas cannot
            // portably capture bindings.
            const String       key    = gpair.first;
            const GroupBucket& bucket = gpair.second;
            const bool         isAuto = bucket.isAuto;

            auto* grp = new AudioGroupItem (key, catAccent[ci], isAuto);
            grp->onContextMenu = [this, ci, key, isAuto] (Point<int> pt)
            {
                showGroupContextMenu (ci, key, isAuto, pt);
            };
            // TS7: order the leaves INSIDE this group.  The group itself keeps
            // its position -- only its contents move.
            std::vector<CategorizedAudioEntry> shownMembers;
            shownMembers.reserve (bucket.members.size());
            for (const auto& mpair : bucket.members)
            {
                auto shown = mpair.first;
                if (mpair.second.isNotEmpty()) shown.displayName = mpair.second;
                shownMembers.push_back (shown);
            }
            sortEntries (shownMembers);
            for (const auto& shown : shownMembers)
                grp->addSubItem (makeLeaf (shown));

            catNode[ci]->addSubItem (grp);
        }
        // TS7: and the ungrouped leaves of this category.
        sortEntries (flat[ci]);
        for (auto& e : flat[ci])
            catNode[ci]->addSubItem (makeLeaf (e));
    }

    rebuildRenderRows();
}

void BrowserPanel::setItemSort (ItemSort s)
{
    if (s == mItemSort) return;
    mItemSort = s;
    rebuildAudioRows();   // rebuilds every category, render rows included
}

void BrowserPanel::showSortMenu()
{
    PopupMenu m;
    m.addItem (1, "Newest first",  true, mItemSort == ItemSort::NewestFirst);
    m.addItem (2, "Oldest first",  true, mItemSort == ItemSort::OldestFirst);
    m.addItem (3, "Alphabetical",  true, mItemSort == ItemSort::Alphabetical);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (mSortBtn.get()),
        [this] (int r)
        {
            if      (r == 1) setItemSort (ItemSort::NewestFirst);
            else if (r == 2) setItemSort (ItemSort::OldestFirst);
            else if (r == 3) setItemSort (ItemSort::Alphabetical);
        });
}

// ONE ordering implementation for every category and every group, so the Sort
// pick cannot mean different things in different parts of the tree.
//
// Time comes from the FILE's modification time rather than the display name:
// exports and reports carry timestamps in their names, but a renamed file would
// otherwise jump position for no reason the user can see.
void BrowserPanel::sortEntries (std::vector<CategorizedAudioEntry>& v) const
{
    const auto mode = mItemSort;
    std::stable_sort (v.begin(), v.end(),
        [mode] (const CategorizedAudioEntry& a, const CategorizedAudioEntry& b)
        {
            if (mode == ItemSort::Alphabetical)
                return a.displayName.compareIgnoreCase (b.displayName) < 0;

            const auto ta = juce::File (a.fullPath).getLastModificationTime();
            const auto tb = juce::File (b.fullPath).getLastModificationTime();
            return mode == ItemSort::NewestFirst ? (ta > tb) : (ta < tb);
        });
}

// ── TS7 §11.2/§11.3: Exports + Reports ───────────────────────────────────────
// Listed straight off disk rather than out of audioLibrary: these have no bound
// page, which is exactly the case the library's own invariant excludes.  Nothing
// here is auto-added to the grid.
//
// The FREEZE folder is deliberately absent -- regenerable cache, not something
// the user made, and surfacing it would invite dragging a frozen render back into
// the arrangement it was frozen FROM.
void BrowserPanel::rebuildRenderRows()
{
    if (! mExportsCat || ! mReportsCat) return;
    mExportsCat->clearSubItems();
    mReportsCat->clearSubItems();
    if (! onGetRenderFolders) return;

    const auto folders = onGetRenderFolders();   // { exports, reports }

    // §11.5a (Jeff, 2026-07-29): a render that has been added and routed MOVES
    // GROUPS -- it belongs to Clips / Vox / Inst now and must stop showing here,
    // or one file would occupy two rows.  The move itself needs no code: the
    // library walk in onEnumerateAudio buckets by pageOwnerChannelId and picks
    // it up under its new category automatically.  All that is left is for this
    // listing to stand down from any file the library has already claimed.
    //
    // fullPath off onEnumerateAudio is already resolved to absolute, so it
    // compares directly against the on-disk sweep below.  StringArray's
    // ignore-case contains is the point rather than a plain set: these paths
    // round-trip through a project-relative stored form, and Windows will hand
    // back a different case than the one that was written.
    juce::StringArray claimed;
    if (onEnumerateAudio)
        for (const auto& e : onEnumerateAudio())
            claimed.add (e.fullPath);

    // Same ordering as every other category, through the one helper.
    auto fill = [this, &claimed] (AudioCategoryItem* cat, const juce::File& dir,
                                  const juce::String& wildcard, juce::Colour accent)
    {
        if (cat == nullptr || ! dir.isDirectory()) return;
        juce::Array<juce::File> files;
        dir.findChildFiles (files, juce::File::findFiles, false, wildcard);

        std::vector<CategorizedAudioEntry> entries;
        entries.reserve ((size_t) files.size());
        for (const auto& f : files)
        {
            if (claimed.contains (f.getFullPathName(), true)) continue;

            CategorizedAudioEntry e;
            e.audioLibIdx = -1;            // not a library entry -- no drag index
            e.displayName = f.getFileNameWithoutExtension();
            e.fullPath    = f.getFullPathName();
            e.accent      = accent;
            entries.push_back (e);
        }
        sortEntries (entries);
        for (const auto& e : entries)
            cat->addSubItem (new AudioBrowserItem (e));
    };

    fill (mExportsCat, folders.first,  "*.wav;*.ogg;*.mp3", juce::Colour (0xff00fff2));
    fill (mReportsCat, folders.second, "*.html;*.csv",      juce::Colour (0xffff9100));
}

// ── TS7 §11.5/§11.5a: add a render to the project ────────────────────────────
// Reached from BOTH the Exports row's "Add to Project..." and a drag of that
// row onto the grid, so the two gestures cannot diverge into meaning different
// things.
//
// No Move/Copy question here, unlike the library Properties dialog: the file is
// unrouted, so there is nothing to move it FROM, and per the owner a copy is a
// thing the user does afterwards if they want one -- not a decision to make
// while adding.  One flat list of targets, worded the same as that menu.
void BrowserPanel::beginAddRenderToProject (const juce::File& f,
                                            std::function<void(int)> onRouted)
{
    if (! f.existsAsFile()) return;

    // Project-relative when the file sits in the project's own render folder,
    // so the entry survives the project folder being moved or renamed.  The
    // folder NAME is taken from the File the editor handed us rather than a
    // second hardcoded "Exports" that could drift from getProjectExportsDir.
    juce::String stored = f.getFullPathName();
    if (onGetRenderFolders)
    {
        const auto dir = onGetRenderFolders().first;
        if (dir != juce::File() && f.isAChildOf (dir))
            stored = dir.getFileName() + "/" + f.getFileName();
    }

    std::vector<RoutablePageInfo> pages;
    if (onEnumerateRoutablePages) pages = onEnumerateRoutablePages();

    struct Tgt { int channelId; int createKind; juce::String name; };
    auto targets = std::make_shared<std::vector<Tgt>>();
    for (const auto& pg : pages)
        targets->push_back ({ pg.channelId, -1, pg.displayName });
    targets->push_back ({ -1, 0, "a new Clip Page" });
    targets->push_back ({ -1, 1, "a new Vox Page" });
    targets->push_back ({ -1, 2, "a new Inst Page" });

    juce::PopupMenu m;
    m.addSectionHeader ("Add \"" + f.getFileNameWithoutExtension() + "\" to:");
    for (int t = 0; t < (int) targets->size(); ++t)
        m.addItem (t + 1, (*targets)[(size_t) t].name);

    juce::Component::SafePointer<BrowserPanel> safeThis (this);
    m.showMenuAsync (juce::PopupMenu::Options(),
        [safeThis, targets, stored, onRouted] (int r)
        {
            auto* self = safeThis.getComponent();
            if (self == nullptr || r <= 0 || r > (int) targets->size()) return;
            const auto& tg = (*targets)[(size_t) (r - 1)];

            // Adding the same render twice re-routes the one entry instead of
            // growing a second row for a single file.
            int libIdx = self->mPM.findAudioLibraryIndexByPath (stored);
            if (libIdx < 0)
            {
                self->mPM.addAudioToLibrary (stored, {}, 0);
                libIdx = self->mPM.findAudioLibraryIndexByPath (stored);
            }
            if (libIdx < 0) return;

            // Register-then-create-then-route, the same order the Properties
            // "Move to a new page" path uses: the page factory re-adds under a
            // real channel, which addAudioToLibrary resolves by UPGRADING this
            // owner-0 entry rather than making a second one.
            int target = tg.channelId;
            if (tg.createKind >= 0 && self->onCreateRoutablePage)
                target = self->onCreateRoutablePage (tg.createKind, stored);
            if (target < 0) return;

            if (self->onApplyLibraryProperties)
                self->onApplyLibraryProperties (libIdx,
                                                self->mPM.getAudioLibraryPitch (libIdx),
                                                self->mPM.getAudioLibraryBPM (libIdx),
                                                self->mPM.getAudioLibraryStretchMode (libIdx),
                                                target);
            else
                self->mPM.setAudioLibraryPageOwner (libIdx, target);

            self->maybePromptGroupAssign (libIdx, target);
            self->rebuildAudioRows();     // appears under its routed category
            self->rebuildRenderRows();    // and is no longer an Exports orphan
            if (onRouted) onRouted (libIdx);
        });
}

// ── QA-Fe2 browser groups: menus + move/copy group assignment ────────────────

void BrowserPanel::showCategoryContextMenu (int category, Point<int> globalPt)
{
    PopupMenu m;
    m.addItem (1, "Create Group...");
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (
                         Rectangle<int> (globalPt.x, globalPt.y, 1, 1)),
        [this, category] (int r)
        {
            if (r != 1) return;
            auto editor = std::make_unique<TextEditor>();
            editor->setText ("New Group", false);
            editor->setFont (Font (13.f));
            editor->setSelectAllWhenFocused (true);
            editor->setSize (180, 26);
            editor->setEscapeAndReturnKeysConsumed (true);
            auto* raw = editor.get();
            editor->onReturnKey = [this, category, raw]
            {
                const String t = raw->getText().trim();
                if (t.isNotEmpty())
                {
                    performLibraryOp ("Create Group", /*withBlocks*/ false,
                                      [this, category, &t] { mPM.addManualAudioGroup (category, t); });
                    rebuildAudioRows();
                }
                if (auto* cb = raw->findParentComponentOfClass<CallOutBox>())
                    cb->dismiss();
            };
            editor->onEscapeKey = [raw]
            {
                if (auto* cb = raw->findParentComponentOfClass<CallOutBox>())
                    cb->dismiss();
            };
            Rectangle<int> anchor;
            if (mAudioTree) anchor = mAudioTree->getScreenBounds().withHeight (28);
            CallOutBox::launchAsynchronously (std::move (editor), anchor, nullptr);
        });
}

void BrowserPanel::showGroupContextMenu (int category, const String& name,
                                         bool isAuto, Point<int> globalPt)
{
    // Auto (recording) group rename moves files the clip streamers may hold
    // open -> same stop-gate as Regenerate De-noise.  Manual groups are
    // label-only renames and stay available during playback.
    const bool renameOk = ! isAuto || ! DSPBase::isTransportPlaying();
    PopupMenu m;
    m.addItem (1, renameOk ? "Rename Group..."
                           : "Rename Group... (stop playback)", renameOk);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (
                         Rectangle<int> (globalPt.x, globalPt.y, 1, 1)),
        [this, category, name, isAuto] (int r)
        {
            if (r != 1) return;
            auto editor = std::make_unique<TextEditor>();
            editor->setText (name, false);
            editor->setFont (Font (13.f));
            editor->setSelectAllWhenFocused (true);
            editor->setSize (220, 26);
            editor->setEscapeAndReturnKeysConsumed (true);
            auto* raw = editor.get();
            editor->onReturnKey = [this, category, name, isAuto, raw]
            {
                const String t = raw->getText().trim();
                if (t.isNotEmpty() && t != name)
                {
                    if (isAuto)
                    {
                        // Disk-rename flow lives in StandaloneEditor.
                        if (onRenameRecordingGroup)
                            onRenameRecordingGroup (name, t);
                    }
                    else
                    {
                        performLibraryOp ("Rename Group", /*withBlocks*/ false,
                                          [this, category, &name, &t]
                                          { mPM.renameManualAudioGroup (category, name, t); });
                    }
                    rebuildAudioRows();
                }
                if (auto* cb = raw->findParentComponentOfClass<CallOutBox>())
                    cb->dismiss();
            };
            editor->onEscapeKey = [raw]
            {
                if (auto* cb = raw->findParentComponentOfClass<CallOutBox>())
                    cb->dismiss();
            };
            Rectangle<int> anchor;
            if (mAudioTree) anchor = mAudioTree->getScreenBounds().withHeight (28);
            CallOutBox::launchAsynchronously (std::move (editor), anchor, nullptr);
        });
}

void BrowserPanel::maybePromptGroupAssign (int libIdx, int targetChannel)
{
    // QA-Layout T11: literals replaced with the MixerChannelIds caps (the old
    // 606/706 bounds were stale below the real strip counts).
    using namespace MixerChannelIds;
    const int category = (targetChannel >= kVoxBase  && targetChannel < kVoxBase  + kMaxVoxStrips)  ? 1
                       : (targetChannel >= kInstBase && targetChannel < kInstBase + kMaxInstStrips) ? 2 : -1;
    if (category < 0 || libIdx < 0 || libIdx >= mPM.getNumAudioLibrary()) return;

    // Candidate groups "connected to that page": the target page's own
    // recording groups (auto bases from entries it owns) + the category's
    // manual groups.
    juce::StringArray candidates = mPM.getManualAudioGroups (category);
    static const char* kTags[4] = { " - DRY CLEANED", " - WET CLEANED",
                                    " - DRY", " - WET" };
    for (int i = 0; i < mPM.getNumAudioLibrary(); ++i)
    {
        if (mPM.getAudioLibraryPageOwner (i) != targetChannel) continue;
        const String stem = juce::File (mPM.getAudioLibraryPath (i)).getFileNameWithoutExtension();
        for (int t = 0; t < 4; ++t)
            if (stem.endsWith (kTags[t]))
            {
                candidates.addIfNotAlreadyThere (stem.upToLastOccurrenceOf (kTags[t], false, false));
                break;
            }
    }
    if (candidates.isEmpty()) return;

    auto* aw = new juce::AlertWindow ("Add to Group",
        "Add this file to one of the destination page's groups?",
        juce::MessageBoxIconType::QuestionIcon);
    juce::StringArray items;
    items.add ("(none)");
    items.addArray (candidates);
    aw->addComboBox ("grp", items, "Group:");
    aw->addButton ("OK", 1);

    juce::Component::SafePointer<BrowserPanel> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, libIdx] (int r)
        {
            auto* self = safeThis.getComponent();
            if (! self || r != 1) return;
            int sel = 0;
            if (auto* cb = aw->getComboBoxComponent ("grp"))
                sel = juce::jmax (0, cb->getSelectedItemIndex());
            if (sel > 0)
            {
                const juce::String g = aw->getComboBoxComponent ("grp")->getText();
                self->performLibraryOp ("Assign Group", /*withBlocks*/ false,
                                        [self, libIdx, &g]
                                        { self->mPM.setAudioLibraryGroup (libIdx, g); });
                self->rebuildAudioRows();
            }
        }),
        true);
}

void BrowserPanel::showAudioTreeContextMenu (AudioBrowserItem& item, Point<int> globalPt)
{
    const int libIdx = item.getAudioLibIdx();

    // TS7 §11.5/§11.6: Exports and Reports are not library entries (libIdx -1),
    // so they get their own short menu rather than falling through the
    // library-index guard below and silently offering nothing.
    if (libIdx < 0)
    {
        const juce::File f (item.getFullPath());
        if (! f.existsAsFile()) return;
        const bool isReport = f.hasFileExtension ("html;csv");

        PopupMenu m;
        if (isReport)
        {
            // §11.6: opens IN THE APP, in the analyzer -- the same view the user
            // watched live.  HTML is never rendered in-app (no WebBrowser).
            m.addItem (1, "Open in Analyzer", f.hasFileExtension ("html"));
        }
        else
        {
            // §11.5: nothing is auto-added to the grid.  Adding imports the file
            // as audio, after which the EXISTING route-to-a-tab / create-a-new-one
            // flow applies -- the same one clips and vox/inst recordings use.
            m.addItem (2, "Add to Project...");
        }
        m.addSeparator();
        m.addItem (3, "Reveal in Explorer");

        m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (
                             Rectangle<int> (globalPt.x, globalPt.y, 1, 1)),
            [this, f] (int r)
            {
                if      (r == 1) { if (onOpenReport) onOpenReport (f); }
                else if (r == 2) { beginAddRenderToProject (f, nullptr); }
                else if (r == 3) { f.revealToUser(); }
            });
        return;
    }

    if (libIdx >= mPM.getNumAudioLibrary()) return;

    constexpr int kIdRename     = 1;
    constexpr int kIdDuplicate  = 2;
    constexpr int kIdDelete     = 3;
    constexpr int kIdProperties = 4;   // QA-E Task 7 (FILE-02)
    constexpr int kIdReveal     = 7;
    constexpr int kIdChokeBase  = 200;
    constexpr int kIdRegenLight  = 300;   // QA-Fe2 De-noise
    constexpr int kIdRegenStrong = 301;

    PopupMenu m;
    m.addItem (kIdRename,    "Rename...");
    m.addItem (kIdDuplicate, "Duplicate...");
    m.addItem (kIdReveal,    "Reveal in Explorer");
    // QA-E Task 7 (FILE-02): the library entry is the source of truth for
    // routing.  Editing this moves every grid copy still following it.
    m.addItem (kIdProperties, "Properties...");

    // QA-Fe2: cleaned takes can be re-generated at either strength from the
    // stored (or self-learned) profile.  Stop-gated (grey while playing) --
    // the clip streamers hold the target file open during playback, so a
    // mid-play overwrite would fail on Windows.  Hard guard lives in
    // StandaloneEditor::regenerateDenoise.
    const String libPath = mPM.getAudioLibraryPath (libIdx);
    const String stem    = File (libPath).getFileNameWithoutExtension();
    const bool isCleanedTake = stem.endsWith (" - DRY CLEANED")
                            || stem.endsWith (" - WET CLEANED");
    if (isCleanedTake && onRegenerateDenoise != nullptr)
    {
        const bool stopped = ! DSPBase::isTransportPlaying();
        PopupMenu regen;
        regen.addItem (kIdRegenLight,  "Light",  stopped);
        regen.addItem (kIdRegenStrong, "Strong", stopped);
        m.addSubMenu (stopped ? "Regenerate De-noise"
                              : "Regenerate De-noise (stop playback)", regen);
    }
    m.addSeparator();

    // Choke Group submenu - same model as the flat-list audio context menu.
    const int curGroup = mPM.getAudioLibraryChokeGroup (libIdx);
    PopupMenu chokeSub;
    chokeSub.addItem (kIdChokeBase, "None", true, curGroup == 0);
    for (int g = 1; g <= 16; ++g)
        chokeSub.addItem (kIdChokeBase + g, "Group " + String (g),
                          true, curGroup == g);
    m.addSubMenu ("Choke Group", chokeSub);
    m.addSeparator();
    m.addItem (kIdDelete, "Delete");

    // Capture the resolved absolute path BEFORE the async menu fires so
    // Reveal in Explorer doesn't try to walk the relative-path string the
    // library stores ("Samples/foo.wav") - that fails File::existsAsFile.
    const String absPath = item.getFullPath();
    auto onRename = item.onRenameRequested;

    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (
                         Rectangle<int> (globalPt.x, globalPt.y, 1, 1)),
        [this, libIdx, absPath, libPath, onRename] (int result)
        {
            if (result == 0) return;

            if (result == kIdRename)
            {
                if (onRename) onRename();
                return;
            }
            if (result == kIdRegenLight || result == kIdRegenStrong)
            {
                if (onRegenerateDenoise)
                    onRegenerateDenoise (libPath, result == kIdRegenStrong ? 1 : 0);
                return;
            }
            if (result == kIdDuplicate)
            {
                // G-6: kick off the duplicate flow.  Default name = "<base>
                // Duplicate" (extension preserved automatically by the
                // helper).  Rename path is recursive - re-prompts with the
                // user's last value.
                if (absPath.isNotEmpty())
                {
                    const String baseName = File (absPath).getFileNameWithoutExtension();
                    runAudioDuplicateFlow (absPath, baseName + " Duplicate");
                }
                return;
            }
            if (result == kIdReveal)
            {
                if (absPath.isNotEmpty())
                {
                    File f (absPath);
                    if (f.existsAsFile())
                        f.revealToUser();
                    else if (f.getParentDirectory().exists())
                        f.getParentDirectory().revealToUser();   // fall back to folder
                }
                return;
            }
            if (result == kIdProperties)
            {
                showLibraryPropertiesDialog (libIdx);
                return;
            }
            if (result >= kIdChokeBase && result <= kIdChokeBase + 16)
            {
                performLibraryOp ("Choke Group", /*withBlocks*/ false,
                                  [this, libIdx, result]
                                  { mPM.setAudioLibraryChokeGroup (libIdx, result - kIdChokeBase); });
                return;
            }
            if (result == kIdDelete)
            {
                confirmAndDeleteLibraryEntry (libIdx);
                return;
            }
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-E Task 5 (2026-05-15): browser Delete -> confirmation prompt + cascade.
// See header.  Shared between tree (showAudioTreeContextMenu) + flat-list
// right-click handlers so both deletion entry points behave identically.
// ─────────────────────────────────────────────────────────────────────────────
void BrowserPanel::confirmAndDeleteLibraryEntry (int libIdx)
{
    if (libIdx < 0 || libIdx >= mPM.getNumAudioLibrary()) return;

    const String path       = mPM.getAudioLibraryPath (libIdx);
    const int    owner      = mPM.getAudioLibraryPageOwner (libIdx);
    const String fileName   = juce::File (path).getFileName();
    // owner == 0 means "generic Audio category" (master capture / untagged
    // drops) -- no owning page to close.
    const int    ownerCount = (owner != 0)
                                ? mPM.countAudioLibraryEntriesForChannel (owner)
                                : 0;
    const bool   isLastOnPage = (owner != 0) && (ownerCount <= 1);

    int blockCount = 0;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
        if (mPM.getBlock (i).clipType == ClipType::Audio
            && mPM.getBlock (i).audioFilePath == path
            && mPM.getBlock (i).routeChannel  == owner)
            ++blockCount;

    juce::String msg = "Delete \"" + fileName + "\" from the library?\n\n";
    if (isLastOnPage)
        msg += "This is the last file routed through its owning page.  The page (and "
             + juce::String (blockCount) + " grid block"
             + (blockCount == 1 ? "" : "s") + " using it) will also be removed.";
    else if (blockCount > 0)
        msg += juce::String (blockCount) + " grid block"
             + (blockCount == 1 ? "" : "s") + " using it will also be removed.";
    else
        msg += "No grid blocks reference it; only the library entry will be removed.";

    auto* aw = new juce::AlertWindow (
        "Delete from Library",
        msg,
        juce::AlertWindow::QuestionIcon);
    aw->addButton ("Delete", 1);
    aw->addButton ("Cancel", 0);

    juce::Component::SafePointer<BrowserPanel> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, libIdx, path, owner, isLastOnPage] (int r)
        {
            if (r != 1) return;
            if (! safeThis) return;
            auto* self = safeThis.getComponent();
            if (! self) return;
            auto& pm = self->mPM;

            self->performLibraryOp ("Delete Audio", /*withBlocks*/ true, [&] {
                // Cascade-remove blocks matching (path, owner).  Blocks routed
                // to a DIFFERENT page (different routeChannel) stay.
                for (int i = pm.getNumBlocks() - 1; i >= 0; --i)
                    if (pm.getBlock (i).clipType == ClipType::Audio
                        && pm.getBlock (i).audioFilePath == path
                        && pm.getBlock (i).routeChannel  == owner)
                        pm.removeBlock (i);

                // Remove the specific library entry by index (precise -- post-
                // Task-5 schema may have multiple entries with same path under
                // different page owners).
                pm.removeAudioFromLibraryAt (libIdx);
            });

            self->rebuildAudioRows();
            if (self->onArrangementChanged) self->onArrangementChanged();

            // Last file on the page -> close the owning Clips / Vox / Inst tab.
            if (isLastOnPage && self->onClosePageForChannelId)
                self->onClosePageForChannelId (owner);
        }),
        true);
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-E Task 7 (FILE-02): browser-entry "Properties..." -> the SAME full Audio
// Properties box as the per-clip grid dialog (Pitch / BPM / Mode + Routes to:),
// built via the shared buildAudioPropsControls helper so the two never drift.
// The library entry is the SOURCE OF TRUTH: applying writes the values onto
// the entry AND propagates them to every grid copy still following the
// original (ArrangementBlock::isOverride == false).  Per-copy customization is
// done instead via the grid clip's own Properties dialog (one combined flag).
// ─────────────────────────────────────────────────────────────────────────────
void BrowserPanel::showLibraryPropertiesDialog (int libIdx)
{
    if (libIdx < 0 || libIdx >= mPM.getNumAudioLibrary()) return;

    const juce::String libPath  = mPM.getAudioLibraryPath (libIdx);
    const juce::String fileName = juce::File (libPath).getFileName();
    const int          curOwner = mPM.getAudioLibraryPageOwner (libIdx);

    juce::String curRouteName;
    std::vector<RoutablePageInfo> pages;
    if (onEnumerateRoutablePages) pages = onEnumerateRoutablePages();
    for (const auto& pg : pages)
        if (pg.channelId == curOwner) { curRouteName = pg.displayName; break; }

    auto* aw = new juce::AlertWindow (
        "Audio File Properties",
        "File: " + fileName + "\n\n"
        "Master settings for this file.  Every copy on the Builder grid\n"
        "follows these, except copies you customized individually.",
        juce::AlertWindow::NoIcon);

    std::shared_ptr<juce::TextButton> routeBtn;
    std::shared_ptr<PendingRoute>     pending;
    // Browser dialog: offerMove == true -> each target gets a Move/Copy
    // submenu.  Move relocates this single entry; Copy forks a renamed file.
    buildAudioPropsControls (*aw,
                             mPM.getAudioLibraryPitch (libIdx),
                             mPM.getAudioLibraryBPM (libIdx),
                             mPM.getAudioLibraryStretchMode (libIdx),
                             curRouteName, pages,
                             /*offerMove*/ true,
                             /*offerResetToMaster*/ false,   // browser edits the master itself
                             routeBtn, pending);

    juce::Component::SafePointer<BrowserPanel> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, libIdx, libPath, curOwner, routeBtn, pending] (int r)
        {
            if (r != 1) return;
            auto* self = safeThis.getComponent();
            if (! self) return;

            const float newPitch = aw->getTextEditorContents ("pitch").getFloatValue();
            const float newBPM   = juce::jmax (1.f,
                                     aw->getTextEditorContents ("bpm").getFloatValue());
            bool        stretch  = true;
            if (auto* cb = aw->getComboBoxComponent ("mode"))
                stretch = (cb->getSelectedItemIndex() == 0);

            // No routing pick -> just update the entry's master props +
            // propagate to followers; owner unchanged.
            if (! pending || ! pending->chosen)
            {
                if (self->onApplyLibraryProperties)
                    self->onApplyLibraryProperties (libIdx, newPitch, newBPM,
                                                    stretch, curOwner);
                return;
            }

            if (pending->isCopy)
            {
                // Copy: duplicate FIRST, then create the new page bound to
                // the DUPLICATE (so "Copy to a new Clip Page" registers only
                // the one dupe entry), then tag it (dedup-safe).  The
                // original entry is untouched.
                if (! self->onDuplicateFileForCopy) return;
                const juce::String np = self->onDuplicateFileForCopy (libPath);
                if (np.isEmpty()) return;
                int target = pending->channelId;
                if (pending->createKind >= 0 && self->onCreateRoutablePage)
                    target = self->onCreateRoutablePage (pending->createKind, np);
                if (target < 0) return;
                if (self->onTagCopiedEntry)
                    self->onTagCopiedEntry (np, target, newPitch, newBPM, stretch);
                // QA-Fe2: offer the destination page's groups for the copy.
                const int newIdx = self->mPM.findAudioLibraryIndexByPath (np);
                if (newIdx >= 0)
                    self->maybePromptGroupAssign (newIdx, target);
            }
            else
            {
                // Move: relocate THIS entry's owner + props; following grid
                // blocks follow (onApplyLibraryProperties propagation).
                // (Move to a new page still creates it bound to the existing
                // file.)
                int target = pending->channelId;
                if (pending->createKind >= 0 && self->onCreateRoutablePage)
                    target = self->onCreateRoutablePage (pending->createKind, libPath);
                if (target < 0) return;
                if (self->onApplyLibraryProperties)
                    self->onApplyLibraryProperties (libIdx, newPitch, newBPM,
                                                    stretch, target);
                self->maybePromptGroupAssign (libIdx, target);   // QA-Fe2
            }
        }),
        true);
}

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): Duplicate... right-click flow on Audio tree leaves.
// Recursive on Rename.  Self-contained: shows name prompt, resolves filename
// conflicts (Overwrite / Cancel / Rename re-prompt), physically copies the
// WAV, then hands BOTH the source path AND the new file's absolute path to
// the editor's onDuplicateClipSpawn callback so StandaloneEditor can spawn
// a new ClipsPage on the copy AND clone the source page's full state
// (engine choice, all knob values, both engines' APVTS state).
// ─────────────────────────────────────────────────────────────────────────────
void BrowserPanel::runAudioDuplicateFlow (const juce::String& sourceAbsPath,
                                          const juce::String& defaultName)
{
    File source (sourceAbsPath);
    if (! source.existsAsFile()) return;

    const String ext = source.getFileExtension();   // e.g. ".wav"

    // Name prompt (juce::AlertWindow with single text editor row).  Modal
    // pattern matches RibbonTabBar::startRename - deleteWhenDismissed = true
    // so the AlertWindow deletes itself once the callback fires.
    auto* aw = new juce::AlertWindow ("Duplicate Clip",
                                       "Enter a name for the duplicate:",
                                       juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", defaultName);
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<BrowserPanel> self (this);
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [self, sourceAbsPath, ext, aw] (int result)
            {
                if (! self || result != 1) return;

                const String typedName = aw->getTextEditorContents ("name").trim();
                if (typedName.isEmpty()) return;

                // Build destination path: same folder as source, typed name,
                // preserve extension.  Strip any extension the user typed
                // (we re-append the source extension).
                File source (sourceAbsPath);
                File destDir = source.getParentDirectory();
                String stem = typedName;
                if (stem.endsWithIgnoreCase (ext)) stem = stem.dropLastCharacters (ext.length());
                File dest = destDir.getChildFile (stem + ext);

                if (dest == source) return;   // same file - no-op

                if (dest.existsAsFile())
                {
                    // G-7 (2026-04-29): no-file-delete contract - Overwrite
                    // dropped.  User can either pick a new name or cancel.
                    // Prevents BaySickDAW from ever deleting user audio files.
                    auto* conflict = new juce::AlertWindow (
                        "File Exists",
                        "A file named \"" + dest.getFileName() + "\" already exists.\n"
                        "Choose a different name or cancel.",
                        juce::MessageBoxIconType::WarningIcon);
                    conflict->addButton ("Rename...", 2);
                    conflict->addButton ("Cancel",    0);

                    juce::Component::SafePointer<BrowserPanel> self2 (self);
                    conflict->enterModalState (true,
                        juce::ModalCallbackFunction::create (
                            [self2, sourceAbsPath, stem] (int r)
                            {
                                if (! self2) return;
                                if (r == 2)
                                {
                                    // Rename: recurse with the user's last typed name.
                                    self2->runAudioDuplicateFlow (sourceAbsPath, stem);
                                }
                                // r == 0 → cancel, do nothing.
                            }),
                        true);
                    return;
                }

                // No conflict - copy and spawn.
                if (source.copyFileTo (dest))
                {
                    if (self->onDuplicateClipSpawn)
                        self->onDuplicateClipSpawn (sourceAbsPath, dest.getFullPathName());
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon,
                        "Duplicate Failed",
                        "Could not copy the file to \"" + dest.getFileName() + "\".  "
                        "Check disk space and folder permissions.");
                }
            }),
        true);   // deleteWhenDismissed: AlertWindow self-deletes after callback
}

void BrowserPanel::rebuildAutomationRows()
{
    for (auto& r : mAutomItems) removeChildComponent(r.get());
    mAutomItems.clear();
    mAutomBlockIndices.clear();

    // Drive the list from the persistent automation template library so
    // deleting blocks doesn't wipe Browser entries.
    for (int i = 0; i < mPM.getNumAutomationTemplates(); ++i)
    {
        const auto& tpl = mPM.getAutomationTemplate(i);
        // Prefer the display-name resolver when wired (honours userDisplayName
        // + current rack state). Fall back to raw paramId, then "(unnamed)".
        String displayLabel;
        if (onResolveDisplayName) displayLabel = onResolveDisplayName(tpl);
        if (displayLabel.isEmpty()) displayLabel = tpl.paramId;
        if (displayLabel.isEmpty()) displayLabel = "(unnamed)";

        const int idx = (int)mAutomItems.size();
        mAutomBlockIndices.push_back(i);   // index into template library
        auto item = std::make_unique<BrowserItem>(BrowserItem::Kind::Automation, idx, displayLabel);
        item->setAccentColour(kAutomCol);
        BrowserItem* raw = item.get();
        raw->onRenameRequested = [this, raw] { openRenamePopup(*raw); };
        raw->onContextMenu     = [this, raw](Point<int> pt) { showItemContextMenu(*raw, pt); };
        raw->onClicked         = [this, idx] { selectAutomationItem (idx); };
        addAndMakeVisible(*item);
        mAutomItems.push_back(std::move(item));
    }
    resized();
}

void BrowserPanel::refresh()
{
    // Diff-based refresh: only rebuild when content actually changed.
    // Destroying and recreating items on every timer tick breaks drag
    // gestures (the BrowserItem under the user's mouse gets deleted
    // mid-drag). Hashes count + names of the visible tab so any real
    // change triggers a rebuild, but idle ticks are free.
    juce::String snapshot;
    if (mActiveTab == 0)
    {
        snapshot << "P:" << mPM.getNumPatterns() << "|";
        for (int i = 0; i < mPM.getNumPatterns(); ++i)
            snapshot << mPM.getPattern(i).name << "\n";
        snapshot << "sel=" << mSelectedPat;
    }
    else if (mActiveTab == 1)
    {
        // G-5 (2026-04-29): tree is page-driven so the snapshot must include
        // page enumeration too - otherwise add/remove/rename of a Clips/
        // Vox/Inst page wouldn't trigger a tree rebuild.
        snapshot << "A:" << mPM.getNumAudioLibrary() << "|";
        for (int i = 0; i < mPM.getNumAudioLibrary(); ++i)
            snapshot << mPM.getAudioLibraryPath(i) << "/" << mPM.getAudioLibraryAlias(i)
                     << "/" << mPM.getAudioLibraryGroup(i) << "\n";   // QA-Fe2: group edits rebuild too
        for (int c = 0; c < 3; ++c)                                   // QA-Fe2: manual-group registry
            snapshot << "G" << c << ":" << mPM.getManualAudioGroups(c).joinIntoString(",") << "|";
        if (onEnumerateAudio)
        {
            auto entries = onEnumerateAudio();
            snapshot << "P:" << entries.size() << "|";
            for (auto& e : entries)
                snapshot << e.category << ":" << e.audioLibIdx << ":" << e.displayName << "|";
        }
    }
    else
    {
        snapshot << "T:" << mPM.getNumAutomationTemplates() << "|";
        for (int i = 0; i < mPM.getNumAutomationTemplates(); ++i)
        {
            const auto& tpl = mPM.getAutomationTemplate(i);
            // Include user rename + auto-resolved display label in the snapshot
            // so effect-swap inside a slot (or a user rename) triggers a visible
            // refresh rather than being masked by paramId-only hashing.
            snapshot << tpl.paramId
                     << "|u=" << tpl.userDisplayName;
            if (onResolveDisplayName)
                snapshot << "|d=" << onResolveDisplayName(tpl);
            snapshot << "\n";
        }
    }

    if (snapshot == mLastRefreshSnapshot) return;  // nothing changed
    mLastRefreshSnapshot = snapshot;

    if      (mActiveTab == 0) rebuildPatternRows();
    else if (mActiveTab == 1) rebuildAudioRows();
    else                      rebuildAutomationRows();
}

void BrowserPanel::refreshAutomationTab()
{
    rebuildAutomationRows();
}

void BrowserPanel::refreshPatternTab()
{
    rebuildPatternRows();
}

void BrowserPanel::refreshRenderRows()
{
    rebuildRenderRows();
}

// ── QA-UndoCoverage Task 4: browser gesture wrappers ─────────────────────────
void BrowserPanel::performPatternSliceOp (const juce::String& label,
                                          const std::function<void()>& op)
{
    if (! (mUndoCtx.isValid() && onCapturePatternSlice && onApplyPatternSlice))
    { op(); return; }

    PatternListSnapshot before = onCapturePatternSlice();
    op();
    PatternListSnapshot after  = onCapturePatternSlice();
    mUndoCtx.perform (new PatternListAction (std::move (before), std::move (after),
                          [fn = onApplyPatternSlice] (const PatternListSnapshot& s) { fn (s); }),
                      label);
}

void BrowserPanel::performLibraryOp (const juce::String& label, bool withBlocks,
                                     const std::function<void()>& op)
{
    if (! mUndoCtx.isValid()) { op(); return; }

    auto capture = [this, withBlocks]() -> AudioLibrarySnapshot
    {
        AudioLibrarySnapshot s;
        s.entries        = mPM.getAudioLibraryEntries();
        s.manualGroups   = mPM.getManualAudioGroupsRaw();
        s.includesBlocks = withBlocks;
        if (withBlocks)
            for (int i = 0; i < mPM.getNumBlocks(); ++i) s.blocks.push_back (mPM.getBlock (i));
        return s;
    };

    AudioLibrarySnapshot before = capture();
    op();
    AudioLibrarySnapshot after = capture();

    auto apply = [sp = juce::Component::SafePointer<BrowserPanel> (this)]
                 (const AudioLibrarySnapshot& s)
    {
        if (sp == nullptr) return;
        auto& pm = sp->mPM;
        pm.restoreAudioLibrary (s.entries, s.manualGroups);
        if (s.includesBlocks)
        {
            while (pm.getNumBlocks() > 0) pm.removeBlock (0);
            for (const auto& b : s.blocks) pm.addBlock (b);
        }
        sp->rebuildAudioRows();
        if (s.includesBlocks && sp->onArrangementChanged) sp->onArrangementChanged();
    };
    mUndoCtx.perform (new AudioLibraryAction (std::move (before), std::move (after),
                                              std::move (apply)),
                      label);
}

void BrowserPanel::performTemplateOp (const juce::String& label, bool withBlocks,
                                      const std::function<void()>& op)
{
    if (! mUndoCtx.isValid()) { op(); return; }

    auto capture = [this, withBlocks]() -> AutomationTemplateSnapshot
    {
        AutomationTemplateSnapshot s;
        s.templates      = mPM.getAutomationTemplatesRaw();
        s.includesBlocks = withBlocks;
        if (withBlocks)
            for (int i = 0; i < mPM.getNumBlocks(); ++i) s.blocks.push_back (mPM.getBlock (i));
        return s;
    };

    AutomationTemplateSnapshot before = capture();
    op();
    AutomationTemplateSnapshot after = capture();

    auto apply = [sp = juce::Component::SafePointer<BrowserPanel> (this)]
                 (const AutomationTemplateSnapshot& s)
    {
        if (sp == nullptr) return;
        auto& pm = sp->mPM;
        pm.restoreAutomationTemplates (s.templates);
        if (s.includesBlocks)
        {
            while (pm.getNumBlocks() > 0) pm.removeBlock (0);
            for (const auto& b : s.blocks) pm.addBlock (b);
        }
        sp->rebuildAutomationRows();
        if (s.includesBlocks && sp->onArrangementChanged) sp->onArrangementChanged();
    };
    mUndoCtx.perform (new AutomationTemplateAction (std::move (before), std::move (after),
                                                    std::move (apply)),
                      label);
}

// ── Rename popup (CallOutBox with TextEditor) ───────────────────────────────
void BrowserPanel::openRenamePopup(BrowserItem& item)
{
    auto editor = std::make_unique<TextEditor>();
    editor->setText(item.getDisplayName(), false);
    editor->setFont(Font(13.f));
    editor->setSelectAllWhenFocused(true);
    editor->setSize(180, 26);
    editor->setEscapeAndReturnKeysConsumed(true);

    const auto kind = item.getKind();
    const int  idx  = item.getIndex();
    auto* rawEdit   = editor.get();
    editor->onReturnKey = [this, kind, idx, rawEdit] {
        const String t = rawEdit->getText().trim();
        if (t.isNotEmpty())
        {
            switch (kind)
            {
                case BrowserItem::Kind::Pattern:    renamePatternAt   (idx, t); break;
                case BrowserItem::Kind::Audio:      renameAudioAt     (idx, t); break;
                case BrowserItem::Kind::Automation: renameAutomationAt(idx, t); break;
            }
        }
        if (auto* cb = rawEdit->findParentComponentOfClass<CallOutBox>())
            cb->dismiss();
    };
    editor->onEscapeKey = [rawEdit] {
        if (auto* cb = rawEdit->findParentComponentOfClass<CallOutBox>())
            cb->dismiss();
    };

    Rectangle<int> screenAnchor = item.getScreenBounds();
    CallOutBox::launchAsynchronously(std::move(editor), screenAnchor, nullptr);
}

void BrowserPanel::showItemContextMenu(BrowserItem& item, Point<int> /*globalPt*/)
{
    const auto kind = item.getKind();
    const int  idx  = item.getIndex();
    BrowserItem* raw = &item;

    // D3: choke-group submenu IDs for audio items: 200 = None, 201..216 = groups 1..16.
    constexpr int kIdChokeBase = 200;

    PopupMenu m;
    m.addItem(1, "Rename...");
    if (kind == BrowserItem::Kind::Pattern)
    {
        m.addItem(4, "Duplicate");
        m.addItem(6, "Change Color...");   // F-1 (2026-04-26)
    }
    // Revert-to-auto only makes sense for Automation items that have a rename.
    bool showRevert = false;
    if (kind == BrowserItem::Kind::Automation
        && idx >= 0 && idx < (int) mAutomBlockIndices.size())
    {
        const int tplIdx = mAutomBlockIndices[idx];
        if (tplIdx >= 0 && tplIdx < mPM.getNumAutomationTemplates()
            && mPM.getAutomationTemplate(tplIdx).userDisplayName.isNotEmpty())
        {
            showRevert = true;
            m.addItem(5, "Revert to auto name");
        }
    }

    // D3: Choke Group submenu - audio items only (per-clip, persisted in the
    // audio library entry).  Synth choke uses the per-tab context menu.
    if (kind == BrowserItem::Kind::Audio)
    {
        m.addSeparator();
        const int curGroup = mPM.getAudioLibraryChokeGroup(idx);
        PopupMenu chokeSub;
        chokeSub.addItem(kIdChokeBase, "None", true, curGroup == 0);
        for (int g = 1; g <= 16; ++g)
            chokeSub.addItem(kIdChokeBase + g, "Group " + String(g),
                             true, curGroup == g);
        m.addSubMenu("Choke Group", chokeSub);
    }

    m.addSeparator();
    if (kind == BrowserItem::Kind::Pattern) {
        m.addItem(2, "Render to WAV...");
        m.addItem(7, "Split by Player Engine...");   // QA-G (owner docket 5)
        m.addSeparator();
    }
    m.addItem(3, "Delete");

    m.showMenuAsync(PopupMenu::Options().withTargetComponent(raw),
        [this, kind, idx, raw, kIdChokeBase](int result)
        {
            if (result == 1) { openRenamePopup(*raw); return; }
            // D3: choke-group submenu (200..216 → 0..16) for audio items.
            if (kind == BrowserItem::Kind::Audio
                && result >= kIdChokeBase && result <= kIdChokeBase + 16)
            {
                performLibraryOp ("Choke Group", /*withBlocks*/ false,
                                  [this, idx, result]
                                  { mPM.setAudioLibraryChokeGroup(idx, result - kIdChokeBase); });
                return;
            }
            if (result == 5 && kind == BrowserItem::Kind::Automation)
            {
                // Clear the user rename; auto-resolver takes over again.
                renameAutomationAt(idx, String());
                return;
            }
            if (result == 2 && kind == BrowserItem::Kind::Pattern)
            {
                if (onRenderPattern) onRenderPattern(idx);
                return;
            }
            if (result == 7 && kind == BrowserItem::Kind::Pattern)
            {
                if (onSplitPattern) onSplitPattern(idx);
                return;
            }
            if (result == 6 && kind == BrowserItem::Kind::Pattern)
            {
                // F-1 (2026-04-26): live-preview colour picker.
                if (idx < 0 || idx >= mPM.getNumPatterns()) return;
                const juce::Colour curCol = mPM.getPattern(idx).color;
                PatternColorPicker::showAsync (raw, curCol,
                    [this, idx, curCol] (juce::Colour newCol)
                    {
                        if (idx < 0 || idx >= mPM.getNumPatterns()) return;
                        mPM.getPattern(idx).color = newCol;
                        rebuildPatternRows();
                        repaint();
                        if (mUndoCtx.isValid() && newCol != curCol)
                            mUndoCtx.perform (new PatternColorAction (idx, curCol, newCol,
                                [sp = juce::Component::SafePointer<BrowserPanel> (this)]
                                (int i, juce::Colour c)
                                {
                                    if (sp == nullptr) return;
                                    if (i < 0 || i >= sp->mPM.getNumPatterns()) return;
                                    sp->mPM.getPattern(i).color = c;
                                    sp->rebuildPatternRows();
                                    sp->repaint();
                                }),
                              "Pattern Color");
                    });
                return;
            }
            if (result == 4 && kind == BrowserItem::Kind::Pattern)
            {
                int newIdx = -1;
                performPatternSliceOp ("Duplicate Pattern", [this, idx, &newIdx] {
                    newIdx = mPM.duplicatePattern(idx);
                    if (newIdx >= 0)
                    {
                        mSelectedPat = newIdx;
                        mPM.setCurrentPattern(newIdx);
                    }
                });
                if (newIdx >= 0)
                {
                    rebuildPatternRows();
                    if (onPatternSelected) onPatternSelected(newIdx);
                }
                return;
            }
            if (result == 3)
            {
                switch (kind)
                {
                case BrowserItem::Kind::Pattern:
                    if (mPM.getNumPatterns() > 1)
                    {
                        performPatternSliceOp ("Delete Pattern", [this, idx] {
                            mPM.removePattern(idx);
                            mSelectedPat = jlimit(0, mPM.getNumPatterns() - 1, mSelectedPat);
                            mPM.setCurrentPattern(mSelectedPat);
                        });
                        rebuildPatternRows();
                        if (onPatternSelected) onPatternSelected(mSelectedPat);
                    }
                    break;
                case BrowserItem::Kind::Audio:
                    if (idx < mAudioPaths.size())
                    {
                        // QA-E Task 5 (2026-05-15): convert flat-list idx to
                        // library index by path lookup, then route through
                        // the shared prompt+cascade helper.  findByPath
                        // returns the FIRST matching entry; if multiple
                        // entries share this path under different page
                        // owners, only the first gets removed per Delete
                        // click (V1 acceptable since the tree right-click
                        // path uses precise libIdx).
                        const int libIdx = mPM.findAudioLibraryIndexByPath (mAudioPaths[idx]);
                        if (libIdx >= 0)
                            confirmAndDeleteLibraryEntry (libIdx);
                    }
                    break;
                case BrowserItem::Kind::Automation:
                    if (idx < (int)mAutomBlockIndices.size())
                    {
                        const int tplIdx = mAutomBlockIndices[idx];
                        if (tplIdx >= 0 && tplIdx < mPM.getNumAutomationTemplates())
                        {
                            performTemplateOp ("Delete Automation", /*withBlocks*/ true,
                                               [this, tplIdx] {
                                // Cascade-remove any blocks created from this
                                // template (matched by paramId).
                                const String pid = mPM.getAutomationTemplate(tplIdx).paramId;
                                for (int i = mPM.getNumBlocks() - 1; i >= 0; --i)
                                    if (mPM.getBlock(i).clipType == ClipType::Automation
                                        && mPM.getBlock(i).automationLane.paramId == pid)
                                        mPM.removeBlock(i);
                                mPM.removeAutomationTemplate(tplIdx);
                            });
                        }
                        rebuildAutomationRows();
                        if (onArrangementChanged) onArrangementChanged();
                    }
                    break;
                }
            }
        });
}

// 2026-04-21: auto-suffix " (2)", " (3)", ... if a candidate rename duplicates
//   any existing name at an index other than the one being renamed. Applies to
//   patterns, audio clips, and automation lanes so the Automate menu + other
//   lists never show ambiguous duplicate labels.
static juce::String ensureUniqueBrowserName (const juce::String& candidate,
                                              int skipIdx,
                                              int count,
                                              std::function<juce::String(int)> nameAt)
{
    auto isDup = [&] (const juce::String& name) -> bool
    {
        for (int i = 0; i < count; ++i)
        {
            if (i == skipIdx) continue;
            if (nameAt(i) == name) return true;
        }
        return false;
    };
    if (candidate.isEmpty() || ! isDup(candidate)) return candidate;
    int n = 2;
    juce::String alt;
    do { alt = candidate + " (" + juce::String(n++) + ")"; } while (isDup(alt));
    return alt;
}

void BrowserPanel::renamePatternAt(int idx, const String& newName)
{
    if (idx < 0 || idx >= mPM.getNumPatterns()) return;
    const juce::String finalName = ensureUniqueBrowserName (
        newName, idx, mPM.getNumPatterns(),
        [this] (int i) { return mPM.getPattern(i).name; });
    const juce::String oldName = mPM.getPattern(idx).name;
    mPM.renamePattern(idx, finalName);
    rebuildPatternRows();
    if (mUndoCtx.isValid() && oldName != finalName)
        mUndoCtx.perform (new PatternRenameAction (idx, oldName, finalName,
                              [sp = juce::Component::SafePointer<BrowserPanel> (this)]
                              (int i, const juce::String& n)
                              {
                                  if (sp == nullptr) return;
                                  sp->mPM.renamePattern (i, n);
                                  sp->rebuildPatternRows();
                              }),
                          "Rename Pattern");
}

void BrowserPanel::renameAudioAt(int idx, const String& newName)
{
    if (idx < 0 || idx >= mAudioPaths.size()) return;
    // mAudioPaths is populated in audioLibrary index order (rebuildAudioRows),
    // so idx IS the library index.  The old path->index first-match re-resolve
    // collapsed same-path entries onto the first one -- renaming the wrong
    // entry whenever a file exists under multiple page owners (FILE-03).
    const int libIdx = (idx < mPM.getNumAudioLibrary()) ? idx : -1;
    const String path  = mAudioPaths[idx];
    const int    owner = (libIdx >= 0) ? mPM.getAudioLibraryPageOwner (libIdx) : 0;
    const juce::String finalName = ensureUniqueBrowserName (
        newName, libIdx, mPM.getNumAudioLibrary(),
        [this] (int i) { return mPM.getAudioLibraryAlias(i); });
    performLibraryOp ("Rename Audio", /*withBlocks*/ true, [&] {
        // Persist the alias in the library so it survives block deletion.
        if (libIdx >= 0) mPM.setAudioLibraryAlias(libIdx, finalName);
        // Also stamp every currently-placed block so the clip title matches.
        // (path, owner) match, not path-only -- same-path entries on OTHER pages
        // keep their own alias (the delete cascade's keying, FILE-03).
        for (int i = 0; i < mPM.getNumBlocks(); ++i)
        {
            auto& bb = mPM.getBlock(i);
            if (bb.clipType == ClipType::Audio && bb.audioFilePath == path
                && bb.routeChannel == owner)
                bb.displayAlias = finalName;
        }
    });
    rebuildAudioRows();
    if (onArrangementChanged) onArrangementChanged();
}

void BrowserPanel::renameAutomationAt(int idx, const String& newName)
{
    if (idx < 0 || idx >= (int)mAutomBlockIndices.size()) return;
    const int tplIdx = mAutomBlockIndices[idx];
    if (tplIdx < 0 || tplIdx >= mPM.getNumAutomationTemplates()) return;
    // Display-only rename: write to `userDisplayName`, not `paramId`.
    // `paramId` stays stable so the automation applicator lookup and the
    // "Channel - Effect - Param" auto-resolver keep working. If newName is
    // blank, treat as a "revert to auto" gesture and clear the override.
    String trimmed = newName.trim();
    // 2026-04-21: auto-suffix duplicate automation names so the Automate menu
    //   never shows two lanes with the same label. Skip if blank (clears override).
    //
    // Bug fix 2026-04-21: compare against each other template's EFFECTIVE DISPLAY
    //   NAME (userDisplayName if set, else resolver output), not just the raw
    //   userDisplayName field - otherwise typing "Bass" onto template A would
    //   sail through the check because template B's userDisplayName is empty,
    //   but B's visible browser label is "Bass 1 - Harmless - …" (which the
    //   user might then try to duplicate-rename, producing a real collision).
    if (trimmed.isNotEmpty())
    {
        trimmed = ensureUniqueBrowserName (
            trimmed, tplIdx, mPM.getNumAutomationTemplates(),
            [this] (int i) -> juce::String
            {
                const auto& other = mPM.getAutomationTemplate(i);
                if (other.userDisplayName.isNotEmpty())    return other.userDisplayName;
                if (onResolveDisplayName)                   return onResolveDisplayName(other);
                return other.paramId;
            });
    }
    performTemplateOp ("Rename Automation", /*withBlocks*/ true, [&] {
        mPM.setAutomationTemplateUserName(tplIdx, trimmed);
        // Propagate the user-rename onto any already-placed blocks that share
        // this paramId so the grid label + Event Editor title update immediately.
        const String pid = mPM.getAutomationTemplate(tplIdx).paramId;
        for (int i = 0; i < mPM.getNumBlocks(); ++i)
        {
            auto& bb = mPM.getBlock(i);
            if (bb.clipType == ClipType::Automation && bb.automationLane.paramId == pid)
                bb.automationLane.userDisplayName = trimmed;
        }
    });
    rebuildAutomationRows();
}

void BrowserPanel::paint(Graphics& g)
{
    g.fillAll(kBrowserBg);
    g.setColour(VC::Accent.withAlpha(0.6f));
    g.fillRect(getWidth() - 1, 0, 1, getHeight());

    // Jeff 2026-08-06: collapsed = width zero (the divider is the handle) --
    // nothing to paint.
    if (mCollapsed)
        return;

    g.setColour(VC::TextDim);
    g.setFont(Font(9.f, Font::bold));
    g.drawText("SOURCE PICKER", 4, 4, getWidth() - 8, 12, Justification::centredLeft);

    g.setColour(VC::Highlight.withAlpha(0.7f));
    g.fillRect(0, 56, getWidth(), 1);

    if (mActiveTab == 2 && mAutomItems.empty()) {
        g.setColour(VC::TextDim.withAlpha(0.6f)); g.setFont(Font(10.f));
        g.drawText("No automation clips", 0, getHeight()/2-10, getWidth(), 20, Justification::centred);
        g.setFont(Font(8.f));
        g.drawText("View > New Automation Clip", 0, getHeight()/2+10, getWidth(), 16, Justification::centred);
    }
}

void BrowserPanel::resized()
{
    auto b = getLocalBounds();
    if (mCollapsed) return;

    // T16: the 22px row the "<<" button occupied is reclaimed -- the header
    // caption still paints over it, so the tabs start below that band.
    b.removeFromTop(22);

    auto tabRow = b.removeFromTop(22).reduced(2, 1);
    int  tabW   = tabRow.getWidth() / 3;
    for (int t = 0; t < 3; ++t)
        mTabBtns[t]->setBounds(tabRow.removeFromLeft(tabW).reduced(1));
    b.removeFromTop(2);

    // TS7: Sort sits on its own row under the tabs, visible on Files only.  Its
    // height is reclaimed when hidden so the tree does not lose a strip on the
    // other two tabs.
    if (mSortBtn && mSortBtn->isVisible())
    {
        mSortBtn->setBounds (b.removeFromTop (20).removeFromLeft (72).reduced (2, 1));
        b.removeFromTop (2);
    }

    auto layoutItems = [&b](std::vector<std::unique_ptr<BrowserItem>>& items)
    {
        for (auto& it : items)
        {
            auto row = b.removeFromTop(26).reduced(3, 1);
            it->setVisible(true);
            it->setBounds(row);
        }
    };

    auto hideItems = [](std::vector<std::unique_ptr<BrowserItem>>& items)
    {
        for (auto& it : items) it->setVisible(false);
    };

    if (mActiveTab == 0) {
        mAddBtn->setVisible(true); mDeleteBtn->setVisible(true);
        auto btnRow = b.removeFromTop(24).reduced(2, 1);
        mAddBtn   ->setBounds(btnRow.removeFromLeft(btnRow.getWidth()/2).reduced(1));
        mDeleteBtn->setBounds(btnRow.reduced(1));
        b.removeFromTop(2);
        layoutItems(mPatItems);
        hideItems(mAudioItems);
        hideItems(mAutomItems);
    } else if (mActiveTab == 1) {
        // G-5 (2026-04-29): Audio tab uses the unified tree.  Hide flat-list
        // items (legacy storage, kept empty post-tree-migration) + give the
        // tree the remaining vertical space.
        mAddBtn->setVisible(false); mDeleteBtn->setVisible(false);
        hideItems(mAudioItems);
        hideItems(mPatItems);
        hideItems(mAutomItems);
        if (mAudioTree)
            mAudioTree->setBounds (b.reduced (3, 1));
    } else {
        mAddBtn->setVisible(false); mDeleteBtn->setVisible(false);
        layoutItems(mAutomItems);
        hideItems(mPatItems);
        hideItems(mAudioItems);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ArrangementGrid - constructor
// ─────────────────────────────────────────────────────────────────────────────
ArrangementGrid::ArrangementGrid(PatternManager& pm,
                                 AudioFormatManager& afm,
                                 AudioThumbnailCache& thumbCache)
    : mPM(pm), mAFM(afm), mThumbCache(thumbCache)
{
    setWantsKeyboardFocus(true);
}

ArrangementGrid::~ArrangementGrid() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Coordinate helpers  (no kLabelW offset - labels are external)
// ─────────────────────────────────────────────────────────────────────────────
// #30 builder half (QA-G3Smoke): pixel-center mapping, mirroring the roll's
// LDT-394 fix -- barToX rounds (C-truncation biased lines a pixel early) and
// xToBar samples the CENTER of pixel column x, so a click just left of a grid
// line no longer snaps to the previous line.
int ArrangementGrid::barToX(float bar) const
{
    return (int) std::lround ((bar - mBarOff) * mPPBar);
}

float ArrangementGrid::xToBar(int x) const
{
    return mBarOff + ((float)x + 0.5f) / mPPBar;
}

float ArrangementGrid::xToBarF(float x) const
{
    return mBarOff + x / mPPBar;
}

int ArrangementGrid::rowToY(int row) const
{
    return kRulerH + (int)(row * mEffectiveRowH);
}

int ArrangementGrid::yToRow(int y) const
{
    // Float division to stay consistent with rowToY's float product -- int
    // row-height truncation drifts rows apart at fractional zoom levels.
    return (int) std::floor ((float)(y - kRulerH) / jmax (1.f, mEffectiveRowH));
}

int ArrangementGrid::rowHeightPx(int row) const
{
    return rowToY(row + 1) - rowToY(row);
}

// Ruler pinning: the band draws on the scrolling content surface but stays
// glued to the top of the VISIBLE area by offsetting all ruler drawing and
// ruler hit-tests by the parent Viewport's vertical scroll.  Row/block
// geometry keeps its kRulerH content inset (grid-local mappings unchanged).
int ArrangementGrid::rulerPinY() const
{
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        return jmax(0, vp->getViewPositionY());
    return 0;
}

float ArrangementGrid::snapBar(float bar) const
{
    if (mAltSnapActive) return bar;
    return snapBarAlt(bar);
}

float ArrangementGrid::snapBarAlt(float bar) const
{
    // QA-Ee Stage 2: snap to the unified tick grid.  div 0 = Off (no snap),
    // div 1 = Line (finest grid rung live at the current zoom -> FL-style
    // lock-to-view), div 2..10 = a fixed division.  Builder is uniform
    // 4-beat-per-bar, so 1 bar = 384 ticks.
    const int div = onGetSnapDiv ? onGetSnapDiv() : 1;   // default Line
    if (div == 0) return bar;                            // Off
    const int g = (div == 1) ? dynamicSnapTicks ((double) mPPBar)
                             : snapDivToTicks (div);
    if (g <= 0) return bar;
    if (g >= 384 && bar > 0.f)
    {
        // QA-G Task 6: bar-level snap targets MAP bar starts (bars resize at
        // markers); multi-bar divisions step in whole map bars.  Negative
        // (pre-roll) territory stays uniform via the generic path below.
        const int barsPer = juce::jmax (1, g / 384);
        int bi = 0; double bib = 0.0;
        mPM.beatToBarAndBeatInBar ((double) bar * 4.0, bi, bib);
        const int    b0 = (bi / barsPer) * barsPer;
        const int    b1 = b0 + barsPer;
        const double p  = (double) bar * 4.0;
        const double s0 = mPM.barStartBeat (b0);
        const double s1 = mPM.barStartBeat (b1);
        return (float) (((p - s0 <= s1 - p) ? s0 : s1) / 4.0);
    }
    const double ticks   = (double) bar * 384.0;                          // bar -> ticks
    const double snapped = std::round (ticks / (double) g) * (double) g;  // nearest grid tick
    return (float) (snapped / 384.0);                                     // ticks -> bar
}

int ArrangementGrid::totalVisibleBars() const
{
    // Content + 16 bars buffer, OR enough to cover the current scroll
    // position + viewport + 8 bars buffer (so the ruler never runs out
    // when the user scrolls past the last block).
    float vpW = 800.f;
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        vpW = (float)jmax(1, vp->getWidth());
    const int barsInViewport = (int)std::ceil(vpW / jmax(1.f, mPPBar));
    const int contentEnd     = mPM.getTotalArrangementBars();
    const int scrollEnd      = (int)mBarOff + barsInViewport;
    return jmax(32, jmax(contentEnd + 16, scrollEnd + 8));
}

// ─────────────────────────────────────────────────────────────────────────────
// Hit testing
// ─────────────────────────────────────────────────────────────────────────────
int ArrangementGrid::blockAtPos(int x, int y) const
{
    if (y < rulerPinY() + kRulerH) return -1;
    int row = yToRow(y);
    for (int i = mPM.getNumBlocks() - 1; i >= 0; --i)
    {
        const auto& b = mPM.getBlock(i);
        if (b.trackRow != row) continue;
        // QA-Ea Task 0c (2026-05-20): use effectiveStartBars so slip-edited
        // clips (possibly negative-start, sub-bar precision) hit-test on
        // their actual visible position, not the legacy int-bar startBar.
        int bx = barToX((float) effectiveStartBars(b));
        int bw = barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - bx;
        if (x >= bx && x < bx + bw) return i;
    }
    return -1;
}

bool ArrangementGrid::nearRightEdge(int blockIdx, int x) const
{
    const auto& b = mPM.getBlock(blockIdx);
    int bx = barToX((float) effectiveStartBars(b));
    int bw = barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - bx;
    return (x >= bx + bw - kResizeZone && x < bx + bw + 2);
}

// QA-Ea Task 0c (2026-05-20): how many bars of negative-bar viewport the
// project can usefully expose, based on the largest contentStartSamples in
// any Audio block.  Returns 0 when no clip has pre-roll captured (negative
// viewport access disabled in that case).  Callers use the NEGATED return
// as the lower clamp for mBarOff (e.g. mBarOff >= -maxRevealableNegativeBars()).
// BPM / SR sourced from the wired callbacks with 120 / 44100 fallbacks for
// early-init paths where callbacks haven't bound yet.
double ArrangementGrid::maxRevealableNegativeBars() const
{
    const double bpm        = onGetBPM        ? onGetBPM()        : 120.0;
    const double sampleRate = onGetSampleRate ? onGetSampleRate() : 44100.0;
    if (bpm <= 0.0 || sampleRate <= 0.0) return 0.0;

    juce::int64 maxContent = 0;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b = mPM.getBlock(i);
        if (b.clipType != ClipType::Audio) continue;
        if (b.contentStartSamples > maxContent)
            maxContent = b.contentStartSamples;
    }
    if (maxContent <= 0) return 0.0;
    // samples -> seconds -> beats -> bars (4 beats/bar at 4/4).
    const double seconds = (double) maxContent / sampleRate;
    const double beats   = seconds * bpm / 60.0;
    return beats / 4.0;
}

// QA-Ee Stage 2 (content-bound dynamic zoom): furthest-right content edge in
// bars -- the max of every clip's right edge, every time marker, and every TS
// change.  Drives the zoom-OUT minimum so the playlist expands as content grows.
float ArrangementGrid::contentMaxBars() const
{
    double maxBars = 0.0;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b = mPM.getBlock(i);
        maxBars = jmax(maxBars, effectiveStartBars(b) + effectiveLengthBars(b));
    }
    for (int i = 0; i < mPM.getNumTimeMarkers(); ++i)
        maxBars = jmax(maxBars, (double) (mPM.getTimeMarker(i).bar + 1));
    for (int i = 0; i < mPM.getNumTimeSigChanges(); ++i)
        maxBars = jmax(maxBars, (double) (mPM.getTimeSigChange(i).bar + 1));
    return (float) jmax(0.0, maxBars);
}

// Zoom-OUT minimum (px/bar).  Empty baseline = vpW / kDefaultPlaylistEmptyPx
// (monitor-dependent); grows with content + an 8-bar pad.
float ArrangementGrid::minZoomPPBar (float vpW) const
{
    const float defaultBars = vpW / kDefaultPlaylistEmptyPx;
    const float maxBars     = jmax (defaultBars, contentMaxBars() + kBuilderZoomPadBars);
    return vpW / jmax (1.f, maxBars);
}

// Zoom-IN maximum (px/bar) -- tick-level micro-editing (kMaxZoomInBeatsAcross
// beats fill the viewport at deepest zoom).
float ArrangementGrid::maxZoomPPBar (float vpW) const
{
    return vpW * 4.f / jmax (0.01f, kMaxZoomInBeatsAcross);
}

// QA-Ea Task 0c (FL pre-roll record): mirror of nearRightEdge.  Resize zone
// is the same `kResizeZone` pixels on the LEFT side of the block.
bool ArrangementGrid::nearLeftEdge(int blockIdx, int x) const
{
    const auto& b = mPM.getBlock(blockIdx);
    int bx = barToX((float) effectiveStartBars(b));
    return (x >= bx - 2 && x < bx + kResizeZone);
}

bool ArrangementGrid::isSelected(int idx) const
{
    return std::find(mSelection.begin(), mSelection.end(), idx) != mSelection.end();
}

// ─────────────────────────────────────────────────────────────────────────────
// Automation point hit test - returns point index within lane, or -1
// ─────────────────────────────────────────────────────────────────────────────
int ArrangementGrid::hitTestAutomPoint(int blockIdx, int x, int y) const
{
    if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return -1;
    const auto& b = mPM.getBlock(blockIdx);
    if (b.clipType != ClipType::Automation) return -1;

    int bx = barToX((float) effectiveStartBars(b));
    int bw = jmax(4, barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - bx - 1);
    int by = rowToY(b.trackRow) + 2;
    int bh = rowHeightPx(b.trackRow) - 4;

    const auto& pts = b.automationLane.points;
    for (int i = (int)pts.size() - 1; i >= 0; --i)
    {
        float px = (float)bx + pts[i].timeTicks * (float)bw;
        float py = (float)by + (1.f - pts[i].value01) * (float)bh;
        float dx = (float)x - px;
        float dy = (float)y - py;
        if (dx * dx + dy * dy <= 36.f)   // 6px radius
            return i;
    }
    return -1;
}

// QA-G Task 5: evaluate a lane at normalized position t exactly the way the
// playback applicator does (linear segments; Stepped holds the left value;
// tension is not applied at runtime -- mirror of PluginProcessor's apply).
static float evalAutomationLaneAt (std::vector<ControlPoint> pts, float t)
{
    if (pts.empty()) return 0.5f;
    std::sort (pts.begin(), pts.end(),
               [](const ControlPoint& a, const ControlPoint& b){ return a.timeTicks < b.timeTicks; });
    if ((int) pts.size() == 1 || t <= pts.front().timeTicks) return pts.front().value01;
    if (t >= pts.back().timeTicks) return pts.back().value01;
    for (int i = 0; i < (int) pts.size() - 1; ++i)
    {
        if (t >= pts[i].timeTicks && t <= pts[i + 1].timeTicks)
        {
            if (pts[i].curveType == CurveType::Stepped) return pts[i].value01;
            const float span = pts[i + 1].timeTicks - pts[i].timeTicks;
            const float u    = (span > 0.f) ? (t - pts[i].timeTicks) / span : 0.f;
            return pts[i].value01 + u * (pts[i + 1].value01 - pts[i].value01);
        }
    }
    return pts.back().value01;
}

// QA-G Task 5: physically split a lane at a normalized cut position -- each
// side keeps its own points renormalized to 0..1 plus an interpolated
// boundary point, so both pieces keep playing exactly what they played
// before the cut (lane points are block-relative).
static void splitAutomationLane (AutomationLane& leftLane, AutomationLane& rightLane, float cutFrac)
{
    cutFrac = juce::jlimit (0.001f, 0.999f, cutFrac);
    const float cutVal = evalAutomationLaneAt (leftLane.points, cutFrac);

    std::vector<ControlPoint> sorted = leftLane.points;
    std::sort (sorted.begin(), sorted.end(),
               [](const ControlPoint& a, const ControlPoint& b){ return a.timeTicks < b.timeTicks; });

    std::vector<ControlPoint> left, right;
    for (const auto& p : sorted)
    {
        ControlPoint q = p;
        if (p.timeTicks < cutFrac)
        {
            q.timeTicks = p.timeTicks / cutFrac;
            left.push_back (q);
        }
        else
        {
            q.timeTicks = (p.timeTicks - cutFrac) / (1.f - cutFrac);
            right.push_back (q);
        }
    }
    ControlPoint bL; bL.timeTicks = 1.f; bL.value01 = cutVal; bL.curveType = CurveType::Linear; bL.tension = 0.f;
    ControlPoint bR; bR.timeTicks = 0.f; bR.value01 = cutVal; bR.curveType = CurveType::Linear; bR.tension = 0.f;
    left.push_back (bL);
    right.insert (right.begin(), bR);
    leftLane.points  = std::move (left);
    rightLane.points = std::move (right);
}

// Returns original lane->points index of the LEFT-side control point of the
// curve handle segment hit, or -1.  bx/bw/by/bh must match block screen coords.
static int hitTestAutomCurveHandleInBlock(const AutomationLane& lane,
                                          int bx, int bw, int by, int bh,
                                          int mx, int my)
{
    if (lane.points.size() < 2) return -1;

    std::vector<std::pair<ControlPoint,int>> sv;
    sv.reserve(lane.points.size());
    for (int i = 0; i < (int)lane.points.size(); ++i)
        sv.push_back({ lane.points[i], i });
    std::sort(sv.begin(), sv.end(),
        [](const auto& a, const auto& b){ return a.first.timeTicks < b.first.timeTicks; });

    for (int i = 0; i < (int)sv.size() - 1; ++i)
    {
        const ControlPoint& p0 = sv[i].first;
        const ControlPoint& p1 = sv[i + 1].first;
        if (p0.curveType == CurveType::Stepped) continue;
        if (std::abs(p0.value01 - p1.value01) < 0.02f) continue;

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
        float hx   = (float)bx + (p0.timeTicks + span * 0.5f) * (float)bw;
        float midV = p0.value01 + evalFL(0.5f, T) * (p1.value01 - p0.value01);
        float hy   = (float)by + (1.f - midV) * (float)bh;

        float dx = (float)mx - hx, dy = (float)my - hy;
        if (dx * dx + dy * dy <= 49.f)   // 7px radius
            return sv[i].second;          // original index of left-side point
    }
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Row name (public setter, shared with TrackHeaderPanel)
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::setRowName(int row, const juce::String& name)
{
    if (row >= 0 && row < kNumRows) { mPM.setRowName(row, name); repaint(); }
}

// ─────────────────────────────────────────────────────────────────────────────
// Clip colour helper
// ─────────────────────────────────────────────────────────────────────────────
Colour ArrangementGrid::blockColour(const ArrangementBlock& b) const
{
    if (b.clipType == ClipType::Automation) return kAutomCol;
    if (b.clipType == ClipType::Audio)      return Colour(0xff5a9fbf);
    // F-1 (2026-04-26): per-pattern user colour replaces the legacy 8-cycle
    // palette.  Default is light grey for fresh patterns; pre-F-1 saves load
    // with the default until the user picks a color via the right-click menu.
    if (b.patternIndex >= 0 && b.patternIndex < mPM.getNumPatterns())
        return mPM.getPattern(b.patternIndex).color;
    return Colour (0xffb0b0b0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio thumbnail helper
// ─────────────────────────────────────────────────────────────────────────────
AudioThumbnail* ArrangementGrid::getOrCreateThumbnail(const juce::String& path) const
{
    if (path.isEmpty()) return nullptr;
    if (mThumbnails.count(path) == 0)
    {
        // P4: stored path may be relative ("Samples/kick.wav") - resolve via
        // the callback (wired by BuilderPage to VibeSynthProcessor).  Cache
        // key uses the ORIGINAL path string so draw-code lookups (which pass
        // b.audioFilePath as-is) always hit the same entry.  When the
        // callback is unset, treat the string as an absolute path (pre-P4).
        const juce::File absolute = onResolveStoredPath
                                     ? onResolveStoredPath (path)
                                     : juce::File (path);
        auto thumb = std::make_unique<AudioThumbnail>(512, mAFM, mThumbCache);
        thumb->setSource(new FileInputSource(absolute));
        mThumbnails[path] = std::move(thumb);
    }
    return mThumbnails.at(path).get();
}

// QA-Ea Task 0c (Rule 5): bake the FULL file waveform once into an off-screen
// juce::Image so the slip-edit drag avoids per-frame drawChannels work.  Image
// is white-on-transparent so the caller's brush color tints via
// fillAlphaChannelWithCurrentBrush at blit time.  Re-bakes on partial -> fully-
// loaded transition (so the user gets a full waveform once the audio file
// finishes background-streaming into the thumbnail cache).  Returns nullptr
// when the thumbnail isn't loaded yet -- caller falls back to direct
// drawChannels with the visible time range so the user still sees partial data.
const juce::Image* ArrangementGrid::getOrBakeWaveformImage (
    const juce::String& path, juce::AudioThumbnail* thumb) const
{
    if (path.isEmpty() || thumb == nullptr) return nullptr;
    const double totalSec = thumb->getTotalLength();
    if (totalSec <= 0.0) return nullptr;       // thumb hasn't read header yet

    auto& entry = mWaveformImages[path];
    const bool nowLoaded = thumb->isFullyLoaded();

    const bool needBake = ! entry.image.isValid()
                        || (! entry.wasFullyLoadedWhenBaked && nowLoaded);
    if (needBake)
    {
        constexpr int kImgW = 2048;
        constexpr int kImgH = 64;
        juce::Image bake (juce::Image::ARGB, kImgW, kImgH, true);
        {
            juce::Graphics ig (bake);
            ig.fillAll (juce::Colours::transparentBlack);
            ig.setColour (juce::Colours::white);
            thumb->drawChannels (ig, bake.getBounds(), 0.0, totalSec, 1.f);
        }
        entry.image                   = std::move (bake);
        entry.wasFullyLoadedWhenBaked = nowLoaded;
    }
    return &entry.image;
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint helpers
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::drawRuler(Graphics& g) const
{
    auto b = getLocalBounds();
    // Pinned band: every ruler y is offset by the viewport's vertical scroll
    // so the band tracks the top of the VISIBLE area, not content y=0.
    const int   yPin = rulerPinY();
    const float fPin = (float) yPin;
    g.setColour(kHeaderBg);
    g.fillRect(0, yPin, b.getWidth(), kRulerH);
    g.setColour(VC::Accent.withAlpha(0.6f));
    g.drawHorizontalLine(yPin + kRulerH - 1, 0.f, (float)b.getWidth());

    int totalBars = totalVisibleBars();
    int maxBar    = (int)mBarOff + b.getWidth() / jmax(1, (int)mPPBar) + 2;

    // Subdivision tick marks in ruler (adaptive: beat / 1/8 / 1/16 / 1/32).
    // QA-G Task 6: positions stay beat-uniform (the uniform-position domain
    // is quarter-beats/4); BAR lines below come from the marker map.
    static constexpr float kMinRulerSpacing = 5.f;
    struct RulerLevel { int denom; float tickH; Colour col; };
    const RulerLevel rulerLevels[] = {
        { 32, 3.f,  VC::Accent.withAlpha(0.25f) },
        { 16, 4.f,  VC::Accent.withAlpha(0.35f) },
        {  8, 5.f,  VC::Accent.withAlpha(0.45f) },
        {  4, 7.f,  VC::Accent.withAlpha(0.60f) },
    };
    for (const auto& rl : rulerLevels)
    {
        if (mPPBar / (float)rl.denom < kMinRulerSpacing) continue;
        float step     = 1.f / (float)rl.denom;
        float startBar = std::floor(mBarOff * (float)rl.denom) / (float)rl.denom;
        g.setColour(rl.col);
        for (float bar = startBar; bar <= (float)(maxBar + 1); bar += step)
        {
            int rx = barToX(bar);
            if (rx < 0 || rx > b.getWidth()) continue;
            g.drawVerticalLine(rx, fPin + (float)(kRulerH - (int)rl.tickH), fPin + (float)kRulerH);
        }
    }

    // Bar lines + labels: MAP-driven (QA-G Task 6) -- bars resize at TS
    // markers; bar k's left edge sits at barStartBeat(k)/4 in the uniform
    // position domain.  Negative-bar (pre-roll) territory stays uniform 4/4
    // (markers cannot exist before bar 0).  No upper cap on bar number - the
    // ruler extends as far as the viewport reaches, regardless of song length.
    (void)totalBars;
    {
        int bar = 0;
        if (mBarOff < 0.f)
            bar = (int) std::floor (mBarOff);
        else
        {
            double bib = 0.0;
            mPM.beatToBarAndBeatInBar ((double) mBarOff * 4.0, bar, bib);
        }
        // Guard bounds the pathological case (1/32-signature bars at macro
        // zoom-out) so paint can never spin unbounded.
        for (int guard = 0; guard < 8192; ++guard, ++bar)
        {
            const double barPos = (bar <= 0) ? (double) bar
                                             : mPM.barStartBeat (bar) / 4.0;
            int rx = barToX ((float) barPos);
            if (rx > b.getWidth()) break;
            if (rx < 0) continue;

            bool isMajor = (bar % 4 == 0);
            g.setColour(isMajor ? VC::Accent.brighter(0.5f) : VC::Accent.withAlpha(0.5f));
            g.drawVerticalLine(rx, fPin, fPin + (float)kRulerH);

            if (isMajor || mPPBar >= 30) {
                g.setColour(VC::Text);
                g.setFont(Font(9));
                // 1-based labels (owner 2026-07-16): song downbeat (bar 0 in code)
                // shows as "1"; pre-roll / slip-edited clips show "0", "-1", etc.
                g.drawText(String(bar + 1), rx + 2, yPin + 1, 24, kRulerH - 2,
                           Justification::centredLeft, false);
            }
        }
    }

    // Performance mode: pulse animation on current bar
    if (mPerfMode && mPlayheadBar >= 0.0)
    {
        float pulse = 0.5f + 0.5f * std::sin(mPulsePhi);
        int px = barToX((float)mPlayheadBar);
        g.setColour(VC::Highlight.withAlpha(0.55f * pulse));
        // #30 (QA-G3Smoke, final form per Jeff): right-hanging handle, mast
        // edge at px (matches the roll/kit flag markers; whole at bar 0).
        g.fillRect(jmax(0, px), yPin, 8, kRulerH);
    }

    // Time selection highlight
    if (mTimeSelStart >= 0.f && mTimeSelEnd > mTimeSelStart)
    {
        int sx = barToX(mTimeSelStart);
        int ex = barToX(mTimeSelEnd);
        g.setColour(VC::Highlight.withAlpha(0.30f));
        g.fillRect(sx, yPin, ex - sx, kRulerH);
        g.setColour(VC::Highlight.withAlpha(0.85f));
        g.drawVerticalLine(sx, fPin, fPin + (float)kRulerH);
        g.drawVerticalLine(ex, fPin, fPin + (float)kRulerH);
    }

    // 2026-04-26 (D-2): time-marker flags (yellow pennant) + TS-change labels.
    // Both render in the ruler band, BELOW the bar-number labels but above
    // the bar-tick lines (drawn earlier).  Drawn before the playhead arrow
    // so the playhead is always visually on top.
    {
        const float flagH    = (float) (kRulerH - 4);
        const float flagW    = 8.f;
        for (int i = 0; i < mPM.getNumTimeMarkers(); ++i)
        {
            const auto& m = mPM.getTimeMarker(i);
            int rx = barToX((float) m.bar);
            if (rx < -16 || rx > b.getWidth()) continue;
            // Pole.
            g.setColour(VC::Yellow.withAlpha(0.9f));
            g.drawVerticalLine(rx, fPin + 1.f, fPin + flagH + 1.f);
            // Pennant (small triangle pointing right).
            juce::Path flag;
            flag.addTriangle((float) rx + 1, fPin + 2.f,
                             (float) rx + 1 + flagW, fPin + 2.f + flagH * 0.45f,
                             (float) rx + 1, fPin + 2.f + flagH * 0.45f);
            g.fillPath(flag);
        }

        for (int i = 0; i < mPM.getNumTimeSigChanges(); ++i)
        {
            const auto& ts = mPM.getTimeSigChange(i);
            int rx = barToX((float)(mPM.barStartBeat(ts.bar) / 4.0));
            if (rx < -32 || rx > b.getWidth()) continue;
            const juce::String txt = juce::String(ts.num) + "/" + juce::String(ts.den);
            const int textW = 28;
            // Linked auto-markers (spawned by a pattern placement, docket B)
            // draw as an OUTLINE pill -- the visual tell vs solid manual ones.
            const bool linked = (ts.linkedPattern >= 0);
            g.setColour(VC::Blue.withAlpha(0.85f));
            if (linked)
                g.drawRoundedRectangle((float)(rx + 1), fPin + 1.f, (float) textW, (float)(kRulerH - 2), 2.f, 1.2f);
            else
                g.fillRoundedRectangle((float)(rx + 1), fPin + 1.f, (float) textW, (float)(kRulerH - 2), 2.f);
            g.setColour(linked ? VC::Blue.brighter(0.6f) : juce::Colours::white);
            g.setFont(Font(9.f, Font::bold));
            g.drawText(txt, rx + 1, yPin + 1, textW, kRulerH - 2,
                       Justification::centred, false);
        }

        // QA-TempoMap (2026-07-08): tempo flags - amber pill with the BPM
        // number, distinct from the yellow marker pennant + blue TS pill.
        for (int i = 0; i < mPM.getNumTempoChanges(); ++i)
        {
            const auto& tc = mPM.getTempoChange(i);
            int rx = barToX((float) tc.bar);
            if (rx < -40 || rx > b.getWidth()) continue;
            const juce::String txt = juce::String((int) std::llround(tc.bpm));
            const int textW = 26;
            g.setColour(juce::Colour(0xffFFB030).withAlpha(0.9f));
            g.drawVerticalLine(rx, fPin + 1.f, fPin + (float)(kRulerH - 3));
            g.setColour(juce::Colour(0xff3A2F18));
            g.fillRoundedRectangle((float)(rx + 1), fPin + 1.f, (float) textW, (float)(kRulerH - 2), 2.f);
            g.setColour(juce::Colour(0xffFFB030));
            g.setFont(Font(9.f, Font::bold));
            g.drawText(txt, rx + 1, yPin + 1, textW, kRulerH - 2,
                       Justification::centred, false);
        }
    }

    // Playhead marker - drawn last so it's always on top of ruler content.
    // Smoke #6: flag form matching the roll/kit markers (mast edge AT px,
    // flag hanging right) -- was still the old centered down-arrow.
    if (mPlayheadBar >= 0.0)
    {
        int px = barToX((float)mPlayheadBar);
        if (px >= -8 && px < b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.9f));
            juce::Path flag;
            flag.addTriangle((float)px,       fPin,
                             (float)(px + 8), fPin,
                             (float)px,       fPin + (float)kRulerH);
            g.fillPath(flag);
        }
    }
}

void ArrangementGrid::drawRowBgs(Graphics& g) const
{
    auto b = getLocalBounds();
    for (int r = 0; r < kNumRows; ++r)
    {
        int y  = rowToY(r);
        int rh = rowHeightPx(r);
        if (y + rh < 0 || y > b.getHeight()) continue;
        g.setColour((r % 2 == 0) ? VC::Panel : VC::Bg.brighter(0.04f));
        g.fillRect(0, y, b.getWidth(), rh);
        g.setColour(kGridLine);
        g.drawHorizontalLine(y + rh - 1, 0.f, (float)b.getWidth());
    }
}

void ArrangementGrid::drawGrid(Graphics& g) const
{
    auto b    = getLocalBounds();
    int  yTop = kRulerH;
    int  yBot = jmin(yTop + (int)(kNumRows * mEffectiveRowH), b.getHeight());

    // QA-Ee Stage 3: the grid is DECOUPLED from the snap division -- it always draws
    // every ladder rung that clears kMinLinePx, so zooming in reveals down to 1/64
    // (straight) or 1/6 Step (triplet) regardless of how coarse the snap is.  The
    // snap TYPE only flips which ladder is used (straight kDynamicSnapLadder vs
    // kTripletGridLadder); the snap DIVISION is pure magnetism and never caps the
    // visual.  Builder is uniform 4-beat-per-bar => 1 bar = 384 ticks, so the bar
    // rung is drawn straight from the ladder here.  Iterated fine -> coarse so the
    // bar line (brightest) draws last and overdraws finer lines at shared x.
    const int div = onGetSnapDiv ? onGetSnapDiv() : 1;   // default Line

    int        nLad   = 0;
    const int* ladder = gridLadderForSnap (div, nLad);

    int maxBar = (int)mBarOff + b.getWidth() / jmax(1, (int)mPPBar) + 2;

    for (int i = nLad - 1; i >= 0; --i)
    {
        const int    gTicks    = ladder[i];
        const double spacingPx = (double) gTicks / 384.0 * (double) mPPBar;
        if (spacingPx < (double) kMinLinePx) continue;   // sub-threshold: declutter at macro zoom-out
        // QA-G Task 6: bar-level rungs come from the marker MAP below --
        // sub-bar rungs here stay beat-uniform.
        if (gTicks >= 384) continue;

        const bool  isBar = (gTicks == 384);
        const float alpha = isBar        ? 0.85f
                          : gTicks >= 96 ? 0.50f
                          : gTicks >= 48 ? 0.28f
                          : gTicks >= 24 ? 0.18f
                          :                0.10f;
        g.setColour (isBar ? kGridLineMj.withAlpha (alpha) : kGridLine.withAlpha (alpha));

        const double stepBars = (double) gTicks / 384.0;
        double startBar = std::floor (mBarOff / stepBars) * stepBars;
        for (double bar = startBar; bar <= (double)(maxBar + 1); bar += stepBars)
        {
            int x = barToX ((float) bar);
            if (x < 0 || x > b.getWidth()) continue;
            g.drawVerticalLine (x, (float) yTop, (float) yBot);
        }
    }

    // QA-G Task 6: bar lines from the marker MAP (bars resize at markers).
    // Declutter parity with the old bar rung: hidden at macro zoom-out.
    if ((double) mPPBar >= (double) kMinLinePx)
    {
        int bar = 0;
        if (mBarOff < 0.f)
            bar = (int) std::floor (mBarOff);
        else
        {
            double bib = 0.0;
            mPM.beatToBarAndBeatInBar ((double) mBarOff * 4.0, bar, bib);
        }
        g.setColour (kGridLineMj.withAlpha (0.85f));
        for (int guard = 0; guard < 8192; ++guard, ++bar)
        {
            const double barPos = (bar <= 0) ? (double) bar
                                             : mPM.barStartBeat (bar) / 4.0;
            const int x = barToX ((float) barPos);
            if (x > b.getWidth()) break;
            if (x < 0) continue;
            g.drawVerticalLine (x, (float) yTop, (float) yBot);
        }
    }
}

// MIDI note shading inside a pattern clip.
// Maps MIDI 72 (C5) to vertical centre; ±24 semitones (2 octaves) span the
// full clip height. Notes outside that range are clamped to the edges.
// Aggregates layer rolls, bass rolls, and all per-drum rolls (plus the
// legacy pre-Phase-D drumRoll) so the preview reflects the whole pattern.
// Owner spec (G1 smoke): notes sit at TRUE musical positions -- BEAT-TRUE
// (x = quarter-beats, matching playback exactly for every signature).
// #24 (QA-G3Smoke): tiling is GONE -- the preview draws ONE pass of the
// pattern content from the content offset (mirrors the de-tiled scheduler);
// block length past the content stays blank.
void ArrangementGrid::drawMidiShading(Graphics& g, const ArrangementBlock& b,
                                      int bx, int by, int bw, int bh) const
{
    if (b.patternIndex < 0 || b.patternIndex >= mPM.getNumPatterns()) return;
    const auto& pat = mPM.getPattern(b.patternIndex);

    const double beatsPerBar = mPM.getPatternBeatsPerBar (b.patternIndex);
    // B-1: the content length is the pattern's REAL note-for-note extent / 4
    // (same value the scheduler's snapshot carries, /4 for this uniform-bar
    // space) -- content past it simply doesn't exist to draw.
    const double cycleBars   = mPM.getPatternContentBeats (b.patternIndex) / 4.0;
    if (beatsPerBar <= 0.0) return;

    const double blockStart = effectiveStartBars (b);
    const double blockLen   = effectiveLengthBars (b);
    if (blockLen <= 0.0) return;

    constexpr int   kCentreNote   = 72;   // C5
    constexpr float kHalfRangeSt  = 24.f; // ±2 octaves spans full clip height
    const int innerH = jmax(1, bh - 4);
    const int innerY = by + 2;
    const int clipL  = bx;
    const int clipR  = bx + bw;

    g.setColour(blockColour(b).brighter(0.7f).withAlpha(0.6f));

    // QA-G Task 5 / #24: the content offset phase-shifts the single pass (a
    // sliced piece draws its true slice -- same mapping playback uses); an
    // offset past the content leaves the block blank, like the scheduler.
    const double offsetBars = juce::jmax (0.0, (double) b.contentOffsetTicks / 384.0);

    auto paintRoll = [&](const PianoRollData& roll)
    {
        for (const auto& n : roll.notes)
        {
            const double noteBar = n.startBeat / 4.0;
            if (noteBar >= cycleBars) continue;   // outside the pattern's content
            const double noteEnd = jmin (cycleBars, noteBar + n.durationBeats / 4.0);
            // Centre = 0.5; semitones above centre move up (smaller y), below move down.
            const float relSt = (float)(n.midiNote - kCentreNote);
            const float yFrac = jlimit(0.f, 1.f, 0.5f - 0.5f * (relSt / kHalfRangeSt));
            const int   ny    = innerY + (int)(yFrac * (float)(innerH - 2));

            const double sBars = noteBar - offsetBars;
            if (sBars >= blockLen) continue;
            const double eBars = jmin (blockLen, noteEnd - offsetBars);
            // B-3: a note straddling the block's LEFT edge draws its visible
            // fragment from the boundary (matches the scheduler's clamp-and-play),
            // rather than being dropped entirely.
            const double drawS = (sBars < 0.0) ? 0.0 : sBars;
            if (eBars <= drawS) continue;   // nothing visible (entirely before)
            int nx = barToX((float)(blockStart + drawS));
            if (nx >= clipR) continue;
            int nw = jmax(1, barToX((float)(blockStart + eBars)) - nx);
            if (nx + nw <= clipL) continue;
            nx = jmax(nx, clipL);
            nw = jmin(nx + nw, clipR) - nx;
            g.fillRect(nx, ny, nw, 2);
        }
    };

    for (const auto& lr : pat.layerRoll) paintRoll(lr);
    for (const auto& br : pat.bassRoll)  paintRoll(br);
    for (const auto& dr : pat.drumRolls) paintRoll(dr);
    paintRoll(pat.drumRoll);   // legacy pre-migration data (usually empty)
    // #29 (QA-G3Smoke, G-8): the preview was blind to three scheduled roll
    // families -- inst + clips + Rusty now shade like the rest (vox excluded:
    // no vox MIDI).
    for (const auto& ir : pat.instRoll)  paintRoll(ir);
    for (const auto& cr : pat.clipRoll)  paintRoll(cr);
    paintRoll(pat.baySickRustyDrumsRoll);
}

void ArrangementGrid::drawPatternClip(Graphics& g, const ArrangementBlock& b,
                                      int x, int y, int w, int h, bool sel) const
{
    Colour base = blockColour(b);

    g.setGradientFill(ColourGradient(
        base.brighter(0.3f), (float)x, (float)y,
        base.darker(0.2f),   (float)x, (float)(y + h), false));
    g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);

    // MIDI note shading
    if (w >= 20) drawMidiShading(g, b, x, y, w, h);

    g.setColour(base.brighter(0.65f).withAlpha(0.7f));
    g.drawLine((float)x + 2.5f, (float)y + 1.f, (float)(x + w) - 2.5f, (float)y + 1.f, 1.f);

    g.setColour(Colour(0xff000000).withAlpha(0.35f));
    g.drawLine((float)x + 2.5f, (float)(y + h) - 1.f, (float)(x + w) - 2.5f, (float)(y + h) - 1.f, 1.f);

    if (sel) {
        g.setColour(Colours::white.withAlpha(0.2f));
        g.drawRoundedRectangle((float)x - 1.f, (float)y - 1.f, (float)w + 2.f, (float)h + 2.f, 4.f, 1.5f);
        g.setColour(Colours::white.withAlpha(0.95f));
        g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.5f);
    } else {
        g.setColour(base.brighter(0.3f).withAlpha(0.55f));
        g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.f);
    }

    // Muted overlay - 2026-04-26 (D-1): 30% black wash + diagonal hatch.
    if (b.muted)
    {
        g.setColour(Colour(0x4d000000));   // ~30% alpha black
        g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);

        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);
        g.reduceClipRegion(clip);
        g.setColour(Colours::white.withAlpha(0.16f));
        const float stride = 8.f;
        for (float dx = -h; dx < w + h; dx += stride)
        {
            Path line;
            line.startNewSubPath((float)x + dx, (float)y);
            line.lineTo((float)x + dx + h, (float)y + h);
            g.strokePath(line, PathStrokeType(2.0f));
        }
    }

    if (w >= 16 && h >= 9) {
        String name = (b.patternIndex < mPM.getNumPatterns())
                      ? mPM.getPattern(b.patternIndex).name : "?";
        g.setColour(Colours::white.withAlpha(0.88f));
        g.setFont(Font(10.f, Font::bold));
        g.drawText(name, x + 4, y + 1, w - 8, h - 2, Justification::centredLeft, true);
    }

    // Resize handle
    g.setColour(base.brighter(0.8f).withAlpha(0.5f));
    g.fillRoundedRectangle((float)(x + w - 5), (float)(y + 3), 3.f, (float)(h - 6), 1.5f);
}

void ArrangementGrid::drawAudioClip(Graphics& g, const ArrangementBlock& b,
                                    int x, int y, int w, int h, bool sel) const
{
    static const Colour kAudioGeneric { 0xff4a8fa0 };   // unrouted / generic Audio row
    static const Colour kMissingBase  { 0xffaa3030 };    // Batch E #3: dim red

    // Batch E #3 (2026-05-01): if the audio file path no longer resolves on
    // disk, render the clip in red instead of the standard teal so the user
    // can see at a glance that this clip is dead.  Empty path also counts as
    // missing (newly-dropped clips that haven't been resolved yet).
    // QA-E Task 5 (2026-05-15): resolve project-relative paths via
    // onResolveStoredPath BEFORE the existsAsFile() check.  Without this,
    // a relative path like "Samples/foo.wav" resolves against the EXE's
    // CWD (not the project folder), always fails existsAsFile(), and every
    // relative-path block (disk drops + Vox/Inst recordings) paints red --
    // shadowing the route-colour logic below.  Mirrors the resolve pattern
    // already used by placeAudioLibraryEntry + getOrCreateThumbnail.
    juce::File resolvedF (b.audioFilePath);
    if (! resolvedF.existsAsFile() && onResolveStoredPath)
        resolvedF = onResolveStoredPath (b.audioFilePath);
    const bool missingFile = b.audioFilePath.isEmpty()
                          || ! resolvedF.existsAsFile();

    // QA-E Task 5 (2026-05-15): colour audio blocks by their route type so
    // the user can tell at a glance which page a block plays through:
    //   Clips  (audioInsert 400..449) -> amber  0xffd4a017
    //   Vox    (voxInsert   600..605) -> teal   0xff0fafa5
    //   Inst   (instInsert  700..705) -> navy   0xff1c3a8a
    //   unrouted / generic Audio row  -> the legacy teal-grey base
    // Ranges mirror MixerChannelIds (Source/VibeGraph.h); kept as literals
    // here so BuilderPage doesn't pull in the heavy graph header just for
    // three int ranges.  Missing-file red still overrides everything.
    Colour base;
    if (missingFile)
        base = kMissingBase;
    else
    {
        const int rc = b.routeChannel;
        using namespace MixerChannelIds;
        if      (rc >= kVoxBase   && rc < kVoxBase   + kMaxVoxStrips)   base = Colour (0xff0fafa5);   // Vox
        else if (rc >= kInstBase  && rc < kInstBase  + kMaxInstStrips)  base = Colour (0xff1c3a8a);   // Inst
        else if (rc >= kAudioBase && rc < kAudioBase + kMaxAudioStrips) base = Colour (0xffd4a017);   // Clips
        else                                                            base = kAudioGeneric;         // unrouted
    }

    // Background
    g.setGradientFill(ColourGradient(
        base.brighter(0.2f), (float)x, (float)y,
        base.darker(0.25f),  (float)x, (float)(y + h), false));
    g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);

    // Waveform thumbnail
    // QA-Ea Task 0c: now honors contentStartSamples + lengthBeats so a
    // slip-edited / FL-pre-roll clip shows only its visible (post-content-
    // start) region of the underlying file -- not the full file packed into
    // the visible width.  Rule 5: uses a cached Image blit (bake-once,
    // scale-blit-many) for low-CPU drag rendering.  Fallback drawChannels
    // path runs when the thumbnail isn't loaded yet so the user still sees
    // partial data while audio finishes background-streaming.
    if (w >= 12 && h >= 6)
    {
        if (auto* thumb = getOrCreateThumbnail(b.audioFilePath))
        {
            const Rectangle<int> destRect (x + 2, y + 2, w - 4, h - 4);
            const double fileTotalSec = thumb->getTotalLength();
            const double sampleRate   = onGetSampleRate
                                          ? onGetSampleRate()
                                          : 44100.0;
            const double bpm          = onGetBPM
                                          ? juce::jmax (20.0, onGetBPM())
                                          : 120.0;
            const double contentSec   = (sampleRate > 0.0)
                ? (double) b.contentStartSamples / sampleRate
                : 0.0;
            const double visibleSec   = effectiveLengthBeats (b) * 60.0 / bpm;

            const juce::Image* img = (fileTotalSec > 0.0)
                ? getOrBakeWaveformImage (b.audioFilePath, thumb)
                : nullptr;

            g.setColour (base.brighter (0.9f).withAlpha (0.7f));

            if (img != nullptr && fileTotalSec > 0.0 && visibleSec > 0.0)
            {
                // QA-Ea Task 0c (2026-05-20 A' mode-gate): visual blit mode
                // depends on EditMode.
                //   Slip mode  = fixed pixels-per-second.  Audible portion
                //                paints at constant density; any dead space
                //                (post-EOF or pre-content) leaves the clip's
                //                background showing through.  With (B')
                //                clamps the audible region == visible region
                //                in practice, but the dead-space-friendly
                //                math is kept for legacy / pre-clamp blocks.
                //   Stretch    = proportional fill.  Audible portion stretches
                //                to fill the ENTIRE block width edge-to-edge.
                //                Visual placeholder for the QA-Ec time-stretch
                //                DSP; audio path is untouched until QA-Ec
                //                wires the real stretch.
                // Audible portion = intersect [contentSec, contentSec+visibleSec]
                // with [0, fileTotalSec].
                const double audibleT0 = juce::jmax (0.0, contentSec);
                const double audibleT1 = juce::jmin (fileTotalSec,
                                                     contentSec + visibleSec);
                if (audibleT1 > audibleT0)
                {
                    // Offsets within the clip (in seconds from clip left edge)
                    // where the audible portion starts and ends.
                    const double clipDestSec0 = audibleT0 - contentSec;
                    const double clipDestSec1 = audibleT1 - contentSec;
                    const float blkW = (float) destRect.getWidth();

                    float destXStart, destXEnd;
                    if (mEditMode == EditMode::Stretch)
                    {
                        // Stretch mode: audible region stretches to fill the
                        // entire block, edge-to-edge.
                        destXStart = (float) destRect.getX();
                        destXEnd   = (float) destRect.getX() + blkW;
                    }
                    else
                    {
                        // Slip mode: fixed pixels-per-second.  Audible region
                        // paints at constant density; dead space (if any)
                        // shows the clip background through.
                        destXStart = (float) destRect.getX()
                                   + (float) (clipDestSec0 / visibleSec) * blkW;
                        destXEnd   = (float) destRect.getX()
                                   + (float) (clipDestSec1 / visibleSec) * blkW;
                    }
                    const int destW = juce::jmax (1,
                                          juce::roundToInt (destXEnd - destXStart));

                    const float imgW = (float) img->getWidth();
                    const int srcX = juce::roundToInt ((audibleT0 / fileTotalSec) * imgW);
                    const int srcW = juce::jmax (1, juce::roundToInt (
                                        ((audibleT1 - audibleT0) / fileTotalSec) * imgW));
                    g.drawImage (*img,
                                 juce::roundToInt (destXStart), destRect.getY(),
                                 destW, destRect.getHeight(),
                                 srcX, 0,
                                 srcW, img->getHeight(),
                                 /*fillAlphaChannelWithCurrentBrush*/ true);
                }
            }
            else
            {
                // Thumb not loaded yet -- direct drawChannels for partial data.
                if (fileTotalSec > 0.0)
                {
                    const double drawT0 = juce::jlimit (0.0, fileTotalSec, contentSec);
                    const double drawT1 = juce::jlimit (drawT0, fileTotalSec,
                                                        contentSec + visibleSec);
                    if (drawT1 > drawT0)
                        thumb->drawChannels (g, destRect, drawT0, drawT1, 1.f);
                    else
                        thumb->drawChannels (g, destRect, 0.0, fileTotalSec, 1.f);
                }
                else
                {
                    thumb->drawChannels (g, destRect, 0.0, fileTotalSec, 1.f);
                }
            }
        }
    }

    // Border
    if (sel) {
        g.setColour(Colours::white.withAlpha(0.95f));
        g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.5f);
    } else {
        g.setColour(base.brighter(0.5f).withAlpha(0.6f));
        g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.f);
    }

    // Filename label
    if (w >= 24 && h >= 9) {
        g.setColour(Colours::white.withAlpha(0.9f));
        g.setFont(Font(9.f, Font::bold));
        // QA-E Task 7 (FILE-02) bug fix: honor the Browser rename.
        // renameAudioAt stamps b.displayAlias on every matching block, but
        // this label was always re-deriving from the file path so the rename
        // never showed on the grid.  Mirror the displayAlias-first pattern
        // already used at StandaloneEditor.cpp:10242.
        String fname = b.displayAlias.isNotEmpty()
                         ? b.displayAlias
                         : File(b.audioFilePath).getFileNameWithoutExtension();
        g.drawText(fname, x + 4, y + 1, w - 8, h - 2, Justification::centredLeft, true);
    }

    // Pitch label (if shifted)
    if (b.pitchSemitones != 0.f && w >= 40) {
        g.setColour(VC::Yellow.withAlpha(0.85f));
        g.setFont(Font(8.f));
        g.drawText((b.pitchSemitones > 0 ? "+" : "") + String(b.pitchSemitones, 1) + "st",
                   x + w - 36, y + 1, 34, 12, Justification::centredRight, false);
    }

    // G1 boundary (Jeff #2): stretch badge - the clip's playback-speed factor
    // vs its NATURAL tempo (library entry / Reset Stretch target) to 0.01.
    // Hidden at x1.00.  Bottom-RIGHT on a dark pill (the top row is owned by
    // the full-width filename label; the bottom-left corner by the follow
    // dot).  Makes a re-fit visible no matter which gesture baked it.
    if (b.clipType == ClipType::Audio && w >= 40 && h >= 24)
    {
        const int   li      = mPM.findAudioLibraryIndexByPath (b.audioFilePath);
        const float natural = li >= 0 ? mPM.getAudioLibraryBPM (li) : 0.f;
        if (natural > 0.f && b.originalBPM > 0.f)
        {
            const double factor = (double) b.originalBPM / (double) natural;
            if (std::abs (factor - 1.0) >= 0.005)
            {
                const String txt = "x" + String (factor, 2);
                g.setFont (Font (9.f, Font::bold));
                const int tw = juce::jmax (30,
                    (int) std::ceil (g.getCurrentFont().getStringWidthFloat (txt)) + 8);
                const juce::Rectangle<float> pill ((float) (x + w - tw - 3),
                                                   (float) (y + h - 15),
                                                   (float) tw, 12.f);
                g.setColour (Colours::black.withAlpha (0.65f));
                g.fillRoundedRectangle (pill, 6.f);
                g.setColour (Colour (0xffFFB030));
                g.drawText (txt, pill.toNearestInt(), Justification::centred, false);
            }
        }
    }

    // QA-E Task 7 (FILE-02): per-copy "follows the file's master" indicator.
    // NOT the project dirty flag (that is the title-bar asterisk).  Always
    // shown on audio blocks: GREEN = this copy still follows the file's
    // library master settings (not individually changed); RED = this copy
    // was customized (isOverride) and no longer follows.  Bottom-LEFT so it
    // never collides with the top-row filename / pitch labels.
    if (b.clipType == ClipType::Audio && w >= 16 && h >= 10)
    {
        const float r  = 3.5f;
        const float cx = (float) x + r + 3.f;
        const float cy = (float) (y + h) - r - 3.f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (cx - r - 1.f, cy - r - 1.f, (r + 1.f) * 2.f, (r + 1.f) * 2.f);
        g.setColour (b.isOverride ? Colour (0xffe5453a)     // red   = customized
                                  : Colour (0xff35c65a));   // green = following
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
    }

    // QA-Ea Task 0c (FL pre-roll record): hidden-audio indicator.  When an
    // Audio clip has contentStartSamples > 0 (post-record default with
    // count-in, OR after a slip-edit drag-right) there is audio data in the
    // underlying WAV before the visible left edge.  Render a small left-
    // pointing triangle on the clip's left side so beginners know they can
    // drag the left edge backward (with the Ctrl+Alt+Home slip-edit mode
    // on) to reveal the pre-roll.  Min-width threshold so the arrow doesn't
    // collide with the resize-handle area on very narrow clips.
    if (b.clipType == ClipType::Audio && b.contentStartSamples > 0
        && w >= 14 && h >= 12)
    {
        const float ax = (float) x + 2.f;            // 2 px in from the left edge
        const float ayCenter = (float) y + (float) h * 0.5f;
        const float aw = 5.f;                        // arrow width
        const float ah = 7.f;                        // arrow height
        Path arrow;
        arrow.startNewSubPath (ax,        ayCenter);             // tip (pointing left)
        arrow.lineTo          (ax + aw,   ayCenter - ah * 0.5f); // upper rear
        arrow.lineTo          (ax + aw,   ayCenter + ah * 0.5f); // lower rear
        arrow.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillPath (arrow);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.strokePath (arrow, PathStrokeType (1.0f));
    }

    // Muted overlay - 2026-04-26 (D-1): 30% black wash + diagonal hatch.
    if (b.muted)
    {
        g.setColour(Colour(0x4d000000));   // ~30% alpha black
        g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);

        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);
        g.reduceClipRegion(clip);
        g.setColour(Colours::white.withAlpha(0.16f));
        const float stride = 8.f;
        for (float dx = -h; dx < w + h; dx += stride)
        {
            Path line;
            line.startNewSubPath((float)x + dx, (float)y);
            line.lineTo((float)x + dx + h, (float)y + h);
            g.strokePath(line, PathStrokeType(2.0f));
        }
    }

    // Resize handle
    g.setColour(base.brighter(0.8f).withAlpha(0.5f));
    g.fillRoundedRectangle((float)(x + w - 5), (float)(y + 3), 3.f, (float)(h - 6), 1.5f);
}

void ArrangementGrid::drawAutomationClip(Graphics& g, const ArrangementBlock& b,
                                         int x, int y, int w, int h, bool sel) const
{
    // Dark teal background
    g.setColour(Colour(0xff0d2a2a));
    g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);

    if (w >= 8 && h >= 6)
    {
        const auto& lane = b.automationLane;
        const float ix  = (float)(x + 2);
        const float iw  = (float)(w - 4);
        const float iy  = (float)(y + 2);
        const float ih  = (float)(h - 4);

        // ── Horizontal reference lines at 25%, 50%, 75% ─────────────────────
        for (float v : { 0.25f, 0.5f, 0.75f })
        {
            float lineY = iy + (1.f - v) * ih;
            g.setColour(kAutomCol.withAlpha(v == 0.5f ? 0.22f : 0.12f));
            g.drawHorizontalLine((int)lineY, ix, ix + iw);
        }

        if (!lane.points.empty())
        {
            // ── Sample curve via evaluateAt - handles tension, spline, stepped ─
            // Sort to find first/last positions
            std::vector<ControlPoint> sortedPts = lane.points;
            std::sort(sortedPts.begin(), sortedPts.end(),
                [](const ControlPoint& a, const ControlPoint& c){ return a.timeTicks < c.timeTicks; });
            float tFirst = sortedPts.front().timeTicks;
            float tLast  = sortedPts.back().timeTicks;
            float xFirst = ix + tFirst * iw;
            float xLast  = ix + tLast  * iw;

            const int kSamples = jmax(16, (int)(iw / 2));
            Path fillPath, linePath;
            {
                float v0  = sortedPts.front().value01;
                float py0 = iy + (1.f - v0) * ih;
                fillPath.startNewSubPath(xFirst, iy + ih);
                fillPath.lineTo(xFirst, py0);
                linePath.startNewSubPath(xFirst, py0);
            }
            for (int s = 1; s <= kSamples; ++s)
            {
                float t   = tFirst + (float)s / (float)kSamples * (tLast - tFirst);
                float val = lane.evaluateAt(t);
                float px  = ix + t * iw;
                float py  = iy + (1.f - val) * ih;
                fillPath.lineTo(px, py);
                linePath.lineTo(px, py);
            }
            fillPath.lineTo(xLast, iy + ih);
            fillPath.closeSubPath();

            g.setColour(kAutomCol.withAlpha(0.13f));
            g.fillPath(fillPath);
            g.setColour(kAutomCol.withAlpha(sel ? 1.0f : 0.85f));
            g.strokePath(linePath, PathStrokeType(1.5f));

            // ── Control point nodes ──────────────────────────────────────────
            std::vector<ControlPoint> pts = lane.points;
            std::sort(pts.begin(), pts.end(),
                [](const ControlPoint& a, const ControlPoint& c)
                    { return a.timeTicks < c.timeTicks; });
            for (const auto& pt : pts)
            {
                float px = ix + pt.timeTicks * iw;
                float py = iy + (1.f - pt.value01) * ih;
                g.setColour(kAutomCol);
                g.drawEllipse(px - 4.f, py - 4.f, 8.f, 8.f, 1.5f);
                g.setColour(Colour(0xff0d2a2a));
                g.fillEllipse(px - 2.5f, py - 2.5f, 5.f, 5.f);
            }

            // ── Curve handle diamonds (only when block is large enough) ──────
            if (w >= 30 && h >= 20)
            {
                const Colour kHCol = Colour(0xff18c8a0);
                for (int pi = 0; pi < (int)pts.size() - 1; ++pi)
                {
                    const auto& p0 = pts[pi];
                    const auto& p1 = pts[pi + 1];
                    if (p0.curveType == CurveType::Stepped) continue;
                    if (std::abs(p0.value01 - p1.value01) < 0.02f) continue;

                    float spanD = p1.timeTicks - p0.timeTicks;
                    float TD    = juce::jlimit(-0.999f, 0.999f, p0.tension);
                    auto evalFLD = [](float x, float Tv) -> float {
                        if (std::abs(Tv) < 0.001f) return x;
                        float denom    = 0.5f * Tv + 0.5f;
                        float t_factor = 1.0f - Tv / (denom * denom);
                        if (std::abs(t_factor - 1.0f) < 0.0001f) return x;
                        return juce::jlimit(0.0f, 1.0f,
                            (std::pow(std::abs(t_factor), x) - 1.0f) / (t_factor - 1.0f));
                    };
                    // Diamond at x=0.5 (horizontal midpoint of segment)
                    float mx   = ix + (p0.timeTicks + spanD * 0.5f) * iw;
                    float midV = p0.value01 + evalFLD(0.5f, TD) * (p1.value01 - p0.value01);
                    float my   = iy + (1.f - midV) * ih;
                    const float r = 3.f;

                    Path diamond;
                    diamond.startNewSubPath(mx,     my - r);
                    diamond.lineTo         (mx + r, my    );
                    diamond.lineTo         (mx,     my + r);
                    diamond.lineTo         (mx - r, my    );
                    diamond.closeSubPath();
                    g.setColour(kHCol.withAlpha(0.12f));
                    g.fillPath(diamond);
                    g.setColour(kHCol.withAlpha(0.6f));
                    g.strokePath(diamond, PathStrokeType(1.f));
                }
            }
        }
        else
        {
            // No points yet - draw dashed guide line at midpoint
            float midY = iy + ih * 0.5f;
            g.setColour(kAutomCol.withAlpha(0.3f));
            float dashLen = 4.f;
            for (float dx = ix; dx < ix + iw; dx += dashLen * 2.f)
                g.drawLine(dx, midY, jmin(dx + dashLen, ix + iw), midY, 1.f);
        }
    }

    // Border
    if (sel) {
        g.setColour(Colours::white.withAlpha(0.95f));
        g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.5f);
    } else {
        g.setColour(kAutomCol.withAlpha(0.7f));
        g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.f);
    }

    // Param label (top-left)
    if (w >= 28 && h >= 9) {
        const auto& lane = b.automationLane;
        g.setColour(kAutomCol.withAlpha(0.75f));
        g.setFont(Font(8.f, Font::bold));
        // Prefer the display resolver so on-grid label tracks the user's rename
        // and effect-swap state; fall back to paramId; finally "Auto".
        String label;
        if (onResolveDisplayName) label = onResolveDisplayName(lane);
        if (label.isEmpty()) label = lane.paramId;
        if (label.isEmpty()) label = "Auto";
        g.drawText(label, x + 4, y + 1, w - 8, h - 2, Justification::centredLeft, true);
    }

    // Muted overlay - 2026-04-26 (D-1): 30% black wash + diagonal hatch.
    if (b.muted)
    {
        g.setColour(Colour(0x4d000000));   // ~30% alpha black
        g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);

        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);
        g.reduceClipRegion(clip);
        g.setColour(Colours::white.withAlpha(0.16f));
        const float stride = 8.f;
        for (float dx = -h; dx < w + h; dx += stride)
        {
            Path line;
            line.startNewSubPath((float)x + dx, (float)y);
            line.lineTo((float)x + dx + h, (float)y + h);
            g.strokePath(line, PathStrokeType(2.0f));
        }
    }

    // Resize handle
    g.setColour(kAutomCol.withAlpha(0.5f));
    g.fillRoundedRectangle((float)(x + w - 5), (float)(y + 3), 3.f, (float)(h - 6), 1.5f);
}

void ArrangementGrid::drawBlocks(Graphics& g) const
{
    int n = mPM.getNumBlocks();
    const int visTop = rulerPinY() + kRulerH;   // pinned band occludes above this
    for (int i = 0; i < n; ++i)
    {
        const auto& b = mPM.getBlock(i);
        // QA-Ea Task 0c (2026-05-20): use effectiveStartBars so slip-edited
        // clips render at their actual visual position (sub-bar precision +
        // possibly negative for clips slipped into the pre-roll zone).
        int bx = barToX((float) effectiveStartBars(b));
        int bw = jmax(4, barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - bx - 1);
        int by = rowToY(b.trackRow) + 2;
        int bh = rowHeightPx(b.trackRow) - 4;

        if (bx + bw < 0 || bx > getWidth()) continue;
        if (by + bh < visTop || by > getHeight()) continue;

        bool sel = isSelected(i);
        switch (b.clipType)
        {
            case ClipType::Pattern:    drawPatternClip   (g, b, bx, by, bw, bh, sel); break;
            case ClipType::Audio:      drawAudioClip     (g, b, bx, by, bw, bh, sel); break;
            case ClipType::Automation: drawAutomationClip(g, b, bx, by, bw, bh, sel); break;
        }

        // B-4: visible seam at a continuation piece's LEFT edge (a sliced right
        // piece plays offset content -- mark the cut so a split isn't invisible).
        const bool continued =
            (b.clipType == ClipType::Pattern && b.contentOffsetTicks   != 0)
         || (b.clipType == ClipType::Audio   && b.contentStartSamples  != 0);
        if (continued)
        {
            g.setColour(Colours::white.withAlpha(0.55f));
            g.fillRect(bx, by, 2, bh);
        }
    }
}

void ArrangementGrid::drawMarquee(Graphics& g) const
{
    if (mMarqueeActive)
    {
        g.setColour(VC::Highlight.withAlpha(0.15f));
        g.fillRect(mMarqueeRect);
        g.setColour(VC::Highlight.withAlpha(0.8f));
        g.drawRect(mMarqueeRect, 1);
    }
    // 2026-04-26 (D-1): zoom-rect overlay (Ctrl+RClick drag).
    if (mZoomRectActive && ! mZoomRect.isEmpty())
    {
        g.setColour(Colours::white.withAlpha(0.18f));
        g.fillRect(mZoomRect);
        g.setColour(Colours::white.withAlpha(0.5f));
        g.drawRect(mZoomRect, 1);
    }
    // B-4: slice drag-line preview (line + two dots, mirrors the piano-roll slice).
    if (mSlicing)
    {
        g.setColour(Colours::white.withAlpha(0.9f));
        g.drawLine((float) mSliceStart.x, (float) mSliceStart.y,
                   (float) mSliceEnd.x,   (float) mSliceEnd.y, 2.0f);
        g.fillEllipse((float) mSliceStart.x - 3.f, (float) mSliceStart.y - 3.f, 6.f, 6.f);
        g.fillEllipse((float) mSliceEnd.x   - 3.f, (float) mSliceEnd.y   - 3.f, 6.f, 6.f);
    }
}

void ArrangementGrid::drawPreviewBlock(Graphics& g) const
{
    if (!mDrawing || mDrawRow < 0 || mDrawRow >= kNumRows) return;
    float len = snapBarAlt(mDrawEnd) - snapBarAlt(mDrawStart);
    if (len < 1.f) len = 1.f;
    int x = barToX(snapBarAlt(mDrawStart));
    int w = jmax(4, barToX(snapBarAlt(mDrawStart) + len) - x - 1);
    int y = rowToY(mDrawRow) + 2;
    int h = rowHeightPx(mDrawRow) - 4;

    g.setColour(VC::Highlight.withAlpha(0.45f));
    g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);
    g.setColour(VC::Highlight.withAlpha(0.85f));
    g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.f);
}

void ArrangementGrid::drawGhostClip(Graphics& g) const
{
    if (!mHasGhost) return;
    const auto& b = mGhostBlock;
    int x = barToX((float) effectiveStartBars(b));
    int w = jmax(4, barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - x - 1);
    int y = rowToY(b.trackRow) + 2;
    int h = rowHeightPx(b.trackRow) - 4;

    g.setColour(VC::Highlight.withAlpha(mGhostAlpha * 0.7f));
    g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);
    g.setColour(VC::Highlight.withAlpha(mGhostAlpha));
    g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.5f);

    // File drag ghost shows the filename
    if (mFileDragActive && b.clipType == ClipType::Audio) {
        g.setColour(Colours::white.withAlpha(0.85f));
        g.setFont(Font(9.f, Font::bold));
        String fname = File(b.audioFilePath).getFileName();
        g.drawText(fname, x + 4, y + 1, w - 8, h - 2, Justification::centredLeft, true);
    }
}

void ArrangementGrid::drawPlayheadOverlay(Graphics& g) const
{
    if (mPlayheadBar < 0.0) return;
    int px = barToX((float)mPlayheadBar);
    if (px < 0 || px >= getWidth()) return;

    // Green line from ruler bottom to grid bottom only (arrow drawn in drawRuler so it's on top).
    // #30 (QA-G3Smoke, final form per Jeff): left-anchored mast -- the line's
    // left edge IS the position (matches the roll/kit markers; whole at bar 0).
    g.setColour(VC::Green.withAlpha(0.8f));
    g.fillRect(px, kRulerH, 1, getHeight() - kRulerH);   // 1-px mast: exact overlay on a grid-line column
}

void ArrangementGrid::drawPerformanceOverlays(Graphics& g) const
{
    if (!mPerfMode || mPlayheadBar < 0.0) return;

    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b = mPM.getBlock(i);
        if (effectiveStartBars(b) > mPlayheadBar
            || effectiveStartBars(b) + effectiveLengthBars(b) <= mPlayheadBar) continue;

        int bx = barToX((float) effectiveStartBars(b));
        int bw = jmax(4, barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - bx - 1);
        int by = rowToY(b.trackRow) + 2;
        int bh = rowHeightPx(b.trackRow) - 4;

        float progress = (float)(mPlayheadBar - effectiveStartBars(b)) / (float) juce::jmax(0.001, effectiveLengthBars(b));
        int progW = (int)(bw * progress);

        // Progress tint
        g.setColour(VC::Green.withAlpha(0.18f));
        g.fillRect(bx, by, progW, bh);

        // Progress edge line
        g.setColour(VC::Green.withAlpha(0.9f));
        g.drawVerticalLine(bx + progW, (float)by, (float)(by + bh));
    }
}

void ArrangementGrid::paint(Graphics& g)
{
    g.fillAll(kGridBg);

    drawRowBgs(g);
    drawGrid(g);
    drawBlocks(g);
    drawPreviewBlock(g);
    drawGhostClip(g);
    drawMarquee(g);
    drawPlayheadOverlay(g);
    drawPerformanceOverlays(g);
    // Time-selection column wash over the body; lives here (not drawRuler)
    // so the pinned band's offset never displaces a content-space fill.
    if (mTimeSelStart >= 0.f && mTimeSelEnd > mTimeSelStart)
    {
        const int sx = barToX(mTimeSelStart);
        const int ex = barToX(mTimeSelEnd);
        g.setColour(VC::Highlight.withAlpha(0.07f));
        g.fillRect(sx, kRulerH, ex - sx, getHeight() - kRulerH);
    }
    drawRuler(g);   // ruler on top (band pinned to the visible top)
}

// ─────────────────────────────────────────────────────────────────────────────
// Undo / Redo
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::beginEdit(const juce::String& label)
{
    mPendingLabel = label;
    mPendingBlocks.clear();
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
        mPendingBlocks.push_back(mPM.getBlock(i));
    mPendingRowNames = mPM.getRowNames();
    // QA-G: capture the before-edit per-row state so undo of Move/Insert-track
    // restores mute/solo/group with the blocks.
    mPendingRowGroups.clear(); mPendingRowColors.clear();
    mPendingRowMuted.clear();  mPendingRowSoloed.clear();
    for (int r = 0; r < kNumRows; ++r)
    {
        mPendingRowGroups.push_back (mPM.getRowGroup (r));
        mPendingRowColors.push_back (mPM.getRowGroupColor (r));
        mPendingRowMuted .push_back (mPM.isRowMuted (r)  ? (char) 1 : (char) 0);
        mPendingRowSoloed.push_back (mPM.isRowSoloed (r) ? (char) 1 : (char) 0);
    }
}

void ArrangementGrid::commitEdit()
{
    if (!mUndoCtx.isValid()) return;

    std::vector<ArrangementBlock> afterBlocks;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
        afterBlocks.push_back(mPM.getBlock(i));

    // QA-G: capture after-edit per-row state (mute/solo/group) for the redo side.
    std::vector<int>  aGroups;  std::vector<juce::uint32> aColors;
    std::vector<char> aMuted;   std::vector<char>         aSoloed;
    for (int r = 0; r < kNumRows; ++r)
    {
        aGroups.push_back (mPM.getRowGroup (r));
        aColors.push_back (mPM.getRowGroupColor (r));
        aMuted .push_back (mPM.isRowMuted (r)  ? (char) 1 : (char) 0);
        aSoloed.push_back (mPM.isRowSoloed (r) ? (char) 1 : (char) 0);
    }

    auto* action = new ArrangementEditAction(
        mPendingLabel,
        { mPendingBlocks, std::vector<juce::String>(mPendingRowNames.begin(), mPendingRowNames.end()),
          mPendingRowGroups, mPendingRowColors, mPendingRowMuted, mPendingRowSoloed },
        { afterBlocks,    std::vector<juce::String>(mPM.getRowNames().begin(), mPM.getRowNames().end()),
          aGroups, aColors, aMuted, aSoloed },
        [this](const ArrangementEditAction::Snapshot& s) {
            std::array<juce::String, kNumRows> rn;
            for (int i = 0; i < kNumRows && i < (int)s.rowNames.size(); ++i)
                rn[i] = s.rowNames[i];
            applySnapshot(s.blocks, rn, s.rowGroups, s.rowColors, s.rowMuted, s.rowSoloed);
        });

    mUndoCtx.perform(action, mPendingLabel);
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
    // QA-G Task 6: linked markers follow their blocks (delete/undo removal
    // half) + follower signatures re-derive after any block mutation.
    mPM.cleanupLinkedMarkers();
    if (onArrangementChanged)   onArrangementChanged();
}

void ArrangementGrid::applySnapshot(const std::vector<ArrangementBlock>& blocks,
                                    const std::array<juce::String, kNumRows>& rowNames,
                                    const std::vector<int>& rowGroups,
                                    const std::vector<juce::uint32>& rowColors,
                                    const std::vector<char>& rowMuted,
                                    const std::vector<char>& rowSoloed)
{
    while (mPM.getNumBlocks() > 0) mPM.removeBlock(0);
    for (const auto& b : blocks) mPM.addBlock(b);
    for (int i = 0; i < kNumRows; ++i)
        mPM.setRowName(i, rowNames[i]);
    // QA-G: restore per-row mute/solo/group so undo of Move/Insert-track is
    // complete (blocks + names alone left mute/group desynced by a row).
    for (int r = 0; r < kNumRows; ++r)
    {
        if (r < (int) rowGroups.size()) mPM.setRowGroup      (r, rowGroups[(size_t) r]);
        if (r < (int) rowColors.size()) mPM.setRowGroupColor (r, rowColors[(size_t) r]);
        if (r < (int) rowMuted.size())  mPM.setRowMuted      (r, rowMuted[(size_t) r]  != 0);
        if (r < (int) rowSoloed.size()) mPM.setRowSoloed     (r, rowSoloed[(size_t) r] != 0);
    }
    mSelection.clear();
    resized();
    repaint();
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
    mPM.cleanupLinkedMarkers();   // QA-G Task 6: undo/redo marker + follower sync
    // G1 boundary smoke: undo/redo restore lands HERE, not in commitEdit -
    // without this the audio players keep the pre-undo clip state (a stretch
    // undo reverted the block visually but kept PLAYING stretched).  Fires a
    // second time on a fresh commit (perform + commitEdit) - harmless, the
    // rebuild is idempotent.
    if (onArrangementChanged) onArrangementChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Selection
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::selectAll()
{
    mSelection.clear();
    // 2026-04-26 (D-7 sub-4 follow-up): range-aware Ctrl+A.  When a ruler
    // range is set, grab only the blocks that overlap [t0, t1) (same rule
    // the ruler-release auto-select uses).  No range -> select every block.
    if (mTimeSelStart >= 0.f && mTimeSelEnd > mTimeSelStart)
    {
        const float t0 = std::min (mTimeSelStart, mTimeSelEnd);
        const float t1 = std::max (mTimeSelStart, mTimeSelEnd);
        for (int i = 0; i < mPM.getNumBlocks(); ++i)
        {
            const auto& b = mPM.getBlock (i);
            const float bs = (float) effectiveStartBars (b);
            const float be = bs + (float) effectiveLengthBars (b);
            if (bs < t1 && be > t0)
                mSelection.push_back (i);
        }
    }
    else
    {
        for (int i = 0; i < mPM.getNumBlocks(); ++i) mSelection.push_back(i);
    }
    repaint();
}

void ArrangementGrid::clearSelection()
{
    mSelection.clear();
    repaint();
}

void ArrangementGrid::finaliseMarquee()
{
    mMarqueeActive = false;
    if (mMarqueeRect.getWidth() < 4 && mMarqueeRect.getHeight() < 4) { repaint(); return; }

    mSelection.clear();
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b = mPM.getBlock(i);
        int bx = barToX((float) effectiveStartBars(b));
        int bw = barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - bx;
        int by = rowToY(b.trackRow);
        if (mMarqueeRect.intersects(Rectangle<int>(bx, by, bw, rowHeightPx(b.trackRow))))
            mSelection.push_back(i);
    }
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Operations
// ─────────────────────────────────────────────────────────────────────────────
// QA-ProjectSave (2026-07-26, Jeff): shared by the Builder grid's own
// last-point delete and by the Event Editor's, so both routes ask the same
// question and take the same action.  An automation with no points controls
// nothing, so the choice is "remove the block" or "keep the point you have".
void ArrangementGrid::promptDeleteWholeAutomation (int blockIdx)
{
    if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return;

    juce::Component::SafePointer<ArrangementGrid> safeThis (this);
    juce::NativeMessageBox::showOkCancelBox (
        juce::MessageBoxIconType::QuestionIcon,
        "Delete Automation",
        "That is the last point in this automation.\n\n"
        "Delete the whole automation and remove it from the arrangement?",
        nullptr,
        juce::ModalCallbackFunction::create ([safeThis, blockIdx] (int result)
        {
            if (result != 1 || ! safeThis) return;
            auto* g = safeThis.getComponent();
            if (blockIdx >= g->mPM.getNumBlocks()) return;
            g->beginEdit ("Delete Automation");
            g->mPM.removeBlock (blockIdx);
            g->commitEdit();
            g->mSelection.clear();
            g->resized();
            g->repaint();
        }));
}

void ArrangementGrid::deleteSelected()
{
    if (mSelection.empty()) return;
    beginEdit("Delete");
    std::sort(mSelection.rbegin(), mSelection.rend());
    for (int idx : mSelection) mPM.removeBlock(idx);
    commitEdit();
    mSelection.clear();
    resized(); repaint();
}

// Ctrl+Left / Ctrl+Right - shift the ruler time-selection box by its own
// length.  After the shift, re-populate `mSelection` from the new range
// using the same overlap rule as the ruler-release auto-select so the
// visual highlight stays in sync.  No-op when no range is set; clamps to
// bar 0 on left shift.
void ArrangementGrid::shiftTimeSelectionLeft()  { shiftTimeSelectionByLength (-1); }
void ArrangementGrid::shiftTimeSelectionRight() { shiftTimeSelectionByLength (+1); }

void ArrangementGrid::shiftTimeSelectionByLength (int direction)
{
    if (mTimeSelStart < 0.f || mTimeSelEnd <= mTimeSelStart) return;
    const float t0 = std::min (mTimeSelStart, mTimeSelEnd);
    const float t1 = std::max (mTimeSelStart, mTimeSelEnd);
    const float len = t1 - t0;
    if (len <= 0.f) return;
    float newStart = t0 + (float) direction * len;
    if (newStart < 0.f) newStart = 0.f;
    const float newEnd = newStart + len;

    mTimeSelStart  = newStart;
    mTimeSelEnd    = newEnd;
    mTimeSelAnchor = newStart;

    mSelection.clear();
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b = mPM.getBlock (i);
        const float bs = (float) effectiveStartBars (b);
        const float be = bs + (float) effectiveLengthBars (b);
        if (bs < newEnd && be > newStart)
            mSelection.push_back (i);
    }
    repaint();
}

// 2026-04-26 (D-7): Ctrl+Delete on the builder.  Ruler range wins (its
// width is exactly what disappears - including any empty bars beyond the
// last selected block); selection bounds are the fallback when no ruler
// range is set.  Erase rule is "block STARTS in [t0, t1)" - matches what
// the ruler-release auto-select highlights.  Bars are integers (removedBars
// = round(t1 - t0)).
void ArrangementGrid::deleteTimeRegion()
{
    float t0 = 0.f, t1 = 0.f;
    bool  haveRange = false;
    if (mTimeSelStart >= 0.f && mTimeSelEnd > mTimeSelStart)
    {
        t0 = std::min(mTimeSelStart, mTimeSelEnd);
        t1 = std::max(mTimeSelStart, mTimeSelEnd);
        haveRange = true;
    }
    else if (!mSelection.empty())
    {
        t0 =  std::numeric_limits<float>::max();
        t1 = -std::numeric_limits<float>::max();
        for (int idx : mSelection)
        {
            if (idx < 0 || idx >= mPM.getNumBlocks()) continue;
            const auto& b = mPM.getBlock(idx);
            const float bs = (float) effectiveStartBars (b);
            const float be = bs + (float) effectiveLengthBars(b);
            t0 = std::min(t0, bs);
            t1 = std::max(t1, be);
        }
        haveRange = (t1 > t0);
    }
    if (!haveRange) return;

    const int removedBars = (int) std::round(t1 - t0);
    if (removedBars <= 0) return;

    beginEdit("Delete Time");

    constexpr float kEps = 1.0e-4f;
    // 1) erase blocks whose START lies in [t0, t1).
    for (int i = mPM.getNumBlocks() - 1; i >= 0; --i)
    {
        const float bs = (float) effectiveStartBars (mPM.getBlock(i));
        if (bs >= t0 - kEps && bs < t1 - kEps)
            mPM.removeBlock(i);
    }
    // 2) slide every block that starts at or after t1 left by removedBars.
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        auto& b = mPM.getBlock(i);
        if ((float) effectiveStartBars (b) >= t1 - kEps)
            b.startBeats = juce::jmax (0.0, b.startBeats - (double) removedBars * 4.0);
    }

    mSelection.clear();
    mTimeSelStart = mTimeSelEnd = -1.f;
    commitEdit();
    resized(); repaint();
}

void ArrangementGrid::copySelected()
{
    if (mSelection.empty()) return;
    mClipboard.clear();
    float earliest = 1e9f;
    for (int idx : mSelection)
        if (idx < mPM.getNumBlocks())
            earliest = jmin(earliest, (float) effectiveStartBars (mPM.getBlock(idx)));
    for (int idx : mSelection) {
        if (idx >= mPM.getNumBlocks()) continue;
        auto b = mPM.getBlock(idx);
        b.startBeats -= (double)(int) earliest * 4.0;
        mClipboard.push_back(b);
    }
}

void ArrangementGrid::pasteClipboard()
{
    if (mClipboard.empty()) return;
    beginEdit("Paste");
    mSelection.clear();
    int pasteStart = jmax(0, (int)snapBar(mBarOff));
    for (auto b : mClipboard) {
        b.startBeats += (double) pasteStart * 4.0;
        mPM.addBlock(b);
        mSelection.push_back(mPM.getNumBlocks() - 1);
        afterPatternBlockPlaced(b);   // QA-G Task 6
    }
    commitEdit();
    resized(); repaint();
}

void ArrangementGrid::duplicateSelected()
{
    // ── Timeline-based duplicate (when a time-selection exists) ───────────
    // The user has marked a range on the ruler. Copy every block whose
    // start lies inside that range and paste it directly after the range
    // (offset by the range length). Selection is replaced with the new
    // copies. This is independent of mSelection so the user can duplicate
    // a stretch of arrangement without first marquee-selecting blocks.
    if (mTimeSelStart >= 0.f && mTimeSelEnd > mTimeSelStart)
    {
        const float selStart = std::min(mTimeSelStart, mTimeSelEnd);
        const float selEnd   = std::max(mTimeSelStart, mTimeSelEnd);
        const float selLen   = selEnd - selStart;
        if (selLen <= 0.f) return;

        std::vector<ArrangementBlock> newBlocks;
        const int n = mPM.getNumBlocks();
        for (int i = 0; i < n; ++i)
        {
            const auto& src = mPM.getBlock(i);
            if ((float) effectiveStartBars (src) >= selStart && (float) effectiveStartBars (src) < selEnd)
            {
                ArrangementBlock nb = src;
                nb.startBeats = src.startBeats + (double)(int) selLen * 4.0;
                newBlocks.push_back(nb);
            }
        }
        if (newBlocks.empty()) return;

        beginEdit("Duplicate (Timeline)");
        mSelection.clear();
        for (auto& nb : newBlocks) {
            mPM.addBlock(nb);
            mSelection.push_back(mPM.getNumBlocks() - 1);
            afterPatternBlockPlaced(nb);   // QA-G Task 6
        }
        // Shift the time-selection to the new range so a second Ctrl+B repeats
        // the duplication forward.
        mTimeSelStart += selLen;
        mTimeSelEnd   += selLen;
        commitEdit();
        resized(); repaint();
        return;
    }

    // ── Selection-based duplicate (legacy path, no time-selection) ────────
    if (mSelection.empty()) return;
    beginEdit("Duplicate");
    float rightmost = 0.f;
    for (int idx : mSelection)
        if (idx < mPM.getNumBlocks())
            rightmost = jmax(rightmost, (float)(effectiveStartBars(mPM.getBlock(idx)) + effectiveLengthBars(mPM.getBlock(idx))));

    float earliest = 1e9f;
    for (int i2 : mSelection)
        if (i2 < mPM.getNumBlocks())
            earliest = jmin(earliest, (float) effectiveStartBars (mPM.getBlock(i2)));

    std::vector<ArrangementBlock> newBlocks;
    for (int idx : mSelection) {
        if (idx >= mPM.getNumBlocks()) continue;
        auto b = mPM.getBlock(idx);
        b.startBeats = ((double)(int) rightmost * 4.0)
                     + (mPM.getBlock(idx).startBeats - (double)(int) earliest * 4.0);
        newBlocks.push_back(b);
    }
    mSelection.clear();
    for (auto& nb : newBlocks) {
        mPM.addBlock(nb);
        mSelection.push_back(mPM.getNumBlocks() - 1);
        afterPatternBlockPlaced(nb);   // QA-G Task 6
    }
    commitEdit();
    resized(); repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Split by Player Engine (QA-G, owner docket 5/5a/5b, 2026-07-17)
// ─────────────────────────────────────────────────────────────────────────────
// QA-UndoCoverage Task 4: the Split-by-Engine local SplitState /
// SplitPatternUndoAction pair was promoted to PatternListSnapshot /
// PatternListAction (UndoActions.h) so browser pattern ops and the transport
// dropdown share the slice.  Linked TS markers stay outside the undo domain
// (established seam).
PatternListSnapshot ArrangementGrid::capturePatternSlice() const
{
    PatternListSnapshot s;
    for (int i = 0; i < mPM.getNumPatterns(); ++i) s.patterns.push_back (mPM.getPattern (i));
    s.currentPattern = mPM.getCurrentPatternIndex();
    for (int i = 0; i < mPM.getNumBlocks(); ++i)   s.blocks.push_back (mPM.getBlock (i));
    for (int r = 0; r < kNumRows; ++r)
    {
        s.rowNames.push_back (mPM.getRowNames()[(size_t) r]);
        s.rowGroups.push_back (mPM.getRowGroup (r));
        s.rowColors.push_back (mPM.getRowGroupColor (r));
        s.rowMuted.push_back (mPM.isRowMuted (r) ? 1 : 0);
        s.rowSoloed.push_back (mPM.isRowSoloed (r) ? 1 : 0);
    }
    return s;
}

void ArrangementGrid::applyPatternSlice (const PatternListSnapshot& s)
{
    // A degenerate (empty) snapshot must not wipe the project -- capture can
    // only be empty when it ran before the grid existed, which no user
    // gesture can reach.
    if (s.patterns.empty() || (int) s.rowNames.size() < kNumRows) return;

    mPM.restorePatternList (s.patterns, s.currentPattern);
    while (mPM.getNumBlocks() > 0) mPM.removeBlock (0);
    for (const auto& b : s.blocks) mPM.addBlock (b);
    for (int r = 0; r < kNumRows; ++r)
    {
        mPM.setRowName (r, s.rowNames[(size_t) r]);
        mPM.setRowMuted (r, s.rowMuted[(size_t) r] != 0);
        mPM.setRowSoloed(r, s.rowSoloed[(size_t) r] != 0);
        mPM.setRowGroup (r, s.rowGroups[(size_t) r]);
        mPM.setRowGroupColor (r, s.rowColors[(size_t) r]);
    }
    mSelection.clear();
    mPM.cleanupLinkedMarkers();
    resized(); repaint();
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
    if (onArrangementChanged)   onArrangementChanged();
}

void ArrangementGrid::performPatternSliceOp (const juce::String& label,
                                             const std::function<void()>& op)
{
    if (! mUndoCtx.isValid()) { op(); return; }

    PatternListSnapshot before = capturePatternSlice();
    op();
    PatternListSnapshot after = capturePatternSlice();
    mUndoCtx.perform (new PatternListAction (std::move (before), std::move (after),
                          [sp = juce::Component::SafePointer<ArrangementGrid> (this)]
                          (const PatternListSnapshot& s)
                          { if (sp != nullptr) sp->applyPatternSlice (s); }),
                      label);
}

void ArrangementGrid::performMarkerSetOp (const juce::String& label,
                                          const std::function<void()>& op)
{
    if (! mUndoCtx.isValid()) { op(); return; }

    auto capture = [this]() -> MarkerSetSnapshot
    {
        MarkerSetSnapshot s;
        for (int i = 0; i < mPM.getNumTimeMarkers(); ++i)    s.timeMarkers.push_back (mPM.getTimeMarker (i));
        for (int i = 0; i < mPM.getNumTimeSigChanges(); ++i) s.timeSigChanges.push_back (mPM.getTimeSigChange (i));
        for (int i = 0; i < mPM.getNumTempoChanges(); ++i)   s.tempoChanges.push_back (mPM.getTempoChange (i));
        s.currentTsUid = mPM.getCurrentTsMarkerUid();
        return s;
    };

    MarkerSetSnapshot before = capture();
    op();
    MarkerSetSnapshot after = capture();

    auto apply = [sp = juce::Component::SafePointer<ArrangementGrid> (this)]
                 (const MarkerSetSnapshot& s)
    {
        if (sp == nullptr) return;
        auto& pm = sp->mPM;
        pm.restoreTimeMarkers (s.timeMarkers);
        pm.restoreTimeSigState (s.timeSigChanges, s.currentTsUid);
        pm.restoreTempoChanges (s.tempoChanges);
        sp->repaint();
        if (sp->onTempoMapChanged) sp->onTempoMapChanged();
    };
    mUndoCtx.perform (new MarkerSetAction (std::move (before), std::move (after),
                                           std::move (apply)),
                      label);
}

void ArrangementGrid::performPatternTsOp (const juce::String& label,
                                          const std::function<void()>& op)
{
    if (! mUndoCtx.isValid()) { op(); return; }

    auto captureMarkers = [this]() -> MarkerSetSnapshot
    {
        MarkerSetSnapshot s;
        for (int i = 0; i < mPM.getNumTimeMarkers(); ++i)    s.timeMarkers.push_back (mPM.getTimeMarker (i));
        for (int i = 0; i < mPM.getNumTimeSigChanges(); ++i) s.timeSigChanges.push_back (mPM.getTimeSigChange (i));
        for (int i = 0; i < mPM.getNumTempoChanges(); ++i)   s.tempoChanges.push_back (mPM.getTempoChange (i));
        s.currentTsUid = mPM.getCurrentTsMarkerUid();
        return s;
    };

    PatternListSnapshot sliceBefore   = capturePatternSlice();
    MarkerSetSnapshot   markersBefore = captureMarkers();
    op();
    PatternListSnapshot sliceAfter   = capturePatternSlice();
    MarkerSetSnapshot   markersAfter = captureMarkers();

    auto sliceApply = [sp = juce::Component::SafePointer<ArrangementGrid> (this)]
                      (const PatternListSnapshot& s)
    { if (sp != nullptr) sp->applyPatternSlice (s); };

    auto markerApply = [sp = juce::Component::SafePointer<ArrangementGrid> (this)]
                       (const MarkerSetSnapshot& s)
    {
        if (sp == nullptr) return;
        auto& pm = sp->mPM;
        pm.restoreTimeMarkers (s.timeMarkers);
        pm.restoreTimeSigState (s.timeSigChanges, s.currentTsUid);
        pm.restoreTempoChanges (s.tempoChanges);
        sp->repaint();
        if (sp->onTempoMapChanged) sp->onTempoMapChanged();
    };

    // The slice opens the named transaction; the marker action appends into
    // the SAME ActionSet (no beginNewTransaction between performs), so one
    // Ctrl+Z reverses both.
    mUndoCtx.perform (new PatternListAction (std::move (sliceBefore), std::move (sliceAfter),
                                             std::move (sliceApply)),
                      label);
    mUndoCtx.manager->perform (new MarkerSetAction (std::move (markersBefore),
                                                    std::move (markersAfter),
                                                    std::move (markerApply)));
}

bool ArrangementGrid::rowIsBlank (int r) const
{
    if (r < 0 || r >= kNumRows) return false;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
        if (mPM.getBlock (i).trackRow == r) return false;
    if (mPM.getRowNames()[(size_t) r] != mPM.defaultRowName (r)) return false;
    if (mPM.isRowMuted (r) || mPM.isRowSoloed (r)) return false;
    if (mPM.getRowGroup (r) != 0) return false;
    return true;
}

bool ArrangementGrid::canInsertRows (int count) const
{
    if (count <= 0 || count >= kNumRows) return false;
    for (int r = kNumRows - count; r < kNumRows; ++r)
        if (! rowIsBlank (r)) return false;
    return true;
}

bool ArrangementGrid::insertBlankRowsAt (int startRow, int count)
{
    if (count <= 0 || startRow < 0 || startRow >= kNumRows) return false;
    if (! canInsertRows (count)) return false;
    for (int r = kNumRows - 1 - count; r >= startRow; --r)
    {
        const int dst = r + count;
        // Positional defaults ("Track N") stay positional; custom names travel.
        const juce::String src = mPM.getRowNames()[(size_t) r];
        mPM.setRowName (dst, src == mPM.defaultRowName (r) ? mPM.defaultRowName (dst) : src);
        mPM.setRowMuted (dst, mPM.isRowMuted (r));
        mPM.setRowSoloed(dst, mPM.isRowSoloed (r));
        mPM.setRowGroup (dst, mPM.getRowGroup (r));
        mPM.setRowGroupColor (dst, mPM.getRowGroupColor (r));
    }
    for (int k = 0; k < count; ++k)
    {
        const int r = startRow + k;
        mPM.setRowName (r, mPM.defaultRowName (r));
        mPM.setRowMuted (r, false);
        mPM.setRowSoloed(r, false);
        mPM.setRowGroup (r, 0);
        mPM.setRowGroupColor (r, 0);
    }
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        auto& b = mPM.getBlock (i);
        if (b.trackRow >= startRow) b.trackRow += count;
    }
    return true;
}

juce::String ArrangementGrid::engineTabName (int kind, int index) const
{
    if (onGetEngineTabName)
    {
        const juce::String n = onGetEngineTabName (kind, index);
        if (n.isNotEmpty()) return n;
    }
    static const char* kFallback[] = { "Layer", "Bass", "Drum", "Clips", "Inst" };
    return juce::String (kFallback[juce::jlimit (0, 4, kind)])
         + " " + juce::String (index + 1);
}

void ArrangementGrid::splitPatternByEngine (int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= mPM.getNumPatterns()) return;

    // One part per tab entry with MIDI data (docket 5a): every roll across
    // the five player-carrying families, kept in its ORIGINAL slot so the
    // same tab/engine keeps playing it.
    std::vector<std::pair<int,int>> parts;   // {kind, engineIndex}
    {
        const auto& pat = mPM.getPattern (patternIndex);
        for (int i = 0; i < (int) pat.layerRoll.size(); ++i)
            if (! pat.layerRoll[(size_t) i].notes.empty()) parts.push_back({ 0, i });
        for (int i = 0; i < (int) pat.bassRoll.size(); ++i)
            if (! pat.bassRoll[(size_t) i].notes.empty())  parts.push_back({ 1, i });
        for (int i = 0; i < (int) pat.drumRolls.size(); ++i)
            if (! pat.drumRolls[(size_t) i].notes.empty()) parts.push_back({ 2, i });
        for (int i = 0; i < (int) pat.clipRoll.size(); ++i)
            if (! pat.clipRoll[(size_t) i].notes.empty())  parts.push_back({ 3, i });
        for (int i = 0; i < (int) pat.instRoll.size(); ++i)
            if (! pat.instRoll[(size_t) i].notes.empty())  parts.push_back({ 4, i });
    }
    if (parts.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "Split by Player Engine", "This pattern has no MIDI data to split.");
        return;
    }

    // ONE group prompt per split op (owner lock) -- asked only when a
    // replaced block sits on a grouped row.
    bool anyGrouped = false;
    for (int i = 0; i < mPM.getNumBlocks() && ! anyGrouped; ++i)
    {
        const auto& b = mPM.getBlock (i);
        anyGrouped = (b.clipType == ClipType::Pattern
                      && b.patternIndex == patternIndex
                      && mPM.getRowGroup (b.trackRow) > 0);
    }

    if (anyGrouped)
    {
        auto* aw = new juce::AlertWindow ("Split by Player Engine",
            "Add the new tracks to the row group?",
            juce::MessageBoxIconType::QuestionIcon);
        aw->addButton ("Yes", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("No",  0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, patternIndex, parts] (int r)
            { performSplitByEngine (patternIndex, parts, r == 1); }), true);
    }
    else
    {
        performSplitByEngine (patternIndex, parts, false);
    }
}

void ArrangementGrid::performSplitByEngine (int patternIndex,
                                            const std::vector<std::pair<int,int>>& parts,
                                            bool joinGroups)
{
    if (patternIndex < 0 || patternIndex >= mPM.getNumPatterns() || parts.empty())
        return;

    const PatternListSnapshot before = capturePatternSlice();

    // 1) Create the split patterns (appended; the original's TS state, bar
    //    count, and color carry over; only the part's roll is copied).
    const juce::String origName = mPM.getPattern (patternIndex).name;
    std::vector<int> newIdx;
    for (const auto& part : parts)
    {
        const int ni = mPM.addPattern (origName + " - "
                                       + engineTabName (part.first, part.second));
        auto& np        = mPM.getPattern (ni);
        const auto& src = mPM.getPattern (patternIndex);
        np.bars = src.bars; np.stepsPerBar = src.stepsPerBar;
        np.tsNum = src.tsNum; np.tsDen = src.tsDen;
        np.tsLocked = src.tsLocked; np.tsBoundMarkerUid = src.tsBoundMarkerUid;
        np.color = src.color;
        switch (part.first)
        {
            case 0: np.layerRoll[(size_t) part.second] = src.layerRoll[(size_t) part.second]; break;
            case 1: np.bassRoll [(size_t) part.second] = src.bassRoll [(size_t) part.second]; break;
            case 2: np.drumRolls[(size_t) part.second] = src.drumRolls[(size_t) part.second]; break;
            case 3: np.clipRoll [(size_t) part.second] = src.clipRoll [(size_t) part.second]; break;
            case 4: np.instRoll [(size_t) part.second] = src.instRoll [(size_t) part.second]; break;
            default: break;
        }
        newIdx.push_back (ni);
    }

    // 2) Replace blocks row by row, bottom-most original row first so row
    //    insertions never shift rows still waiting to be processed.  Sibling
    //    rows: the N-1 directly below when ALL whole-row blank, else insert
    //    fresh rows there (owner rev: shift the in-the-way rows down,
    //    stealing blank rows from the bottom).
    const int nParts = (int) parts.size();
    std::vector<int> rowsWithBlocks;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b = mPM.getBlock (i);
        if (b.clipType == ClipType::Pattern && b.patternIndex == patternIndex)
            if (std::find (rowsWithBlocks.begin(), rowsWithBlocks.end(), b.trackRow)
                == rowsWithBlocks.end())
                rowsWithBlocks.push_back (b.trackRow);
    }
    std::sort (rowsWithBlocks.rbegin(), rowsWithBlocks.rend());

    bool capacityBlocked = false;
    for (int rowOrig : rowsWithBlocks)
    {
        if (nParts > 1)
        {
            if (rowOrig + nParts - 1 >= kNumRows) { capacityBlocked = true; break; }
            bool allBlank = true;
            for (int k = 1; k < nParts && allBlank; ++k)
                allBlank = rowIsBlank (rowOrig + k);
            if (! allBlank)
            {
                if (! canInsertRows (nParts - 1)) { capacityBlocked = true; break; }
                insertBlankRowsAt (rowOrig + 1, nParts - 1);
            }
        }
        const int nBlocks = mPM.getNumBlocks();
        for (int i = 0; i < nBlocks; ++i)
        {
            ArrangementBlock proto = mPM.getBlock (i);
            if (proto.clipType != ClipType::Pattern
                || proto.patternIndex != patternIndex
                || proto.trackRow != rowOrig) continue;
            mPM.getBlock (i).patternIndex = newIdx[0];
            for (int k = 1; k < nParts; ++k)
            {
                ArrangementBlock sib = proto;
                sib.patternIndex = newIdx[(size_t) k];
                sib.trackRow     = rowOrig + k;
                mPM.addBlock (sib);
            }
        }
        if (joinGroups && nParts > 1)
        {
            const int gid = mPM.getRowGroup (rowOrig);
            if (gid > 0)
            {
                const juce::uint32 col = mPM.getRowGroupColor (rowOrig);
                for (int k = 1; k < nParts; ++k)
                {
                    mPM.setRowGroup (rowOrig + k, gid);
                    mPM.setRowGroupColor (rowOrig + k, col);
                }
            }
        }
    }

    if (capacityBlocked)
    {
        // Owner spec: prompt + abort untouched.
        applyPatternSlice (before);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Maximum Tracks Reached",
            "Splitting needs more tracks than remain - all 500 rows are in use.");
        return;
    }

    // 3) The original dies (removePattern re-indexes blocks + linked markers;
    //    the new patterns sit above patternIndex so they shift down by one).
    if (mPM.getCurrentPatternIndex() == patternIndex)
        mPM.setCurrentPattern (newIdx[0]);
    mPM.removePattern (patternIndex);

    const PatternListSnapshot after = capturePatternSlice();
    if (mUndoCtx.isValid())
        mUndoCtx.perform (new PatternListAction (before, after,
                              [sp = juce::Component::SafePointer<ArrangementGrid> (this)]
                              (const PatternListSnapshot& s)
                              { if (sp != nullptr) sp->applyPatternSlice (s); }),
                          "Split by Player Engine");

    mSelection.clear();
    mPM.cleanupLinkedMarkers();
    resized(); repaint();
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
    if (onArrangementChanged)   onArrangementChanged();
}

void ArrangementGrid::nudgeSelection(int dBars, int dRows)
{
    if (mSelection.empty()) return;
    beginEdit("Nudge");
    for (int idx : mSelection) {
        if (idx >= mPM.getNumBlocks()) continue;
        auto& b = mPM.getBlock(idx);
        b.startBeats = jmax(0.0, b.startBeats + (double) dBars * 4.0);
        b.trackRow = jlimit(0, kNumRows - 1, b.trackRow + dRows);
    }
    commitEdit();
    resized(); repaint();
}

void ArrangementGrid::muteSelected(bool mute)
{
    if (mSelection.empty()) return;
    beginEdit(mute ? "Mute" : "Unmute");
    for (int idx : mSelection) {
        if (idx < mPM.getNumBlocks())
            mPM.getBlock(idx).muted = mute;
    }
    commitEdit();
    repaint();
}

// 2026-04-26 (D-1): Ctrl+Shift+RClick on a block → zoom horizontally so the
// block fills the viewport width.  Vertical zoom + scroll unchanged.
void ArrangementGrid::fitBlockToViewport(int blockIdx)
{
    if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return;
    const auto& b = mPM.getBlock(blockIdx);
    const float lenBars = (float) jmax(0.5, effectiveLengthBars(b));
    float vpW = 800.f;
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        vpW = (float)jmax(1, vp->getWidth());
    const float minPP = minZoomPPBar (vpW), maxPP = maxZoomPPBar (vpW);
    mPPBar  = jlimit(minPP, maxPP, vpW / lenBars);
    mBarOff = jmax(0.f, (float) effectiveStartBars (b));
    resized(); repaint();
}

// 2026-04-26 (D-2): TooltipClient - show marker label / TS info when the
// mouse hovers over a flag or TS pill on the ruler.  Returns empty string
// when the cursor is outside the ruler band so we don't overwrite tooltips
// owned by other components (block names etc).
juce::String ArrangementGrid::getTooltip()
{
    const auto pt = getMouseXYRelative();
    const int  ry = pt.y - rulerPinY();
    if (ry < 0 || ry >= kRulerH) return {};

    const float bar = xToBar (pt.x);
    const int   markerIdx = mPM.findTimeMarkerNearBar (bar, 0.5f);
    if (markerIdx >= 0)
    {
        const auto& m = mPM.getTimeMarker (markerIdx);
        const juce::String name = m.label.isNotEmpty() ? m.label : juce::String("(unnamed marker)");
        return "Marker: " + name + "  -  Bar " + juce::String (m.bar + 1)
             + "\nRight-click to edit / delete";
    }

    // QA-G Task 6: TS markers live at MAP bar indices.
    int tsBar = 0; double tsBib = 0.0;
    mPM.beatToBarAndBeatInBar (juce::jmax (0.0, (double) bar * 4.0), tsBar, tsBib);
    const int tsIdx = mPM.findTimeSigChangeAtBar (tsBar);
    if (tsIdx >= 0)
    {
        const auto& ts = mPM.getTimeSigChange (tsIdx);
        return "Time Signature: " + juce::String (ts.num) + "/" + juce::String (ts.den)
             + "  @ Bar " + juce::String (ts.bar + 1)
             + "\nRight-click to edit / delete";
    }

    const int tcIdx = mPM.findTempoChangeNearBar (bar, 0.5f);
    if (tcIdx >= 0)
    {
        const auto& tc = mPM.getTempoChange (tcIdx);
        return "Tempo: " + juce::String (tc.bpm, 1) + " BPM from Bar " + juce::String (tc.bar + 1)
             + "\nRight-click to edit / delete";
    }

    return {};
}

// 2026-04-26 (D-2): right-click on the Builder ruler.  If the click lands
// near an existing time-marker / TS-change, the menu offers Edit / Delete.
// Otherwise it offers Add at this bar (or at the playhead if the user
// invoked it via Alt+T / Shift+Alt+T - those reuse the prompt helpers).
void ArrangementGrid::showRulerContextMenu(int xPx)
{
    const float clickedBar  = xToBar(xPx);
    const int   snappedBar  = juce::jmax(0, (int) std::floor(clickedBar));
    // QA-G Task 6: TS markers live at MAP bar indices (bars resize at
    // markers); time markers + tempo flags stay uniform-bar time events.
    int tsBar = 0; double tsBib = 0.0;
    mPM.beatToBarAndBeatInBar(juce::jmax(0.0, (double) clickedBar * 4.0), tsBar, tsBib);

    const int existingMarker = mPM.findTimeMarkerNearBar(clickedBar, 0.5f);
    const int existingTS     = mPM.findTimeSigChangeAtBar(tsBar);
    const int existingTempo  = mPM.findTempoChangeNearBar(clickedBar, 0.5f);

    juce::PopupMenu m;
    if (existingMarker >= 0)
    {
        const auto& mk = mPM.getTimeMarker(existingMarker);
        m.addSectionHeader ("Marker: " + (mk.label.isNotEmpty() ? mk.label : juce::String("(unnamed)")));
        m.addItem (1, "Edit Marker...");
        m.addItem (2, "Delete Marker");
        m.addSeparator();
    }
    if (existingTS >= 0)
    {
        const auto& ts = mPM.getTimeSigChange(existingTS);
        m.addSectionHeader ("Time Sig: " + juce::String(ts.num) + "/" + juce::String(ts.den)
                            + "  @ Bar " + juce::String(ts.bar + 1));
        m.addItem (3, "Edit Time Signature...");
        m.addItem (4, "Delete Time Signature");
        m.addSeparator();
    }
    if (existingTempo >= 0)
    {
        const auto& tc = mPM.getTempoChange(existingTempo);
        m.addSectionHeader ("Tempo: " + juce::String(tc.bpm, 1) + " BPM  @ Bar "
                            + juce::String(tc.bar + 1));
        m.addItem (5, "Edit Tempo Change...");
        m.addItem (6, "Delete Tempo Change");
        m.addSeparator();
    }
    m.addItem (10, "Add Time Marker at Bar " + juce::String(snappedBar + 1) + "...");
    m.addItem (11, "Add Time Signature at Bar " + juce::String(tsBar + 1) + "...");
    m.addItem (12, "Add Tempo Change at Bar " + juce::String(snappedBar + 1) + "...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent(this),
        [this, snappedBar, tsBar, existingMarker, existingTS, existingTempo](int result)
        {
            if (result <= 0) return;
            if (result == 1 && existingMarker >= 0) promptRenameTimeMarker(existingMarker);
            else if (result == 2 && existingMarker >= 0)
            {
                performMarkerSetOp ("Delete Marker",
                                    [this, existingMarker] { mPM.removeTimeMarker(existingMarker); });
                repaint();
            }
            else if (result == 3 && existingTS >= 0)     promptEditTimeSigChange(existingTS);
            else if (result == 4 && existingTS >= 0)
            {
                performMarkerSetOp ("Delete Time Signature",
                                    [this, existingTS] { mPM.removeTimeSigChange(existingTS); });
                repaint();
                // Docket 4B: the current marker died with 2+ left -> re-ask.
                if (mPM.getCurrentTsMarkerUid() < 0)
                    showCurrentTsPicker();
            }
            else if (result == 5 && existingTempo >= 0)  promptEditTempoChange(existingTempo);
            else if (result == 6 && existingTempo >= 0)
            {
                performMarkerSetOp ("Delete Tempo Change",
                                    [this, existingTempo] { mPM.removeTempoChange(existingTempo); });
                if (onTempoMapChanged) onTempoMapChanged();
                repaint();
            }
            else if (result == 10) promptAddTimeMarker(snappedBar);
            else if (result == 11) promptAddTimeSigChange(tsBar);
            else if (result == 12) promptAddTempoChange(snappedBar);
        });
}

// QA-TempoMap (2026-07-08): tempo-flag prompts - mirror the D-2 prompt shape.
void ArrangementGrid::promptAddTempoChange(int bar)
{
    auto* aw = new juce::AlertWindow ("Add Tempo Change",
        "Tempo from Bar " + juce::String(bar + 1) + " onward (20-300 BPM):",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("bpm", "120");
    aw->addButton ("Add",    1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, bar, aw](int r)
        {
            if (r != 1) return;
            const double bpm = aw->getTextEditorContents("bpm").getDoubleValue();
            if (bpm <= 0.0) return;
            performMarkerSetOp ("Add Tempo Change",
                                [this, bar, bpm] { mPM.addTempoChange (bar, bpm); });
            if (onTempoMapChanged) onTempoMapChanged();
            repaint();
        }), true);
}

void ArrangementGrid::promptEditTempoChange(int idx)
{
    if (idx < 0 || idx >= mPM.getNumTempoChanges()) return;
    const auto cur = mPM.getTempoChange(idx);
    auto* aw = new juce::AlertWindow ("Edit Tempo Change",
        "Tempo from Bar " + juce::String(cur.bar + 1) + " onward (20-300 BPM):",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("bpm", juce::String(cur.bpm, 1));
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, cur, aw](int r)
        {
            if (r != 1) return;
            const double bpm = aw->getTextEditorContents("bpm").getDoubleValue();
            if (bpm <= 0.0) return;
            // addTempoChange replaces the existing change at the same bar.
            performMarkerSetOp ("Edit Tempo Change",
                                [this, &cur, bpm] { mPM.addTempoChange (cur.bar, bpm); });
            if (onTempoMapChanged) onTempoMapChanged();
            repaint();
        }), true);
}

void ArrangementGrid::promptAddTimeMarker(int bar)
{
    auto* aw = new juce::AlertWindow ("Add Time Marker",
        "Label for marker at Bar " + juce::String(bar + 1) + ":",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("label", "");
    aw->addButton ("Add",    1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, bar, aw](int r)
        {
            if (r != 1) return;
            const juce::String label = aw->getTextEditorContents("label").trim();
            performMarkerSetOp ("Add Time Marker",
                                [this, bar, &label] { mPM.addTimeMarker (bar, label); });
            repaint();
        }), true);
}

void ArrangementGrid::promptRenameTimeMarker(int idx)
{
    if (idx < 0 || idx >= mPM.getNumTimeMarkers()) return;
    const juce::String current = mPM.getTimeMarker(idx).label;
    auto* aw = new juce::AlertWindow ("Edit Marker",
        "New label:",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("label", current);
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, idx, aw](int r)
        {
            if (r != 1) return;
            const juce::String label = aw->getTextEditorContents("label").trim();
            performMarkerSetOp ("Edit Marker",
                                [this, idx, &label] { mPM.renameTimeMarker (idx, label); });
            repaint();
        }), true);
}

// QA-G Task 6 (docket B): placement events of USER-SET patterns spawn a
// linked marker at the block's map bar.  Same-bar existing markers win
// (manual or otherwise); follower patterns never spawn.
void ArrangementGrid::afterPatternBlockPlaced (const ArrangementBlock& b)
{
    if (b.clipType != ClipType::Pattern) return;
    if (b.patternIndex < 0 || b.patternIndex >= mPM.getNumPatterns()) return;
    const auto& pat = mPM.getPattern (b.patternIndex);
    if (! pat.tsLocked) return;
    int bar = 0; double bib = 0.0;
    mPM.beatToBarAndBeatInBar (jmax (0.0, effectiveStartBeats (b)), bar, bib);
    if (mPM.findTimeSigChangeAtBar (bar) >= 0) return;
    mPM.addTimeSigChange (bar, pat.tsNum, pat.tsDen, b.patternIndex);
    repaint();
}

// QA-G Task 6 (docket #4 revision): pick which marker is "current" (the one
// newly-created patterns bind to).  Fired after every marker add beyond the
// first, and after the current marker is deleted with 2+ left (4B).
void ArrangementGrid::showCurrentTsPicker()
{
    const int n = mPM.getNumTimeSigChanges();
    if (n < 2) return;
    juce::PopupMenu m;
    m.addSectionHeader ("Current time signature (for new patterns)");
    const int curUid = mPM.getCurrentTsMarkerUid();
    for (int i = 0; i < n; ++i)
    {
        const auto& ts = mPM.getTimeSigChange (i);
        m.addItem (i + 1, "Bar " + juce::String (ts.bar + 1) + "  -  "
                          + juce::String (ts.num) + "/" + juce::String (ts.den),
                   true, ts.uid == curUid);
    }
    m.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent (this),
        [this] (int result)
        {
            if (result <= 0 || result > mPM.getNumTimeSigChanges()) return;
            const int uid = mPM.getTimeSigChange (result - 1).uid;
            performMarkerSetOp ("Current Time Signature",
                                [this, uid] { mPM.setCurrentTsMarkerUid (uid); });
            repaint();
        });
}

void ArrangementGrid::promptAddTimeSigChange(int bar)
{
    auto* aw = new juce::AlertWindow ("Add Time Signature Change",
        "Time signature at Bar " + juce::String(bar + 1) + ":",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("num", "4");
    aw->addTextEditor ("den", "4");
    aw->addTextBlock  ("Format: numerator (1-32) / denominator (power of 2: 1, 2, 4, 8, 16, 32).");
    aw->addButton ("Add",    1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, bar, aw](int r)
        {
            if (r != 1) return;
            const int n = aw->getTextEditorContents("num").getIntValue();
            const int d = aw->getTextEditorContents("den").getIntValue();
            performMarkerSetOp ("Add Time Signature",
                                [this, bar, n, d] { mPM.addTimeSigChange (bar, n, d); });
            repaint();
            // Docket #4: every add beyond the first re-asks which marker is
            // current (the picker defaults its tick to the standing choice).
            showCurrentTsPicker();
        }), true);
}

void ArrangementGrid::promptEditTimeSigChange(int idx)
{
    if (idx < 0 || idx >= mPM.getNumTimeSigChanges()) return;
    const auto cur = mPM.getTimeSigChange(idx);
    auto* aw = new juce::AlertWindow ("Edit Time Signature",
        "Time signature at Bar " + juce::String(cur.bar + 1) + ":",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("num", juce::String(cur.num));
    aw->addTextEditor ("den", juce::String(cur.den));
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, cur, aw](int r)
        {
            if (r != 1) return;
            const int n = aw->getTextEditorContents("num").getIntValue();
            const int d = aw->getTextEditorContents("den").getIntValue();
            // addTimeSigChange replaces existing change at same bar.
            performMarkerSetOp ("Edit Time Signature",
                                [this, &cur, n, d] { mPM.addTimeSigChange (cur.bar, n, d); });
            repaint();
        }), true);
}

// 2026-04-26 (D-1): Alt+RClick → popup with Bar / Half-bar / Beat / Step
// quantize options.  Snaps each selected block's startBar to the chosen unit.
//
// NOTE: ArrangementBlock currently stores startBar as int, so sub-bar units
// (1/2 bar / beat / step) round to the same integer bar - effectively a
// no-op until startBar gets promoted to fractional alongside per-bar
// time-signature changes (D-2).  The math is written for fractional units so
// it Just Works once that migration lands.
void ArrangementGrid::showQuantizePopup()
{
    if (mSelection.empty()) return;
    juce::PopupMenu m;
    m.addSectionHeader("Quantize selection to nearest:");
    m.addItem(1, "Bar");
    m.addItem(2, "1/2 Bar");
    m.addItem(3, "Beat (1/4 bar)");
    m.addItem(4, "Step (1/16 bar)");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [this](int result)
        {
            if (result <= 0) return;
            const float unit = (result == 1) ? 1.f
                             : (result == 2) ? 0.5f
                             : (result == 3) ? 0.25f
                             : 0.0625f;
            beginEdit("Quantize");
            for (int idx : mSelection)
            {
                if (idx < 0 || idx >= mPM.getNumBlocks()) continue;
                auto& blk = mPM.getBlock(idx);
                const float startBars = (float) effectiveStartBars (blk);
                const float snapped   = std::round(startBars / unit) * unit;
                blk.startBeats = (double) jmax(0, (int) std::round(snapped)) * 4.0;
            }
            commitEdit();
            repaint();
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Context menus (Task 9)
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::showClipContextMenu(int blockIdx)
{
    if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return;
    const auto& b = mPM.getBlock(blockIdx);

    PopupMenu m;
    m.addItem(1, "Cut");
    m.addItem(2, "Copy");
    m.addItem(3, "Paste");
    m.addSeparator();
    m.addItem(4, "Delete");
    m.addSeparator();
    m.addItem(5, b.muted ? "Unmute" : "Mute");
    m.addSeparator();
    if (b.clipType == ClipType::Audio)
    {
        m.addItem(6, "Properties...");                 // QA-E Task 7 (FILE-02):
                                                        // renamed; now also hosts
                                                        // the Routing dropdown
        // G1 boundary (Jeff pick 1): escape hatch for the stretch state -
        // restores the clip's NATURAL tempo identity (its library entry's
        // BPM), keeping position + length.  Enabled only when re-fit.
        {
            const int   li      = mPM.findAudioLibraryIndexByPath (b.audioFilePath);
            const bool  canRst  = li >= 0
                && std::abs (b.originalBPM - mPM.getAudioLibraryBPM (li)) > 0.01f;
            m.addItem(7, "Reset Stretch", canRst);
        }
    }
    if (b.clipType == ClipType::Automation)
        m.addItem(8, "Open in Event Editor...");
    // QA-E Task 7 (FILE-02): dead duplicate item 7 deleted (no case 7 in the
    // result switch below -- it never did anything).

    // C.5b: Pattern blocks get a "Set Time Signature" submenu that overrides
    // the referenced pattern's intrinsic TS (also lockable via this path).
    if (b.clipType == ClipType::Pattern
        && b.patternIndex >= 0 && b.patternIndex < mPM.getNumPatterns())
    {
        m.addSeparator();
        juce::PopupMenu tsSub;
        const auto& pat = mPM.getPattern(b.patternIndex);
        const int curN = pat.tsNum;
        const int curD = pat.tsDen;
        struct TsOpt { int n, d; const char* lbl; };
        static const TsOpt kTsOpts[] = {
            {4,4,"4/4"}, {3,4,"3/4"}, {2,4,"2/4"}, {6,8,"6/8"},
            {5,4,"5/4"}, {7,8,"7/8"}, {12,8,"12/8"}, {9,8,"9/8"}
        };
        const int kTsIdBase = 100;   // 100..107
        for (int i = 0; i < 8; ++i)
        {
            const auto& o = kTsOpts[i];
            const bool tick = (curN == o.n && curD == o.d);
            tsSub.addItem (kTsIdBase + i, o.lbl, true, tick);
        }
        m.addSubMenu ("Set Time Signature", tsSub);
    }

    m.showMenuAsync(PopupMenu::Options(), [this, blockIdx](int result) {
        if (blockIdx >= mPM.getNumBlocks()) return;
        switch (result)
        {
            case 1: if (isSelected(blockIdx)) copySelected();
                    deleteSelected(); break;
            case 2: if (!isSelected(blockIdx)) { mSelection.clear(); mSelection.push_back(blockIdx); }
                    copySelected(); break;
            case 3: pasteClipboard(); break;
            case 4: if (!isSelected(blockIdx)) { mSelection.clear(); mSelection.push_back(blockIdx); }
                    deleteSelected(); break;
            case 5: { bool m2 = !mPM.getBlock(blockIdx).muted;
                      if (!isSelected(blockIdx)) { mSelection.clear(); mSelection.push_back(blockIdx); }
                      muteSelected(m2); break; }
            case 6: showAudioClipProperties(blockIdx); break;
            case 7: // Reset Stretch (G1 boundary, Jeff pick 1)
            {
                auto& b = mPM.getBlock(blockIdx);
                const int li = mPM.findAudioLibraryIndexByPath (b.audioFilePath);
                if (b.clipType == ClipType::Audio && li >= 0)
                {
                    beginEdit("Reset Stretch");
                    b.originalBPM = mPM.getAudioLibraryBPM (li);
                    // Jeff (G1 smoke): reset = the ORIGINAL DROP FORM in one
                    // click - natural speed AND full file length at the
                    // natural tempo, slip offset cleared.  Position kept.
                    juce::File af (b.audioFilePath);
                    if (! af.existsAsFile() && onResolveStoredPath)
                        af = onResolveStoredPath (b.audioFilePath);
                    if (af.existsAsFile())
                    {
                        juce::AudioFormatManager fm;
                        fm.registerBasicFormats();
                        if (std::unique_ptr<juce::AudioFormatReader> rd { fm.createReaderFor (af) })
                        {
                            if (rd->sampleRate > 0 && rd->lengthInSamples > 0
                                && b.originalBPM > 0.f)
                            {
                                const double secs  = (double) rd->lengthInSamples / rd->sampleRate;
                                const double beats = secs * (double) b.originalBPM / 60.0;
                                b.setLengthBeats (beats);
                                b.lengthBars = juce::jmax (1, (int) std::ceil (beats / 4.0));
                            }
                        }
                    }
                    b.contentStartSamples = 0;
                    // Follow-state is DERIVED (same rule as the Properties
                    // dialog): back on the library master only if every other
                    // per-copy prop matches it too.
                    b.isOverride = std::abs (b.pitchSemitones - mPM.getAudioLibraryPitch (li)) > 0.001f
                                || b.stretchMode != mPM.getAudioLibraryStretchMode (li);
                    commitEdit();   // fires onArrangementChanged -> rebuildAudioClipPlayers
                    repaint();
                }
                break;
            }
            case 8: if (onOpenEventEditor) onOpenEventEditor(blockIdx); break;
            default:
                // C.5b: 100..107 = TS preset picks for the block's pattern.
                if (result >= 100 && result <= 107)
                {
                    struct TsOpt { int n, d; };
                    static const TsOpt kTsOpts[] = {
                        {4,4},{3,4},{2,4},{6,8},{5,4},{7,8},{12,8},{9,8}
                    };
                    const int patIdx = mPM.getBlock(blockIdx).patternIndex;
                    const int optIdx = result - 100;
                    if (patIdx >= 0 && optIdx >= 0 && optIdx < 8)
                    {
                        performPatternTsOp ("Pattern Time Signature",
                                            [this, patIdx, optIdx] {
                            mPM.setPatternTimeSig (patIdx, kTsOpts[optIdx].n, kTsOpts[optIdx].d);
                        });
                        repaint();
                    }
                }
                break;
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-E Task 7 (FILE-02): the IDENTICAL Audio Properties box used by BOTH the
// per-clip grid Properties (ArrangementGrid::showAudioClipProperties) and the
// browser-entry Properties (BrowserPanel::showLibraryPropertiesDialog).  ONE
// builder so the two dialogs can never drift.  Adds Pitch / Original BPM /
// Mode + a "Routes to:" menu-tree button + Apply / Cancel.
//
// The routing control is a button that opens a PopupMenu of every Vox/Inst/
// Clips page + 3 "Add a new ___ Page" entries.  offerMove == true (browser
// dialog) gives each target a submenu { Move here / Copy here }; offerMove ==
// false (per-clip dialog) gives each target a single "Copy to <name>" item
// (per-clip only ever copies).  The pick is stored in outPending and shown
// on the button; nothing happens until the caller's Apply reads it.  outBtn
// / outPending are shared_ptr so the caller captures them in the modal lambda
// (their lifetime spans the modal; the AlertWindow does NOT own the custom
// component).
// ─────────────────────────────────────────────────────────────────────────────
namespace {
struct RouteTarget { int channelId; juce::String name; };  // chId>=0 page; -1/-2/-3 = new Clip/Vox/Inst

void buildAudioPropsControls (juce::AlertWindow& aw,
                              float curPitch, float curBPM, bool curStretch,
                              const juce::String& curRouteName,
                              const std::vector<RoutablePageInfo>& pages,
                              bool offerMove,
                              bool offerResetToMaster,
                              std::shared_ptr<juce::TextButton>& outBtn,
                              std::shared_ptr<PendingRoute>& outPending,
                              const juce::String& bpmDisplayOverride)
{
    aw.addTextEditor ("pitch", juce::String (curPitch, 2), "Pitch shift (semitones):");
    // G1 boundary (Jeff, teaching-app cut): the PER-CLIP dialog shows tempo
    // as a read-only detected display (bpmDisplayOverride non-empty) - the
    // number a stretch-drag rewrites is no longer hand-editable per copy.
    // The BROWSER entry keeps the editable field as the single correction
    // point for detection misses (octave errors need a human override).
    if (bpmDisplayOverride.isNotEmpty())
        aw.addTextBlock (bpmDisplayOverride);
    else
        aw.addTextEditor ("bpm", juce::String (curBPM, 1), "Original BPM:");

    aw.addComboBox ("mode", { "Stretch (pitch locked)", "Resample (pitch follows tempo)" }, "Mode:");
    if (auto* cb = aw.getComboBoxComponent ("mode"))
        cb->setSelectedItemIndex (curStretch ? 0 : 1, juce::dontSendNotification);

    auto targets = std::make_shared<std::vector<RouteTarget>>();
    for (const auto& pg : pages)
        targets->push_back ({ pg.channelId, pg.displayName });
    targets->push_back ({ -1, "a new Clip Page" });
    targets->push_back ({ -2, "a new Vox Page" });
    targets->push_back ({ -3, "a new Inst Page" });

    outPending = std::make_shared<PendingRoute>();
    outBtn     = std::make_shared<juce::TextButton>();
    outBtn->setSize (360, 26);
    const juce::String baseLabel = "Routes to: "
        + (curRouteName.isNotEmpty() ? curRouteName : juce::String ("(unrouted)"));
    outBtn->setButtonText (baseLabel);

    juce::Component::SafePointer<juce::TextButton> btnSafe (outBtn.get());
    auto pending = outPending;

    outBtn->onClick = [targets, offerMove, btnSafe, pending, baseLabel]
    {
        juce::PopupMenu m;
        // id encoding per target t: Move = 2t+1 (odd), Copy = 2t+2 (even).
        for (int t = 0; t < (int) targets->size(); ++t)
        {
            const auto& tg = (*targets)[(size_t) t];
            if (offerMove)
            {
                juce::PopupMenu sub;
                sub.addItem (2 * t + 1, "Move here");
                sub.addItem (2 * t + 2, "Copy here");
                m.addSubMenu (tg.name, sub);
            }
            else
            {
                m.addItem (2 * t + 2, "Copy to " + tg.name);
            }
        }

        m.showMenuAsync (juce::PopupMenu::Options(),
            [targets, btnSafe, pending, baseLabel] (int res)
            {
                if (res <= 0) return;
                const int  t     = (res - 1) / 2;
                const bool isCpy = ((res % 2) == 0);   // Copy id = 2t+2 (even)
                if (t < 0 || t >= (int) targets->size()) return;
                const auto& tg = (*targets)[(size_t) t];

                pending->chosen     = true;
                pending->isCopy     = isCpy;
                pending->channelId  = (tg.channelId >= 0) ? tg.channelId : -1;
                pending->createKind = (tg.channelId >= 0)
                                        ? -1
                                        : (tg.channelId == -1 ? 0
                                           : (tg.channelId == -2 ? 1 : 2));
                if (auto* b = btnSafe.getComponent())
                    b->setButtonText (baseLabel + "    [ "
                        + (isCpy ? juce::String ("Copy to ") : juce::String ("Move to "))
                        + tg.name + " ]");
            });
    };

    aw.addCustomComponent (outBtn.get());

    // "Apply" (not "OK") so it's clear nothing takes effect until pressed.
    aw.addButton ("Apply",  1);
    aw.addButton ("Cancel", 0);
    // QA-E Task 7 (FILE-02): per-clip dialog only -- explicit re-attach.
    // Snaps every setting back to the file's library (browser) master and
    // clears the override so the clip follows again (dot -> green).
    if (offerResetToMaster)
        aw.addButton ("Reset to Browser Entry", 2);
}
} // namespace

void ArrangementGrid::showAudioClipProperties(int blockIdx)
{
    if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return;
    const auto& block = mPM.getBlock(blockIdx);
    if (block.clipType != ClipType::Audio) return;

    juce::String curRouteName;
    std::vector<RoutablePageInfo> pages;
    if (onEnumerateRoutablePages) pages = onEnumerateRoutablePages();
    for (const auto& pg : pages)
        if (pg.channelId == block.routeChannel) { curRouteName = pg.displayName; break; }

    auto* aw = new AlertWindow("Audio Clip Properties",
                               "File: " + File(block.audioFilePath).getFileName()
                               + "\n\nThese settings apply to this clip only. "
                                 "Changing them stops this clip from following "
                                 "the file's master settings.",
                               AlertWindow::NoIcon);

    std::shared_ptr<juce::TextButton> routeBtn;
    std::shared_ptr<PendingRoute>     pending;
    // G1 boundary: detected-tempo display (path-cached so reopening the
    // dialog doesn't re-read the file; message thread only).
    juce::String bpmDisplay;
    {
        static std::map<juce::String, BpmDetect::Estimate> sBpmCache;
        auto it = sBpmCache.find (block.audioFilePath);
        if (it == sBpmCache.end())
        {
            BpmDetect::Estimate est;
            juce::File bf (block.audioFilePath);
            if (! bf.existsAsFile() && onResolveStoredPath)
                bf = onResolveStoredPath (block.audioFilePath);
            if (bf.existsAsFile())
            {
                juce::AudioFormatManager bfm;
                bfm.registerBasicFormats();
                if (std::unique_ptr<juce::AudioFormatReader> rd { bfm.createReaderFor (bf) })
                    est = BpmDetect::detect (*rd);
            }
            it = sBpmCache.emplace (block.audioFilePath, est).first;
        }
        bpmDisplay = it->second.confident
            ? "Detected tempo: ~" + juce::String (it->second.bpm, 1) + " BPM"
            : juce::String ("Detected tempo: (not detectable)");
    }

    // Owner call 2026-07-11: the grid dialog offers Move + Copy, same as the
    // browser dialog (offerMove == true).  Move relocates the shared library
    // entry (linked path -- see the Apply handler); Copy forks a duplicate.
    buildAudioPropsControls (*aw, block.pitchSemitones, block.originalBPM,
                             block.stretchMode, curRouteName, pages,
                             /*offerMove*/ true,
                             /*offerResetToMaster*/ true, routeBtn, pending,
                             bpmDisplay);

    aw->enterModalState(true,
        ModalCallbackFunction::create([this, blockIdx, aw, routeBtn, pending](int r) {
            if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return;
            if (r == 0) return;   // Cancel

            // Snapshot by value: commitEdit() rebuilds the block list via
            // applySnapshot (removeBlock/addBlock), so a block reference must
            // NOT be held across a commit.  Re-fetch by index per edit.
            const ArrangementBlock cur = mPM.getBlock(blockIdx);

            // r == 2: "Reset to Browser Entry" -- snap every setting back to
            // the file's library (browser) master and re-attach (follow
            // again).  Ignores the typed fields + any pending route pick.
            if (r == 2)
            {
                const int li = mPM.findAudioLibraryIndexByPath(cur.audioFilePath);
                if (li >= 0)
                {
                    beginEdit("Follow File Master");
                    auto& b = mPM.getBlock(blockIdx);
                    b.pitchSemitones = mPM.getAudioLibraryPitch(li);
                    b.originalBPM    = mPM.getAudioLibraryBPM(li);
                    b.stretchMode    = mPM.getAudioLibraryStretchMode(li);
                    b.routeChannel   = mPM.getAudioLibraryPageOwner(li);
                    b.isOverride     = false;            // follows again -> green
                    commitEdit();
                    repaint();
                }
                return;
            }

            if (r != 1) return;   // defensive (only 0/1/2 expected)

            const float newPitch  = aw->getTextEditorContents("pitch").getFloatValue();
            // G1 boundary: the per-clip dialog has no BPM editor anymore (the
            // tempo identity is display-only here) - the clip keeps its
            // current identity through Apply/Copy.
            const float bpmClamped = jmax(1.f, cur.originalBPM);
            bool  stretch = true;
            if (auto* cb = aw->getComboBoxComponent("mode"))
                stretch = (cb->getSelectedItemIndex() == 0);

            // Owner call 2026-07-11: per-clip Move mirrors the browser Move
            // EXACTLY -- resolve the target (creating a page bound to THIS
            // file if a "new page" entry was picked), then relocate via the
            // SAME onApplyLibraryProperties lambda the browser uses.  That
            // moves the library entry + every following copy as one, so grid
            // and browser stay linked.  (Copies the user customized
            // individually are detached and, like the browser, keep their
            // route until re-attached -- the dialog's "Reset to Browser
            // Entry" re-attaches.)
            if (pending && pending->chosen && ! pending->isCopy)
            {
                const int li = mPM.findAudioLibraryIndexByPath(cur.audioFilePath);
                int target = pending->channelId;
                if (pending->createKind >= 0 && onCreateRoutablePage)
                    target = onCreateRoutablePage(pending->createKind, cur.audioFilePath);
                if (li >= 0 && target >= 0 && onApplyLibraryProperties)
                    onApplyLibraryProperties(li, newPitch, bpmClamped, stretch, target);
                return;   // the Move was the action
            }

            // QA-E Task 7 (FILE-02): per-clip Copy.  Duplicate FIRST, THEN
            // create the new page bound to the DUPLICATE (so "Copy to a new
            // Clip Page" registers only the one dupe entry -- not the original
            // too).  Then tag it (dedup-safe) + THIS block BECOMES the copy.
            if (pending && pending->chosen && pending->isCopy && onDuplicateFileForCopy)
            {
                const juce::String np = onDuplicateFileForCopy(cur.audioFilePath);
                if (np.isNotEmpty())
                {
                    int target = pending->channelId;
                    if (pending->createKind >= 0 && onCreateRoutablePage)
                        target = onCreateRoutablePage(pending->createKind, np);
                    if (target >= 0)
                    {
                        if (onTagCopiedEntry)
                            onTagCopiedEntry(np, target, newPitch, bpmClamped, stretch);
                        if (blockIdx < mPM.getNumBlocks())
                        {
                            beginEdit("Audio Clip Copy");
                            auto& b = mPM.getBlock(blockIdx);
                            b.audioFilePath  = np;
                            b.routeChannel   = target;
                            b.pitchSemitones = newPitch;
                            b.originalBPM    = bpmClamped;
                            b.stretchMode    = stretch;
                            b.displayAlias   = {};        // new file -> new name
                            // The new library entry has exactly these props,
                            // so the block matches its own new master -> it
                            // FOLLOWS it (green), not detached.
                            b.isOverride     = false;
                            commitEdit();
                            repaint();
                        }
                    }
                }
                return;   // a Copy was the action; don't also do an in-place edit
            }

            // Plain prop edit.  isOverride is DERIVED: this copy is detached
            // (red) iff its resulting settings differ from the file's library
            // master -- so typing the master values back (or pressing Apply
            // when they already match) auto-re-attaches it (green) and it
            // resumes following browser-entry edits.
            const int li = mPM.findAudioLibraryIndexByPath(cur.audioFilePath);
            const bool matchesMaster = (li >= 0)
                && newPitch       == mPM.getAudioLibraryPitch(li)
                && bpmClamped     == mPM.getAudioLibraryBPM(li)
                && stretch        == mPM.getAudioLibraryStretchMode(li)
                && cur.routeChannel == mPM.getAudioLibraryPageOwner(li);
            const bool desiredOverride = ! matchesMaster;
            const bool propsChanged = (newPitch   != cur.pitchSemitones
                                       || bpmClamped != cur.originalBPM
                                       || stretch  != cur.stretchMode);
            if (propsChanged || desiredOverride != cur.isOverride)
            {
                beginEdit("Audio Clip Properties");
                auto& b2 = mPM.getBlock(blockIdx);
                b2.pitchSemitones = newPitch;
                b2.originalBPM    = bpmClamped;
                b2.stretchMode    = stretch;
                b2.isOverride     = desiredOverride;
                commitEdit();            // fires onArrangementChanged → rebuildAudioClipPlayers
                repaint();
            }
        }), true);
}

void ArrangementGrid::importAudioFile(const juce::String& path, int targetRow, float targetBar, int routeChannel)
{
    ClipDropDiag::log ("importAudioFile ENTER", "path=" + path + " row=" + juce::String (targetRow)
                        + " bar=" + juce::String (targetBar) + " routeChannel=" + juce::String (routeChannel));
    File f(path);
    // QA-E Task 4 (2026-05-12): if the path is project-relative (e.g.
    // "Samples/foo.wav" from the audio library), File(path) resolves it
    // against CWD = the EXE's folder, not the project folder.  Fall through
    // to onResolveStoredPath (VibeSynthProcessor::resolveProjectFile) when
    // the as-is path doesn't exist, so library-relative paths from the
    // browser drag work the same as absolute paths from external drops.
    if (!f.existsAsFile() && onResolveStoredPath)
    {
        const File resolved = onResolveStoredPath (path);
        if (resolved.existsAsFile())
            f = resolved;
    }
    if (!f.existsAsFile()) { ClipDropDiag::log ("importAudioFile BAIL: file not found", "path=" + path + " (drop produced no clip/page/entry)"); return; }

    // Read file metadata to get actual duration.
    // Use a local AudioFormatManager - message thread, so file I/O is fine.
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));

    // QA-Ec (G, 2026-07-08): the clip lands at its TRUE wall-clock length at
    // the tempo in effect at the target bar (base + ruler tempo flags), and
    // plays 1:1 there - originalBPM = that tempo makes the render ratio
    // exactly 1 at import.  The old hardcoded 120 sized blocks wrong at any
    // other project tempo AND silently engaged the stretcher on every import.
    // QA-E Task 5 (2026-05-15): lengthBeats stays the precise float beat
    // count so playback / loop end match the source file's exact duration.
    auto tempoAtBar = [this] (float bar) -> double
    {
        double t = mPM.getGlobalTempo();
        for (int i = 0; i < mPM.getNumTempoChanges(); ++i)
        {
            const auto& tc = mPM.getTempoChange (i);
            if ((float) tc.bar <= bar) t = tc.bpm;
            else break;
        }
        return t;
    };
    int lengthBars = 4;  // fallback
    float originalBPM = (float) tempoAtBar (snapBar (targetBar));
    float fileBeats = -1.f;
    if (reader && reader->sampleRate > 0 && reader->lengthInSamples > 0)
    {
        // G1 smoke ruling (Jeff, supersedes the A/A pick): the DROP-TIME tempo
        // is the identity for EVERY clip regardless of length - it lands at
        // its actual wall-clock time and plays 1:1.  Content-tempo detection
        // is DISPLAY-ONLY (the per-clip Properties readout); letting it drive
        // identity broke portability (tracks recorded in one project and
        // dropped into another at a different BPM would silently re-stretch).
        const double durationSecs = (double) reader->lengthInSamples / reader->sampleRate;
        const double beats = durationSecs * (originalBPM / 60.0);
        lengthBars = juce::jmax (1, (int) std::ceil (beats / 4.0));
        fileBeats  = (float) beats;
    }

    // P4: copy-on-drop.  If a callback is wired, route the external file
    // through ProjectManager::importSample (copies into Samples/, returns a
    // relative string to store).  Empty return = caller rejected (e.g. no
    // project open); in that case, forward to onDropWithoutProject so caller
    // can prompt the user to create a project and retry.  No callback set at
    // all = store absolute path (legacy fallback).
    juce::String storedPath = path;
    if (onImportSampleRequest)
    {
        storedPath = onImportSampleRequest (f);
        if (storedPath.isEmpty())
        {
            ClipDropDiag::log ("importAudioFile BAIL: copy-on-drop returned empty",
                                "path=" + path + " (no project = expected, New-Project prompt follows; a real copy-fail WITH a project is alerted in importSample)");
            if (onDropWithoutProject)
                onDropWithoutProject (f, targetRow, targetBar);
            return;
        }
    }

    beginEdit("Import Audio");
    // QA-UndoCoverage Task 4: the library entry rides the Import Audio
    // transaction (an AudioLibraryAction appended after commitEdit below) so
    // undoing the import no longer strands the entry in the browser.
    AudioLibrarySnapshot libBefore;
    libBefore.entries      = mPM.getAudioLibraryEntries();
    libBefore.manualGroups = mPM.getManualAudioGroupsRaw();
    // Register the stored path in the persistent audio library so the Browser
    // keeps showing it even if every block referencing it gets deleted.
    // QA-E Task 4 (2026-05-12): tag ownerChannelId so re-drag from browser
    // continues to find the entry under the right category.
    const int libCountBefore = mPM.getNumAudioLibrary();
    mPM.addAudioToLibrary(storedPath, {}, routeChannel);
    // QA-Ec (G): a NEW library entry inherits the import tempo as its
    // source-of-truth BPM so browser re-drags land 1:1 too (Properties can
    // override later).  Existing entries are left alone - re-importing a
    // file must not clobber a user-set BPM.
    if (mPM.getNumAudioLibrary() > libCountBefore)
    {
        const int li = mPM.getNumAudioLibrary() - 1;
        mPM.setAudioLibraryClipDefaults (li, mPM.getAudioLibraryPitch (li),
                                         originalBPM, mPM.getAudioLibraryStretchMode (li));
    }
    ClipDropDiag::log ("importAudioFile library-add", "storedPath=" + storedPath + " routeChannel=" + juce::String (routeChannel) + " libCount=" + juce::String (mPM.getNumAudioLibrary()));
    ArrangementBlock b;
    b.clipType       = ClipType::Audio;
    b.trackRow       = jlimit(0, kNumRows - 1, targetRow);
    b.startBeats     = (double)(int) snapBar(targetBar) * 4.0;   // 8A: bar-truncated placement preserved (Task 4 #27 un-truncates moves)
    b.lengthBars     = lengthBars;
    b.setLengthBeats (fileBeats);      // QA-E Task 5 (2026-05-15): exact end so
                                       // playback / loop match the source file
    b.audioFilePath  = storedPath;
    b.originalBPM    = originalBPM;
    b.stretchMode    = true;
    b.routeChannel   = routeChannel;   // QA-E Task 4 (2026-05-12)
    mPM.addBlock(b);
    mSelection.clear();
    mSelection.push_back(mPM.getNumBlocks() - 1);
    commitEdit();

    if (mUndoCtx.isValid())
    {
        AudioLibrarySnapshot libAfter;
        libAfter.entries      = mPM.getAudioLibraryEntries();
        libAfter.manualGroups = mPM.getManualAudioGroupsRaw();
        // Appends into commitEdit's transaction (no beginNewTransaction in
        // between): one Ctrl+Z removes the block AND the library entry.
        mUndoCtx.manager->perform (new AudioLibraryAction (
            std::move (libBefore), std::move (libAfter),
            [sp = juce::Component::SafePointer<ArrangementGrid> (this)]
            (const AudioLibrarySnapshot& s)
            {
                if (sp == nullptr) return;
                sp->mPM.restoreAudioLibrary (s.entries, s.manualGroups);
                if (sp->onArrangementChanged) sp->onArrangementChanged();
            }));
    }

    // Ensure thumbnail is loaded - cache key is the STORED path so later
    // paint calls (which pass b.audioFilePath) hit the same entry.
    getOrCreateThumbnail(storedPath);
    resized(); repaint();

    // QA-E Task 4 (2026-05-12): skip onAudioClipAdded for routed clips
    // (Vox/Inst/Clips-page-routed) -- those play through the originating
    // page's chain, not via an Audio strip.  Mirrors the routeChannel==0
    // guard inside dropWavAsClip at StandaloneEditor.cpp.
    if (routeChannel == 0 && onAudioClipAdded)
    {
        ClipDropDiag::log ("importAudioFile -> onAudioClipAdded", "firing cascade; row=" + juce::String (b.trackRow) + " storedPath=" + storedPath);
        onAudioClipAdded(b.trackRow, mPM.getRowNames()[b.trackRow], storedPath);
    }
    else
        ClipDropDiag::log ("importAudioFile DONE", "routeChannel=" + juce::String (routeChannel) + " (routed clip; no Clips-page spawn) storedPath=" + storedPath);
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-E Task 5 (2026-05-15): place an existing library entry on the grid
// WITHOUT a fresh importSample/library-add/page-spawn cycle.  Used for
// browser->grid drops + the disk-drag "Existing routing" prompt callback.
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::placeAudioLibraryEntry(int libIdx, int targetRow, float targetBar)
{
    if (libIdx < 0 || libIdx >= mPM.getNumAudioLibrary()) return;
    const juce::String path  = mPM.getAudioLibraryPath (libIdx);
    const int          owner = mPM.getAudioLibraryPageOwner (libIdx);
    if (path.isEmpty()) return;

    // Resolve path to absolute (entries may be relative "Samples/foo.wav").
    juce::File f (path);
    if (! f.existsAsFile() && onResolveStoredPath)
    {
        const juce::File resolved = onResolveStoredPath (path);
        if (resolved.existsAsFile()) f = resolved;
    }
    if (! f.existsAsFile())
    {
        // QA-H Task 8 (#19): the silent return here made a re-drop of a
        // moved/deleted file look like a dead click.
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Audio File Not Found",
            "Cannot place this clip - the file is missing:\n\n" + path
            + "\n\nIt may have been moved or deleted. Restore the file, or "
              "remove the entry from the browser (right-click > Remove).");
        return;
    }

    // Read file metadata for exact lengthBeats (mirrors importAudioFile).
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    int   lengthBars  = 4;
    // QA-Ec (G): the library entry's stored BPM is the source of truth
    // (stamped at first import with the import tempo; user-editable via
    // Properties) - the old hardcoded 120 stretched every placement the
    // moment the project tempo was anything else.
    const float originalBPM = mPM.getAudioLibraryBPM (libIdx);
    float fileBeats   = -1.f;
    if (reader && reader->sampleRate > 0 && reader->lengthInSamples > 0)
    {
        const double durationSecs = (double) reader->lengthInSamples / reader->sampleRate;
        const double beats = durationSecs * (originalBPM / 60.0);
        lengthBars = juce::jmax (1, (int) std::ceil (beats / 4.0));
        fileBeats  = (float) beats;
    }

    beginEdit ("Place Audio Clip");
    ArrangementBlock b;
    b.clipType      = ClipType::Audio;
    b.trackRow      = juce::jlimit (0, kNumRows - 1, targetRow);
    b.startBeats    = (double)(int) snapBar (targetBar) * 4.0;
    b.lengthBars    = lengthBars;
    b.setLengthBeats (fileBeats);
    b.audioFilePath = path;
    b.originalBPM   = originalBPM;
    // QA-Ec: inherit the entry's stretch/resample mode too - the library is
    // the source of truth for clip defaults (FILE-02 design); hardcoded
    // `true` ignored a Properties edit on the browser entry.
    b.stretchMode   = mPM.getAudioLibraryStretchMode (libIdx);
    b.routeChannel  = owner;
    mPM.addBlock (b);
    mSelection.clear();
    mSelection.push_back (mPM.getNumBlocks() - 1);
    commitEdit();

    getOrCreateThumbnail (path);
    resized();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Ghost clip helpers (Task 4 - Source Picker drag)
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::setGhostClip(const juce::OptionalScopedPointer<ArrangementBlock>*)
{
    // API stub - ghost block set directly via mHasGhost/mGhostBlock
}

void ArrangementGrid::clearGhostClip()
{
    mHasGhost = false;
    repaint();
}

void ArrangementGrid::placeGhostClip()
{
    if (!mHasGhost) return;
    beginEdit("Place Clip");
    mPM.addBlock(mGhostBlock);
    mSelection.clear();
    mSelection.push_back(mPM.getNumBlocks() - 1);
    commitEdit();
    afterPatternBlockPlaced(mGhostBlock);   // QA-G Task 6
    mHasGhost = false;
    resized(); repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// File Drag and Drop (Task 5 - audio import from OS)
// ─────────────────────────────────────────────────────────────────────────────
bool ArrangementGrid::isInterestedInFileDrag(const StringArray& files)
{
    for (const auto& f : files)
        if (File(f).hasFileExtension("wav;mp3;aiff;flac;ogg;aif")) return true;
    return false;
}

void ArrangementGrid::fileDragMove(const StringArray& files, int x, int y)
{
    if (files.size() == 0) return;
    mFileDragActive = true;
    mFileDragPath   = files[0];
    mFileDragX = x; mFileDragY = y;

    mHasGhost = true;
    mGhostBlock.clipType      = ClipType::Audio;
    mGhostBlock.audioFilePath = files[0];
    mGhostBlock.trackRow      = jlimit(0, kNumRows - 1, yToRow(y));
    mGhostBlock.startBeats    = (double)(int) snapBar(xToBar(x)) * 4.0;
    mGhostBlock.lengthBars    = 4;
    mGhostAlpha = 0.5f;
    repaint();
}

void ArrangementGrid::fileDragExit(const StringArray&)
{
    mFileDragActive = false;
    mHasGhost = false;
    repaint();
}

void ArrangementGrid::filesDropped(const StringArray& files, int x, int y)
{
    ClipDropDiag::log ("filesDropped ENTER", "numFiles=" + juce::String (files.size()) + " (drag-drop path)");
    mFileDragActive = false;
    mHasGhost = false;
    // Multi-file drop: stagger each file onto its OWN row (baseRow, +1, +2, ...)
    // so every clip lives on its own track AND triggers its own mixer strip
    // via importAudioFile -> onAudioClipAdded. All start at the same bar.
    const int baseRow = jlimit(0, kNumRows - 1, yToRow(y));
    const float bar   = xToBar(x);
    int placed = 0;
    for (const auto& fs : files) {
        if (! File(fs).hasFileExtension("wav;mp3;aiff;flac;ogg;aif")) continue;
        const int row = jmin(kNumRows - 1, baseRow + placed);

        // QA-E Task 5 / Task 7 (FILE-02, 2026-05-17): if the dropped file is
        // already in the audio library, defer to StandaloneEditor's
        // duplicate-drop prompt (Use Existing / New Page / Cancel).
        //
        // Match by FILENAME (case-insensitive, Windows), not just resolved-
        // path equality: copy-on-drop relocates imports into the project's
        // Samples/ folder, so the external source path the user drags from
        // is NEVER equal to the library entry's stored Samples/ path -- the
        // old exact-path check therefore never fired the prompt and every
        // re-drop silently re-imported (new page + duplicate same-name
        // entry).  Filename match mirrors importSample's own collision
        // keying; the prompt itself is the disambiguation (Jeff's call (a),
        // 2026-05-17).
        const File droppedF (fs);
        int existingLibIdx = -1;
        for (int i = 0; i < mPM.getNumAudioLibrary(); ++i)
        {
            const String stored = mPM.getAudioLibraryPath (i);
            if (stored.isEmpty()) continue;
            File libFile;
            if (onResolveStoredPath) libFile = onResolveStoredPath (stored);
            else                     libFile = File (stored);
            if (libFile == droppedF
                || libFile.getFileName().equalsIgnoreCase (droppedF.getFileName()))
            { existingLibIdx = i; break; }
        }

        if (existingLibIdx >= 0 && onDuplicateFileDrop)
        {
            ClipDropDiag::log ("filesDropped -> duplicate prompt", "file=" + droppedF.getFileName() + " matches libIdx=" + juce::String (existingLibIdx));
            onDuplicateFileDrop (droppedF, existingLibIdx, row, bar);
        }
        else
        {
            ClipDropDiag::log ("filesDropped -> importAudioFile", "file=" + droppedF.getFileName() + " (not in library)");
            importAudioFile (fs, row, bar);
        }
        ++placed;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DragAndDropTarget - accept BrowserItem drags from BrowserPanel
// Description format: "kind:index"  (kind = pattern / audio / auto)
// ─────────────────────────────────────────────────────────────────────────────
static bool parseBrowserDragDescription(const var& desc, String& outKind, int& outIdx)
{
    if (!desc.isString()) return false;
    const String s = desc.toString();
    const int colon = s.indexOfChar(':');
    if (colon <= 0) return false;
    outKind = s.substring(0, colon);
    outIdx  = s.substring(colon + 1).getIntValue();
    return outKind == "pattern" || outKind == "audio" || outKind == "auto";
}

bool ArrangementGrid::isInterestedInDragSource(const SourceDetails& d)
{
    // TS7 §11.5: a render carries a path, not an index, so it is checked before
    // the index parser -- which cannot represent it.
    if (d.description.toString().startsWith ("render:")) return true;
    String kind; int idx;
    return parseBrowserDragDescription(d.description, kind, idx);
}

void ArrangementGrid::itemDragEnter(const SourceDetails& d)
{
    itemDragMove(d);
}

void ArrangementGrid::itemDragMove(const SourceDetails& d)
{
    String kind; int idx;
    if (!parseBrowserDragDescription(d.description, kind, idx)) return;

    const int x = d.localPosition.getX();
    const int y = d.localPosition.getY();
    if (y < rulerPinY() + kRulerH) { mHasGhost = false; repaint(); return; }

    mGhostBlock = ArrangementBlock();
    mGhostBlock.trackRow   = jlimit(0, kNumRows - 1, yToRow(y));
    mGhostBlock.startBeats = (double) jmax(0, (int)snapBar(xToBar(x))) * 4.0;
    mGhostBlock.lengthBars = 1;

    if (kind == "pattern")
    {
        mGhostBlock.clipType     = ClipType::Pattern;
        mGhostBlock.patternIndex = idx;
        if (idx >= 0 && idx < mPM.getNumPatterns())
        {
            // #25 (QA-G3Smoke): ghost previews the pattern's REAL content
            // length (already bar-ceiled at its own TS by
            // getPatternContentBeats) -- the same size the drop places.
            const double contentBeats = mPM.getPatternContentBeats (idx);
            mGhostBlock.setLengthBeats (contentBeats);
            mGhostBlock.lengthBars = jmax (1, (int) std::ceil (contentBeats / 4.0));
        }
    }
    else if (kind == "audio")
    {
        mGhostBlock.clipType   = ClipType::Audio;
        mGhostBlock.lengthBars = 4;
    }
    else // auto
    {
        mGhostBlock.clipType   = ClipType::Automation;
        mGhostBlock.lengthBars = 4;
    }
    mHasGhost   = true;
    mGhostAlpha = 0.55f;
    repaint();
}

void ArrangementGrid::itemDragExit(const SourceDetails&)
{
    mHasGhost = false;
    repaint();
}

void ArrangementGrid::itemDropped(const SourceDetails& d)
{
    // TS7 §11.5: a render dropped from the Files browser.  Handled before the
    // index parser because it carries a PATH, and routed through the SAME
    // import-then-prompt callback the context menu's "Add to Project..." uses --
    // so dragging and right-clicking cannot end up meaning different things.
    const String desc = d.description.toString();
    if (desc.startsWith ("render:"))
    {
        mHasGhost = false;
        repaint();
        const int y = d.localPosition.getY();
        if (y < rulerPinY() + kRulerH) return;
        const int   row = jlimit(0, kNumRows - 1, yToRow(y));
        const float bar = jmax(0.f, snapBar(xToBar(d.localPosition.getX())));
        if (onRenderFileDropped)
            onRenderFileDropped (desc.fromFirstOccurrenceOf ("render:", false, false),
                                 row, bar);
        return;
    }

    String kind; int idx;
    if (!parseBrowserDragDescription(d.description, kind, idx))
    {
        mHasGhost = false; repaint(); return;
    }

    const int x = d.localPosition.getX();
    const int y = d.localPosition.getY();
    if (y < rulerPinY() + kRulerH) { mHasGhost = false; repaint(); return; }

    const int   row = jlimit(0, kNumRows - 1, yToRow(y));
    const float bar = jmax(0.f, snapBar(xToBar(x)));

    if (kind == "pattern")
    {
        if (idx < 0 || idx >= mPM.getNumPatterns()) { mHasGhost = false; repaint(); return; }
        beginEdit("Place Pattern Clip");
        ArrangementBlock b;
        b.clipType     = ClipType::Pattern;
        b.trackRow     = row;
        b.patternIndex = idx;
        b.startBeats   = (double)(int) bar * 4.0;
        // #25 (QA-G3Smoke): a dropped pattern is sized from its REAL content
        // length (getPatternContentBeats -- note-for-note extent, bar-ceiled
        // at the pattern's own TS), not the stored Pattern.bars.
        {
            const double contentBeats = mPM.getPatternContentBeats (idx);
            b.setLengthBeats (contentBeats);
            b.lengthBars = jmax (1, (int) std::ceil (contentBeats / 4.0));
        }
        mPM.addBlock(b);
        mSelection.clear();
        mSelection.push_back(mPM.getNumBlocks() - 1);
        commitEdit();
        afterPatternBlockPlaced(b);
    }
    else if (kind == "audio")
    {
        // QA-E Task 5 (2026-05-15): browser->grid is "place existing library
        // entry" -- skip importSample / addAudioToLibrary / onAudioClipAdded.
        // The file is by definition already in the library; calling
        // importAudioFile here was the source of the spurious duplicate
        // library entries (importSample's filename-collision fallback would
        // copy the file to "<name> (2).wav" when juce::File equality failed
        // for stored-vs-resolved path mismatches on Windows).
        placeAudioLibraryEntry(idx, row, bar);
    }
    else // auto
    {
        // Spawn an Automation block from the persistent template library.
        if (idx >= 0 && idx < mPM.getNumAutomationTemplates())
        {
            beginEdit("Place Automation Clip");
            ArrangementBlock b;
            b.clipType       = ClipType::Automation;
            b.trackRow       = row;
            b.startBeats     = (double)(int) bar * 4.0;
            b.lengthBars     = 4;
            b.automationLane = mPM.getAutomationTemplate(idx);
            mPM.addBlock(b);
            mSelection.clear();
            mSelection.push_back(mPM.getNumBlocks() - 1);
            commitEdit();
        }
    }

    mHasGhost = false;
    if (onArrangementChanged) onArrangementChanged();
    resized(); repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Cursor
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::updateCursor()
{
    switch (mActiveTool)
    {
        case AGTool::Draw:      setMouseCursor(MouseCursor::CrosshairCursor);       break;
        case AGTool::Paint:     setMouseCursor(MouseCursor::CrosshairCursor);       break;
        case AGTool::Select:    setMouseCursor(MouseCursor::NormalCursor);          break;
        case AGTool::Delete:    setMouseCursor(MouseCursor::CrosshairCursor);       break;
        case AGTool::Mute:      setMouseCursor(MouseCursor::NormalCursor);          break;
        // QA-Ea Task 0c (2026-05-20): AGTool::SlipEdit removed (now the
        // EditMode dropdown).  The hover-cursor for slip mode is set in
        // mouseMove when EditMode == Slip + cursor over an Audio clip edge.
        case AGTool::Slice:     setMouseCursor(MouseCursor::CrosshairCursor);       break;
        case AGTool::Zoom:      setMouseCursor(MouseCursor::CrosshairCursor);       break;
        case AGTool::PlaySelected: setMouseCursor(MouseCursor::NormalCursor);       break;
    }
}

void ArrangementGrid::setTool(AGTool t)
{
    mActiveTool = t;
    updateCursor();
    if (onToolChanged) onToolChanged(t);
}

// ─────────────────────────────────────────────────────────────────────────────
// Modifier key tracking (Alt = snap override)
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::modifierKeysChanged(const juce::ModifierKeys& mods)
{
    mAltSnapActive = mods.isAltDown();
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse
// ─────────────────────────────────────────────────────────────────────────────
void ArrangementGrid::mouseMove(const MouseEvent& e)
{
    int hit = blockAtPos(e.x, e.y);
    // QA-Ea Task 0c (FL pre-roll record): when EditMode is Slip, an Audio
    // clip's LEFT or RIGHT edge gets the horizontal-resize cursor hint so
    // beginners know either edge is draggable.
    if (hit >= 0 && mEditMode == EditMode::Slip
        && mPM.getBlock (hit).clipType == ClipType::Audio
        && (nearLeftEdge (hit, e.x) || nearRightEdge (hit, e.x)))
    {
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
        return;
    }
    if (hit >= 0 && (mActiveTool == AGTool::Draw || mActiveTool == AGTool::Select)
        && nearRightEdge(hit, e.x))
        setMouseCursor(MouseCursor::LeftRightResizeCursor);
    else
        updateCursor();
}

// #26 (QA-G3Smoke, G-15): clicking a block makes it the "brush" -- identity +
// length + content offset, nothing else.  Automation blocks carry no template
// identity, so they never prime; an audio block not (or no longer) in the
// library leaves the brush untouched.
void ArrangementGrid::primeClickMemoryFrom (int blockIdx)
{
    if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return;
    const auto& b = mPM.getBlock (blockIdx);
    if (b.clipType == ClipType::Pattern)
    {
        mDropKind               = BrowserDropKind::Pattern;
        mBrowserSelection       = b.patternIndex;
        mClickMemoryLenBeats    = effectiveLengthBeats (b);
        mClickMemoryOffsetTicks = b.contentOffsetTicks;
    }
    else if (b.clipType == ClipType::Audio)
    {
        const int libIdx = mPM.findAudioLibraryIndexByPath (b.audioFilePath);
        if (libIdx < 0) return;
        mDropKind                       = BrowserDropKind::Audio;
        mDropAudioIdx                   = libIdx;
        mClickMemoryLenBeats            = effectiveLengthBeats (b);
        mClickMemoryContentStartSamples = b.contentStartSamples;
    }
}

void ArrangementGrid::mouseDown(const MouseEvent& e)
{
#if JUCE_DEBUG
    G3PlayheadDiag::log ("click(builder) x=" + juce::String (e.x) + " y=" + juce::String (e.y)
                         + " rawBar=" + juce::String (xToBar (e.x), 4)
                         + " snapBar=" + juce::String (snapBar (xToBar (e.x)), 4)
                         + " playheadBar=" + juce::String (mPlayheadBar, 4));
#endif
    grabKeyboardFocus();

    // ── 2026-04-26 (D-7 sub-4): click-outside-time-range clears state ────
    if (mTimeSelStart >= 0.f && mTimeSelEnd > mTimeSelStart
        && ! e.mods.isRightButtonDown())
    {
        const float t0 = std::min (mTimeSelStart, mTimeSelEnd);
        const float t1 = std::max (mTimeSelStart, mTimeSelEnd);
        const float clickBar = xToBar (e.x);
        if (clickBar < t0 || clickBar >= t1)
        {
            mTimeSelStart    = mTimeSelEnd = -1.f;
            mTimeSelDragging = false;
            mSelection.clear();
        }
    }

    // Ruler zone: Ctrl+drag = time selection; bare click = seek playhead;
    // right-click = ruler context menu (D-2: time markers + TS changes).
    if (e.y < rulerPinY() + kRulerH)
    {
        if (e.mods.isRightButtonDown())
        {
            showRulerContextMenu (e.x);
            return;
        }
        if (e.mods.isCtrlDown())
        {
            float bar = snapBar(xToBar(e.x));
            mTimeSelAnchor   = bar;
            mTimeSelStart    = bar;
            mTimeSelEnd      = bar;
            mTimeSelDragging = true;
            repaint();
        }
        else
        {
            // Bare left-click on ruler: seek playhead to clicked position.
            // 1A (QA-G3Smoke, Jeff 2026-07-23): seek obeys snap; Alt = free.
            double beat = static_cast<double>(e.mods.isAltDown()
                              ? xToBar(e.x) : snapBar(xToBar(e.x))) * 4.0;
            if (onSeek) onSeek(beat);
        }
        return;
    }

    // 2026-04-26 (B-4): Ctrl+drag from empty grid area = marquee selection
    // regardless of active tool.  If the click lands on a block, fall through
    // to tool-specific handling so existing Ctrl+click semantics on a block
    // are preserved.
    if (e.mods.isCtrlDown() && !e.mods.isRightButtonDown()
        && blockAtPos(e.x, e.y) < 0)
    {
        if (!e.mods.isShiftDown()) mSelection.clear();
        mMarqueeActive = true;
        mMarqueeStart  = e.getPosition();
        mMarqueeRect   = {};
        repaint();
        return;
    }

    // Right-click: context menu - also handles automation point right-click menu
    if (e.mods.isRightButtonDown())
    {
        // 2026-04-26 (D-1): modifier-aware right-click - these branches fire
        // BEFORE the context menu so they take priority over the default flow.
        if (e.mods.isCtrlDown() && e.mods.isShiftDown())
        {
            // Ctrl+Shift+RClick = fit selected block to viewport.
            int hit = blockAtPos(e.x, e.y);
            if (hit >= 0) fitBlockToViewport(hit);
            return;
        }
        if (e.mods.isCtrlDown())
        {
            // Ctrl+RClick + drag = drag-rect zoom-fit.  No-op on bare click
            // (rect ends up empty in mouseUp).
            mZoomRectActive = true;
            mZoomRectStart  = e.getPosition();
            mZoomRect       = {};
            repaint();
            return;
        }
        if (e.mods.isAltDown())
        {
            // Alt+RClick = quantize popup for selected blocks.
            showQuantizePopup();
            return;
        }

        int hit = blockAtPos(e.x, e.y);
        if (hit >= 0)
        {
            // Draw tool on Automation clip: right-click near a point → Reset/Delete menu
            if (mActiveTool == AGTool::Draw
                && mPM.getBlock(hit).clipType == ClipType::Automation)
            {
                int ptIdx = hitTestAutomPoint(hit, e.x, e.y);
                if (ptIdx >= 0)
                {
                    juce::PopupMenu m;
                    m.addItem(1, "Reset to midpoint");
                    m.addSeparator();
                    m.addItem(2, "Delete");
                    m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),
                        [this, hit, ptIdx](int result)
                        {
                            if (hit >= mPM.getNumBlocks()) return;
                            auto& lane = mPM.getBlock(hit).automationLane;
                            if (ptIdx >= (int)lane.points.size()) return;
                            if (result == 1)
                            {
                                beginEdit("Reset to Midpoint");
                                lane.points[ptIdx].value01 = 0.5f;
                                commitEdit();
                                repaint();
                            }
                            else if (result == 2)
                            {
                                // QA-ProjectSave (2026-07-26, Jeff): the last
                                // point going means the automation controls
                                // nothing, so offer to remove the whole block
                                // rather than leave an empty one on the grid.
                                if (lane.points.size() <= 1)
                                {
                                    promptDeleteWholeAutomation (hit);
                                    return;
                                }
                                beginEdit("Delete Automation Point");
                                lane.points.erase(lane.points.begin() + ptIdx);
                                commitEdit();
                                repaint();
                            }
                        });
                    return;
                }
            }
            showClipContextMenu(hit);
        } else if (mActiveTool == AGTool::Delete) {
            // nothing - right-click on empty with Delete tool is fine
        }
        return;
    }

    // 2026-04-26 (D-1) + smoke #45: RIGHT-Alt+LClick on a block = toggle mute,
    // regardless of active tool.  LEFT Alt is the fine-move (no-snap) drag
    // modifier, so it must fall through to the normal move path instead of
    // eating the mouseDown here.  Still falls through when the click misses
    // (so Zoom tool's Alt+click = zoom-out works on empty area).
    if (e.mods.isAltDown() && ! e.mods.isCtrlDown() && ! e.mods.isShiftDown()
        && isRightAltKeyDown())
    {
        int hit = blockAtPos(e.x, e.y);
        if (hit >= 0)
        {
            beginEdit("Mute Toggle");
            mPM.getBlock(hit).muted = ! mPM.getBlock(hit).muted;
            commitEdit();
            repaint();
            return;
        }
    }

    // QA-Ea Task 0c (FL pre-roll record): slip-mode edge drag.  Fires when
    // mEditMode == Slip AND the user clicks an Audio clip's LEFT or RIGHT
    // edge -- regardless of the active tool (Draw / Select / etc.).
    // Mirrors the mouseMove cursor-hint logic above (also tool-agnostic).
    // Must come BEFORE the tool-specific branches so the slip-mode wins.
    //
    // LEFT edge slip = Option A (slip-in): left edge moves on timeline,
    // contentStart shifts, lengthBeats grows/shrinks to keep right end fixed.
    //
    // RIGHT edge slip = slip-out: right edge moves on timeline (via
    // lengthBeats), contentStart + startBeats stay fixed.
    //
    // Pattern + Automation clips bypass this entirely (gated on Audio).
    // Stretch mode left-edge = silent no-op; right-edge keeps falling
    // through to the existing length-only resize (QA-Ec will replace).
    if (mEditMode == EditMode::Slip)
    {
        int hit = blockAtPos (e.x, e.y);
        if (hit >= 0
            && mPM.getBlock (hit).clipType == ClipType::Audio
            && (nearLeftEdge (hit, e.x) || nearRightEdge (hit, e.x)))
        {
            beginEdit ("Slip");
            mSlipEditing             = true;
            mSlipEditIdx             = hit;
            mSlipEditEdge            = nearLeftEdge (hit, e.x) ? SlipEdge::Left
                                                                : SlipEdge::Right;
            mSlipEditDragOrigX       = e.x;
            mSlipEditOrigContent     = mPM.getBlock (hit).contentStartSamples;
            // QA-Ea Task 0c (2026-05-20): use effectiveLengthBeats so the
            // origin captures sub-bar precision even when lengthBeats == -1
            // (legacy bar-int-length blocks).
            mSlipEditOrigLengthBeats = (float) effectiveLengthBeats (mPM.getBlock (hit));
            // QA-Ea Task 0c (2026-05-20 - Option A semantic): snapshot the
            // start position in beats (sub-bar precision) so drag delta
            // anchors to the click-time state, NOT the in-progress mutating
            // value.  Without this snapshot, repeated drag fires would
            // accumulate floating-point drift.
            mSlipEditOrigStartBeats  = (float) effectiveStartBeats (mPM.getBlock (hit));
            // QA-Ea Task 0c (Option ii auto-scroll): origin mouse bar at
            // click time.  Drag uses bar-delta from this so auto-scroll
            // works correctly -- as the viewport scrolls, xToBar(e.x)
            // returns a smaller bar value for the same component-x and
            // the edge follows naturally.
            mSlipEditOrigMouseBar    = xToBar (e.x);
            return;
        }
    }

    // Zoom tool: click zooms in, Alt+click zooms out (anchored to cursor)
    if (mActiveTool == AGTool::Zoom)
    {
        float factor = e.mods.isAltDown() ? (1.f / 1.15f) : 1.15f;
        float vpW = 800.f;
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            vpW = (float)jmax(1, vp->getWidth());
        const float minPP = minZoomPPBar (vpW), maxPP = maxZoomPPBar (vpW);
        const float anchorBar = xToBar(e.x);
        mPPBar = jlimit(minPP, maxPP, mPPBar * factor);
        // Restore the bar that was under the cursor to the cursor's x.
        // QA-Ea Task 0c (Option ii): allow negative-bar viewport so slip-
        // edited clips remain visible after the user drags left past bar 0.
        mBarOff = jmax(-(float) maxRevealableNegativeBars(),
                       anchorBar - (float)e.x / jmax(1.f, mPPBar));
        resized(); repaint();
        return;
    }

    // Mute tool: toggle mute on clicked clip
    if (mActiveTool == AGTool::Mute)
    {
        int hit = blockAtPos(e.x, e.y);
        if (hit >= 0) {
            beginEdit("Mute Toggle");
            mPM.getBlock(hit).muted = !mPM.getBlock(hit).muted;
            commitEdit();
            repaint();
        }
        return;
    }

    // B-4: Slice tool - begin a drag-line (snap the anchor X to the active grid).
    // The cut is applied on mouseUp so the user can drag + preview the line first;
    // it then splits every block the line's y-span crosses (see applySliceLine).
    if (mActiveTool == AGTool::Slice)
    {
        mSlicing    = true;
        const int snappedX = barToX (snapBar (xToBar (e.x)));
        mSliceStart = { snappedX, e.y };
        mSliceEnd   = mSliceStart;
        repaint();
        return;
    }

    // Delete tool
    if (mActiveTool == AGTool::Delete)
    {
        int hit = blockAtPos(e.x, e.y);
        if (hit >= 0) {
            beginEdit("Delete");
            mPM.removeBlock(hit);
            commitEdit();
            mSelection.erase(std::remove(mSelection.begin(), mSelection.end(), hit), mSelection.end());
            for (auto& idx : mSelection) if (idx > hit) --idx;
            resized(); repaint();
        }
        return;
    }

    // Draw tool
    if (mActiveTool == AGTool::Draw)
    {
        int hit = blockAtPos(e.x, e.y);
        if (hit >= 0)
        {
            primeClickMemoryFrom (hit);   // #26: this block becomes the brush
            // ── Automation clip: point editing ───────────────────────────────
            if (mPM.getBlock(hit).clipType == ClipType::Automation)
            {
                mAutomEditBlock = hit;
                int bx = barToX((float) effectiveStartBars(mPM.getBlock(hit)));
                int bw = jmax(4, barToX((float)(effectiveStartBars(mPM.getBlock(hit))
                                                + mPM.getBlock(hit).lengthBars)) - bx - 1);
                int by = rowToY(mPM.getBlock(hit).trackRow) + 2;
                int bh = rowHeightPx(mPM.getBlock(hit).trackRow) - 4;

                // ── Right-click on point → Reset / Delete menu ──────────────
                if (e.mods.isRightButtonDown())
                {
                    int ptIdx = hitTestAutomPoint(hit, e.x, e.y);
                    if (ptIdx >= 0)
                    {
                        juce::PopupMenu m;
                        m.addItem(1, "Reset to midpoint");
                        m.addSeparator();
                        m.addItem(2, "Delete");
                        int blockSnap = hit;
                        int ptSnap    = ptIdx;
                        m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),
                            [this, blockSnap, ptSnap](int result)
                            {
                                if (blockSnap < 0 || blockSnap >= mPM.getNumBlocks()) return;
                                auto& lane = mPM.getBlock(blockSnap).automationLane;
                                if (ptSnap < 0 || ptSnap >= (int)lane.points.size()) return;
                                beginEdit(result == 1 ? "Reset to Midpoint" : "Delete Point");
                                if (result == 1)
                                    lane.points[ptSnap].value01 = 0.5f;
                                else if (result == 2)
                                    lane.points.erase(lane.points.begin() + ptSnap);
                                commitEdit();
                                repaint();
                            });
                    }
                    return;
                }

                // ── Curve handle drag ────────────────────────────────────────
                int chIdx = hitTestAutomCurveHandleInBlock(
                                mPM.getBlock(hit).automationLane,
                                bx, bw, by, bh, e.x, e.y);
                if (chIdx >= 0)
                {
                    mAutomCurveHandleDrag = chIdx;
                    mAutomEditBlock       = hit;
                    mAutomCurveHandleOrigTension =
                        mPM.getBlock(hit).automationLane.points[chIdx].tension;
                    beginEdit("Adjust Curve");
                    return;
                }

                int ptIdx = hitTestAutomPoint(hit, e.x, e.y);
                if (ptIdx >= 0)
                {
                    // Start dragging existing point
                    mAutomDragPoint  = ptIdx;
                    mAutomDragging   = true;
                    mAutomDragOrig   = mPM.getBlock(hit).automationLane.points[ptIdx];
                    beginEdit("Move Automation Point");
                }
                else if (!nearRightEdge(hit, e.x))
                {
                    // Place new control point
                    float relX = (float)(e.x - bx) / (float)bw;
                    float relY = 1.f - (float)(e.y - by) / (float)bh;
                    relX = jlimit(0.f, 1.f, relX);
                    relY = jlimit(0.f, 1.f, relY);

                    beginEdit("Add Automation Point");
                    ControlPoint pt;
                    pt.timeTicks = relX;
                    pt.value01   = relY;
                    auto& lane = mPM.getBlock(hit).automationLane;
                    lane.points.push_back(pt);
                    std::sort(lane.points.begin(), lane.points.end(),
                        [](const ControlPoint& a, const ControlPoint& c)
                            { return a.timeTicks < c.timeTicks; });
                    commitEdit();
                    repaint();
                }
                else
                {
                    // Near right edge - resize the clip
                    beginEdit("Resize");
                    mResizing        = true;
                    mResizeIdx       = hit;
                    mResizeOrigLen   = (float)mPM.getBlock(hit).lengthBars;
                    mResizeOrigStart = (float) effectiveStartBars (mPM.getBlock(hit));
                    mAutomEditBlock  = -1;
                    // Snapshot automation points so we can rescale absolute positions
                    if (mPM.getBlock(hit).clipType == ClipType::Automation)
                        mResizeOrigPoints = mPM.getBlock(hit).automationLane.points;
                    else
                        mResizeOrigPoints.clear();
                }
                return;
            }

            if (nearRightEdge(hit, e.x))
            {
                // Resize or time-stretch for audio clips.  G1 boundary fix
                // (Jeff's smoke): the toolbar's Slip/Stretch EDIT MODE is the
                // primary stretch gesture - in Stretch mode a plain edge drag
                // re-fits (that was always the dropdown's stated purpose,
                // "QA-Ec ships it"); Shift+drag stretches from ANY mode.
                beginEdit("Resize");
                mResizing        = true;
                mStretching      = mPM.getBlock(hit).clipType == ClipType::Audio
                                   && (e.mods.isShiftDown() || mEditMode == EditMode::Stretch);
                mResizeIdx       = hit;
                mResizeOrigLen   = (float)mPM.getBlock(hit).lengthBars;
                mStretchOrigBeats = effectiveLengthBeats (mPM.getBlock(hit));   // QA-Ec: exact re-fit base
                mResizeOrigStart = (float) effectiveStartBars (mPM.getBlock(hit));
                if (mPM.getBlock(hit).clipType == ClipType::Automation)
                    mResizeOrigPoints = mPM.getBlock(hit).automationLane.points;
                else
                    mResizeOrigPoints.clear();
            }
            else
            {
                if (!isSelected(hit)) { mSelection.clear(); mSelection.push_back(hit); }
                mMoving         = true;
                mMoveDragOrigin = e.getPosition();
                mMoveIndices.clear(); mMoveOrigBars.clear(); mMoveOrigRows.clear();
                for (int idx : mSelection) {
                    if (idx >= mPM.getNumBlocks()) continue;
                    mMoveIndices.push_back(idx);
                    mMoveOrigBars.push_back((float) effectiveStartBars (mPM.getBlock(idx)));   // #27: fractional origin
                    mMoveOrigRows.push_back(mPM.getBlock(idx).trackRow);
                }
                // QA-G Task 6: capture follower signatures for the
                // move-across-TS prompt (compared at the move commit).
                mMovePreTs.clear();
                for (int mvIdx : mMoveIndices)
                {
                    const auto& mb = mPM.getBlock(mvIdx);
                    if (mb.clipType != ClipType::Pattern) continue;
                    if (mb.patternIndex < 0 || mb.patternIndex >= mPM.getNumPatterns()) continue;
                    const auto& mp = mPM.getPattern(mb.patternIndex);
                    if (mp.tsLocked || ! mPM.patternHasNotes(mb.patternIndex)) continue;
                    bool seen = false;
                    for (auto& pre : mMovePreTs)
                        if (pre[0] == mb.patternIndex) { seen = true; break; }
                    if (! seen)
                        mMovePreTs.push_back({ mb.patternIndex, mp.tsNum, mp.tsDen });
                }
                beginEdit("Move");
            }
        }
        else
        {
            float bar = xToBar(e.x);
            int   row = yToRow(e.y);
            if (row < 0 || row >= kNumRows) return;
            mDrawing   = true;
            mDrawRow   = row;
            mDrawStart = snapBar(bar);
            mDrawEnd   = mDrawStart + 1.f;
            mSelection.clear();
            repaint();
        }
        return;
    }

    // Paint tool (continuous draw)
    if (mActiveTool == AGTool::Paint)
    {
        int   row = yToRow(e.y);
        float bar = snapBar(xToBar(e.x));
        if (row < 0 || row >= kNumRows) return;
        mPainting    = true;
        mPaintRow    = row;
        mPaintLastBar = bar;
        mSelection.clear();
        // Place first block immediately
        beginEdit("Paint");
        ArrangementBlock b;
        b.trackRow     = row;
        b.patternIndex = mBrowserSelection;
        b.startBeats   = (double)(int) bar * 4.0;
        // #25 (QA-G3Smoke): paint stamps at the pattern's REAL content length
        // (was a fixed 1 bar); the drag continuation advances by the same
        // stamp size so stamps butt-join without overlap.
        {
            const double contentBeats = (mBrowserSelection >= 0
                                         && mBrowserSelection < mPM.getNumPatterns())
                ? mPM.getPatternContentBeats (mBrowserSelection) : 4.0;
            b.setLengthBeats (contentBeats);
            b.lengthBars = jmax (1, (int) std::ceil (contentBeats / 4.0));
            mPaintLenBars = b.lengthBars;
        }
        b.layerTrack   = true;
        mPM.addBlock(b);
        afterPatternBlockPlaced(b);   // QA-G Task 6
        // (commitEdit on mouseUp)
        repaint();
        return;
    }

    // Select tool
    if (mActiveTool == AGTool::Select)
    {
        int hit = blockAtPos(e.x, e.y);
        if (hit >= 0)
        {
            primeClickMemoryFrom (hit);   // #26: this block becomes the brush
            // QA-Ea Task 0c (2026-05-20): EditMode == Slip on Audio clip
            // edges is handled BEFORE every tool-specific branch (see
            // hoisted block above).  Legacy AGTool::SlipEdit + the
            // mSlipping/mSlipIdx stub state were removed; the EditMode
            // dropdown replaces them.

            if (e.mods.isShiftDown()) {
                auto it = std::find(mSelection.begin(), mSelection.end(), hit);
                if (it != mSelection.end()) mSelection.erase(it);
                else mSelection.push_back(hit);
            } else {
                if (!isSelected(hit)) { mSelection.clear(); mSelection.push_back(hit); }
                mMoving         = true;
                mMoveDragOrigin = e.getPosition();
                mMoveIndices.clear(); mMoveOrigBars.clear(); mMoveOrigRows.clear();
                for (int idx : mSelection) {
                    if (idx >= mPM.getNumBlocks()) continue;
                    mMoveIndices.push_back(idx);
                    mMoveOrigBars.push_back((float) effectiveStartBars (mPM.getBlock(idx)));   // #27: fractional origin
                    mMoveOrigRows.push_back(mPM.getBlock(idx).trackRow);
                }
                // QA-G Task 6: capture follower signatures for the
                // move-across-TS prompt (compared at the move commit).
                mMovePreTs.clear();
                for (int mvIdx : mMoveIndices)
                {
                    const auto& mb = mPM.getBlock(mvIdx);
                    if (mb.clipType != ClipType::Pattern) continue;
                    if (mb.patternIndex < 0 || mb.patternIndex >= mPM.getNumPatterns()) continue;
                    const auto& mp = mPM.getPattern(mb.patternIndex);
                    if (mp.tsLocked || ! mPM.patternHasNotes(mb.patternIndex)) continue;
                    bool seen = false;
                    for (auto& pre : mMovePreTs)
                        if (pre[0] == mb.patternIndex) { seen = true; break; }
                    if (! seen)
                        mMovePreTs.push_back({ mb.patternIndex, mp.tsNum, mp.tsDen });
                }
                beginEdit("Move");
            }
        }
        else
        {
            if (!e.mods.isShiftDown()) mSelection.clear();
            mMarqueeActive = true;
            mMarqueeStart  = e.getPosition();
            mMarqueeRect   = {};
        }
        repaint();
    }
}

// B-4: split ONE block at cutBars (in bars).  Caller wraps begin/commitEdit.  The
// right piece keeps a content-offset continuation per clip type so it plays what it
// would have played uncut (FL-style, no copy); the straddling note fires via the
// B-3 clamp-and-play at read time.  Returns true if the block was split.
bool ArrangementGrid::sliceOneBlock (int hit, double cutBars)
{
    if (hit < 0 || hit >= mPM.getNumBlocks()) return false;
    auto& orig = mPM.getBlock (hit);
    const double startBars = effectiveStartBars (orig);
    const double endBars   = startBars + effectiveLengthBars (orig);
    constexpr double kMinPieceBars = 1.0 / 384.0;   // 1 tick
    if (! (cutBars > startBars + kMinPieceBars && cutBars < endBars - kMinPieceBars))
        return false;

    const double cutBeats = (cutBars - startBars) * 4.0;
    ArrangementBlock right = orig;
    right.setStartBeats  (cutBars * 4.0);
    right.setLengthBeats ((endBars - cutBars) * 4.0);
    right.lengthBars = jmax (1, (int) std::round (endBars - cutBars));
    switch (orig.clipType)
    {
        case ClipType::Pattern:
        {
            // #24 (QA-G3Smoke): no tiling, no cycle wrap -- the right piece's
            // offset is simply the cut position into the (single-pass) content;
            // an offset past the content plays/draws blank, like everywhere else.
            right.contentOffsetTicks = orig.contentOffsetTicks + beatsToTicks (cutBeats);
            break;
        }
        case ClipType::Audio:
        {
            // Continuation = content start advances by the file samples the cut span
            // consumes (fileRate * 60 / originalBPM per project beat).
            const juce::File f = onResolveStoredPath
                ? onResolveStoredPath (orig.audioFilePath)
                : juce::File (orig.audioFilePath);
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                    mAFM.createReaderFor (f)))
            {
                const double fileBPM = (orig.originalBPM > 0.f)
                    ? (double) orig.originalBPM : 120.0;
                right.contentStartSamples = orig.contentStartSamples
                    + (juce::int64) std::llround (
                          cutBeats * reader->sampleRate * 60.0 / fileBPM);
            }
            break;
        }
        case ClipType::Automation:
        {
            splitAutomationLane (orig.automationLane, right.automationLane,
                (float) ((cutBars - startBars) / jmax (1.0e-9, endBars - startBars)));
            break;
        }
    }
    orig.setLengthBeats (cutBeats);
    orig.lengthBars = jmax (1, (int) std::round (cutBars - startBars));
    mPM.addBlock (right);
    return true;
}

// B-4/B-5: apply the slice drag-line - split every block whose row the line's
// y-span crosses, at the line's interpolated X for that row (Shift already made it
// vertical during the drag).  Collect first, then split, so the appends never
// invalidate a pending index.
void ArrangementGrid::applySliceLine (juce::Point<int> start, juce::Point<int> end)
{
    const int loY = jmin (start.y, end.y);
    const int hiY = jmax (start.y, end.y);
    constexpr double kMinPieceBars = 1.0 / 384.0;

    struct SliceReq { int idx; double cutBars; };
    std::vector<SliceReq> reqs;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b    = mPM.getBlock (i);
        const int rowTop = rowToY (b.trackRow);
        const int rowBot = rowTop + rowHeightPx (b.trackRow);
        // Row-EXTENT overlap (not just center) so a plain click still slices the
        // block under the cursor and a grazing drag catches tall block rows.
        if (hiY < rowTop || loY > rowBot) continue;   // line's y-span misses this row
        const int rowCy  = (rowTop + rowBot) / 2;

        int cutX;
        if (end.y == start.y)
            cutX = start.x;                            // click / horizontal: cut at the anchor X
        else
        {
            float t = (float)(rowCy - start.y) / (float)(end.y - start.y);
            t = juce::jlimit (0.f, 1.f, t);            // clamp to the drawn segment's extent
            cutX = (int)(start.x + t * (float)(end.x - start.x));
        }

        const double startBars = effectiveStartBars (b);
        const double endBars   = startBars + effectiveLengthBars (b);
        // B-4(ii): prefer the snapped cut; if it lands outside the block (e.g. a
        // short block with no interior grid line) fall back to the raw cursor X so
        // the block is still sliceable.
        const double snapped = (double) snapBar (xToBar (cutX));
        const double raw     = (double)          xToBar (cutX);
        const double cut = (snapped > startBars + kMinPieceBars
                            && snapped < endBars - kMinPieceBars) ? snapped : raw;
        if (cut > startBars + kMinPieceBars && cut < endBars - kMinPieceBars)
            reqs.push_back ({ i, cut });
    }
    if (reqs.empty()) return;

    beginEdit ("Slice");
    for (const auto& r : reqs) sliceOneBlock (r.idx, r.cutBars);
    commitEdit();
    resized(); repaint();
}

void ArrangementGrid::mouseDrag(const MouseEvent& e)
{
    // 2026-04-26 (D-1): Ctrl+RClick zoom-rect drag.
    if (mZoomRectActive)
    {
        mZoomRect = Rectangle<int>::leftTopRightBottom(
            jmin(mZoomRectStart.x, e.x), jmin(mZoomRectStart.y, e.y),
            jmax(mZoomRectStart.x, e.x), jmax(mZoomRectStart.y, e.y));
        repaint();
        return;
    }

    // ── B-4/B-5: Slice drag-line ──────────────────────────────────────────────
    if (mSlicing)
    {
        const int snappedX = barToX (snapBar (xToBar (e.x)));
        if (e.mods.isShiftDown())
        {
            // B-5: Shift forces a VERTICAL cut at the snapped X under the cursor
            // (both endpoints share that X); the y extent is the raw drag.
            mSliceStart.x = snappedX;
            mSliceEnd     = { snappedX, e.y };
        }
        else
            mSliceEnd = { snappedX, e.y };
        repaint();
        return;
    }

    // ── Ruler time selection drag ─────────────────────────────────────────────
    if (mTimeSelDragging)
    {
        float bar = snapBar(xToBar(e.x));
        mTimeSelStart = jmin(mTimeSelAnchor, bar);
        mTimeSelEnd   = jmax(mTimeSelAnchor, bar);
        repaint();
        return;
    }

    // ── Automation curve handle drag ──────────────────────────────────────────
    if (mAutomCurveHandleDrag >= 0 && mAutomEditBlock >= 0
        && mAutomEditBlock < mPM.getNumBlocks())
    {
        auto& blk  = mPM.getBlock(mAutomEditBlock);
        auto& lane = blk.automationLane;
        if (mAutomCurveHandleDrag < (int)lane.points.size())
        {
            auto& p0 = lane.points[mAutomCurveHandleDrag];
            // Find next sorted point
            float nextTick = 2.f, nextVal = p0.value01;
            for (const auto& pt : lane.points)
                if (pt.timeTicks > p0.timeTicks && pt.timeTicks < nextTick)
                    { nextTick = pt.timeTicks; nextVal = pt.value01; }

            float dv = nextVal - p0.value01;
            if (nextTick <= 1.f && std::abs(dv) >= 0.02f)
            {
                int bx = barToX((float) effectiveStartBars(blk));
                int bw = jmax(4, barToX((float)(effectiveStartBars(blk) + blk.lengthBars)) - bx - 1);
                int by = rowToY(blk.trackRow) + 2;
                int bh = rowHeightPx(blk.trackRow) - 4;
                float dragVal = jlimit(0.f, 1.f, 1.f - (float)(e.y - by) / (float)bh);
                // FL formula inverse at x=0.5: evalFL(0.5, T) = 0.5*T + 0.5 => T = 2*y - 1
                float y_mid   = (dragVal - p0.value01) / dv;
                p0.tension    = jlimit(-0.999f, 0.999f, 2.f * y_mid - 1.f);
            }
        }
        repaint();
        return;
    }

    // ── Automation point drag ─────────────────────────────────────────────────
    if (mAutomDragging && mAutomEditBlock >= 0 && mAutomDragPoint >= 0
        && mAutomEditBlock < mPM.getNumBlocks())
    {
        auto& blk = mPM.getBlock(mAutomEditBlock);
        int bx = barToX((float) effectiveStartBars(blk));
        int bw = jmax(4, barToX((float)(effectiveStartBars(blk) + blk.lengthBars)) - bx - 1);
        int by = rowToY(blk.trackRow) + 2;
        int bh = rowHeightPx(blk.trackRow) - 4;

        float relX = jlimit(0.f, 1.f, (float)(e.x - bx) / (float)bw);
        float relY = jlimit(0.f, 1.f, 1.f - (float)(e.y - by) / (float)bh);
        // Midpoint detent: snap value to 0.5 within ±3%
        if (std::abs(relY - 0.5f) < 0.03f) relY = 0.5f;

        if (mAutomDragPoint < (int)blk.automationLane.points.size())
        {
            blk.automationLane.points[mAutomDragPoint].timeTicks = relX;
            blk.automationLane.points[mAutomDragPoint].value01   = relY;
        }
        repaint();
        return;
    }

    if (mResizing && mResizeIdx >= 0 && mResizeIdx < mPM.getNumBlocks())
    {
        float endBar = snapBar(xToBar(e.x));
        auto& block  = mPM.getBlock(mResizeIdx);
        // QA-Ea Task 0c (2026-05-20): sub-bar precision for the resize drag.
        // Previously: (int)newLen truncated to whole bars + cleared
        // lengthBeats=-1, so resize was bar-locked regardless of snap mode.
        // Now: keep lengthBeats float (sub-bar precision) and only round
        // lengthBars for display.  The snap mode picker (Bar / Beat / Cell /
        // Steps / None) drives finer-than-bar snapping via snapBar().
        float newLenBars = jmax (0.0625f, endBar - mResizeOrigStart);
        block.setLengthBeats ((double) newLenBars * 4.0);
        block.lengthBars  = jmax (1, (int) std::ceil (newLenBars));

        // Automation: rescale timeTicks so control points stay at their absolute positions.
        // QA-Ea Task 0c (2026-05-20): clamp removed.  Previously the
        // jlimit(0.f, 1.f, ...) pinned out-of-range points to the new edge
        // (so shrinking past a dot pulled it to the boundary).  Now dots
        // keep their absolute timeline positions forever: shrinking past a
        // dot just makes it invisible (timeTicks > 1 or < 0), and growing
        // the block back brings it visually back at its original position.
        // No data destroyed.  Drawing / hit-test / playback already skip
        // out-of-range points naturally.
        if (block.clipType == ClipType::Automation && !mResizeOrigPoints.empty()
            && mResizeOrigLen > 0.f)
        {
            block.automationLane.points = mResizeOrigPoints;
            for (auto& pt : block.automationLane.points)
                pt.timeTicks = pt.timeTicks * mResizeOrigLen / newLenBars;
        }

        // QA-Ec (F): the Shift+drag re-fit applies ONCE at mouseUp (no
        // mid-drag audio rebuilds on the hot path) - see the mResizing
        // branch in mouseUp.  This replaced the old no-op Rubber Band stub.
        resized(); repaint();
        return;
    }

    if (mMoving)
    {
        // #27 (QA-G3Smoke, on 8A): fractional move via DELTA-snap (the roll's
        // idiom): the delta comes from raw pixel math, snapping re-anchors the
        // FIRST selected block's target to the grid, and the same delta then
        // applies to every block -- sub-bar phase relationships between
        // selected blocks survive the move.  snapBar is Alt-aware, so
        // Alt+drag = free fine positioning (#28).
        double dBars = (double)(e.x - mMoveDragOrigin.x) / (double) mPPBar;
        int    dRows = (int)std::round((float)(e.y - mMoveDragOrigin.y) / mEffectiveRowH);
        if (! mMoveOrigBars.empty())
        {
            const double snappedFirst = (double) snapBar ((float)((double) mMoveOrigBars[0] + dBars));
            dBars = snappedFirst - (double) mMoveOrigBars[0];
        }
        // QA-Ea Task 0c (2026-05-20 - perf): hoist maxRevealableNegativeBars
        // out of the per-block loop (O(M) per drag fire regardless of
        // selection size).
        const int negFloor = -(int) std::ceil (maxRevealableNegativeBars());
        for (int i = 0; i < (int)mMoveIndices.size(); ++i) {
            int idx = mMoveIndices[i];
            if (idx >= mPM.getNumBlocks()) continue;
            auto& b = mPM.getBlock(idx);
            // QA-Ea Task 0c (2026-05-20 - Option ii): moves may enter the
            // negative-bar zone; the floor matches the viewport's dynamic
            // reveal limit.
            b.startBeats = jmax ((double) negFloor * 4.0,
                                 ((double) mMoveOrigBars[i] + dBars) * 4.0);
            b.trackRow = jlimit(0, kNumRows - 1, mMoveOrigRows[i] + dRows);
        }
        resized(); repaint();
        return;
    }

    if (mDrawing)
    {
        float bar = xToBar(e.x);
        mDrawEnd  = jmax(mDrawStart + 1.f, bar);
        repaint();
        return;
    }

    if (mPainting)
    {
        float bar = snapBar(xToBar(e.x));
        // Paint continuous blocks left-to-right, avoid overlaps.  #25: the
        // stamp size + advance are the pattern's content length (mPaintLenBars,
        // captured at paint start), not a fixed 1 bar.
        if (bar > mPaintLastBar + (float) mPaintLenBars - 0.5f && mPaintRow >= 0)
        {
            ArrangementBlock b;
            b.trackRow     = mPaintRow;
            b.patternIndex = mBrowserSelection;
            b.startBeats   = (double)((int) mPaintLastBar + mPaintLenBars) * 4.0;
            const double contentBeats = (mBrowserSelection >= 0
                                         && mBrowserSelection < mPM.getNumPatterns())
                ? mPM.getPatternContentBeats (mBrowserSelection) : 4.0;
            b.setLengthBeats (contentBeats);
            b.lengthBars   = jmax (1, (int) std::ceil (contentBeats / 4.0));
            b.layerTrack   = true;
            mPM.addBlock(b);
            afterPatternBlockPlaced(b);   // QA-G Task 6
            // #25: track the PLACED stamp's start (not the mouse bar) so the
            // chain butt-joins deterministically at stamp-length intervals.
            mPaintLastBar = (float)((int) mPaintLastBar + mPaintLenBars);
            repaint();
        }
        return;
    }

    // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim).
    // EITHER edge can be slip-dragged when EditMode == Slip (owner-locked
    // 2026-05-20).
    //
    // LEFT EDGE slip (Option A): left edge moves on timeline, right edge
    // stays fixed.  Drag-LEFT = left slides into negative-bar territory,
    // lengthBeats grows, contentStartSamples decreases (pre-roll exposed).
    // Drag-RIGHT = left moves right, lengthBeats shrinks, contentStart
    // increases (leading audio trimmed).
    //
    // RIGHT EDGE slip (slip-out): right edge moves on timeline (via
    // lengthBeats), left edge stays fixed.  ContentStartSamples + startBeats
    // stay untouched.  Drag-RIGHT = clip grows; Drag-LEFT = clip shrinks.
    //
    // Sub-bar precision via snapBeats() (snapBar*4) honors the user's snap
    // mode (Bar / Beat / Cell / Steps / None) so 1/16 / 1/32 steps are
    // achievable when snap is set finer than Bar.
    //
    // Rule-4 UI floor: contentStartSamples never < 0 (audio thread also
    // defensively floors -- belt+suspenders).  When floor hits, lock the
    // remaining drag (further leftward mouse motion stops moving the edge).
    if (mSlipEditing && mSlipEditIdx >= 0 && mSlipEditIdx < mPM.getNumBlocks())
    {
        auto& blk = mPM.getBlock (mSlipEditIdx);
        if (blk.clipType != ClipType::Audio)
        {
            mSlipEditing = false;
            mSlipEditIdx = -1;
            return;
        }
        const double bpm        = onGetBPM        ? onGetBPM()        : 120.0;
        const double sampleRate = onGetSampleRate ? onGetSampleRate() : 44100.0;
        // QA-Ea Task 0c (Option ii): bar-position-based drag (not pixel-delta).
        // currentMouseBars uses xToBar(e.x) which reads mBarOff -- so when
        // auto-scroll changes mBarOff during the drag, the bar-delta from
        // the origMouseBar updates accordingly and the edge follows.
        const double currentMouseBars = (double) xToBar (e.x);
        const double dBarsRaw = currentMouseBars - (double) mSlipEditOrigMouseBar;

        if (mSlipEditEdge == SlipEdge::Left)
        {
            // ── LEFT-edge slip-in (Option A) ─────────────────────────────
            double newStartBeatsProposed = (double) mSlipEditOrigStartBeats
                                         + dBarsRaw * 4.0;
            // Apply the project snap mode (Bar / Beat / Cell / Steps / None).
            // snapBar operates in BAR units, so divide-snap-multiply.
            double newStartBars  = (double) snapBar ((float) (newStartBeatsProposed / 4.0));
            double newStartBeats = newStartBars * 4.0;

            // Sample-domain delta corresponding to the SNAPPED beats delta
            // (so contentStartSamples stays in lockstep with the visible edge).
            double dBeatsSnapped = newStartBeats - (double) mSlipEditOrigStartBeats;
            juce::int64 dSamplesSnapped = (bpm > 0.0 && sampleRate > 0.0)
                ? (juce::int64) (dBeatsSnapped * 60.0 / bpm * sampleRate)
                : (juce::int64) 0;

            juce::int64 newContent = mSlipEditOrigContent + dSamplesSnapped;

            // QA-Ea Task 0c (2026-05-20 B' clamp): contentStartSamples can
            // never go below 0 -- you can't reveal audio that doesn't exist
            // in the file.  Option A has NO dead space on either edge of an
            // Audio clip in Slip mode.  When the user drags the left edge
            // past the file's sample-0 boundary, lock the edge by recomputing
            // newStartBeats from the clamped sample delta.  Audio engine
            // streamer-read sites also defensively floor (Rule-4 belt+
            // suspenders) -- the clamp here makes that floor a dead branch.
            if (newContent < 0)
            {
                newContent = 0;
                if (sampleRate > 0.0 && bpm > 0.0)
                {
                    const double clampedDBeats = (double) (-mSlipEditOrigContent)
                                                / sampleRate * bpm / 60.0;
                    newStartBeats = (double) mSlipEditOrigStartBeats + clampedDBeats;
                    newStartBars  = newStartBeats / 4.0;
                }
            }

            // Right-end stays fixed at the click-time right edge.
            const double origRightEndBeats = (double) mSlipEditOrigStartBeats
                                           + (double) mSlipEditOrigLengthBeats;
            double newLengthBeats = origRightEndBeats - newStartBeats;
            if (newLengthBeats < 0.0625) newLengthBeats = 0.0625;

            blk.contentStartSamples = newContent;
            blk.setStartBeats (newStartBeats);
            blk.setLengthBeats (newLengthBeats);
            blk.lengthBars          = juce::jmax (1,
                                                  (int) std::ceil (newLengthBeats / 4.0));

            // Auto-scroll the viewport LEFT if the dragged edge has moved
            // off the left side.  Floor at maxRevealableNegativeBars() so
            // the viewport only opens up the negative space that actual
            // pre-roll content can fill.
            const float autoScrollMarginBars = 1.0f;
            const double newStartBarsForScroll = newStartBeats / 4.0;
            if ((float) newStartBarsForScroll < mBarOff + autoScrollMarginBars)
            {
                float scrolledBarOff = (float) newStartBarsForScroll - autoScrollMarginBars;
                const float negFloor = -(float) maxRevealableNegativeBars();
                if (scrolledBarOff < negFloor) scrolledBarOff = negFloor;
                if (scrolledBarOff < mBarOff)
                {
                    mBarOff = scrolledBarOff;
                    resized();
                }
            }
        }
        else
        {
            // ── RIGHT-edge slip-out ──────────────────────────────────────
            // Right edge moves on timeline via lengthBeats; contentStart
            // and startBeats stay fixed.  origRightEndBeats was captured at
            // click time as origStart + origLength.
            const double origRightEndBeats = (double) mSlipEditOrigStartBeats
                                           + (double) mSlipEditOrigLengthBeats;
            double newRightEndBeatsProposed = origRightEndBeats + dBarsRaw * 4.0;
            // Apply the project snap mode.
            double newRightEndBars  = (double) snapBar ((float) (newRightEndBeatsProposed / 4.0));
            double newRightEndBeats = newRightEndBars * 4.0;

            // lengthBeats = newRightEnd - origStart.
            double newLengthBeats = newRightEndBeats - (double) mSlipEditOrigStartBeats;
            // Floor at 1/64 note so the clip doesn't disappear.
            if (newLengthBeats < 0.0625) newLengthBeats = 0.0625;

            // QA-Ea Task 0c (2026-05-20 B' clamp): right edge can't extend
            // past the file's actual end.  Mirror of the left-edge
            // contentStart>=0 clamp -- Option A has NO dead space on either
            // edge in Slip mode.  fileRemainingSec = fileTotalSec - contentSec
            // (the playable audio from the current contentStart forward);
            // convert to beats and cap newLengthBeats.  Best-effort: when the
            // thumbnail isn't loaded yet, getTotalLength() returns 0 and the
            // clamp is skipped (audio engine still EOF-protects).
            if (auto* thumb = getOrCreateThumbnail (blk.audioFilePath))
            {
                const double fileTotalSec = thumb->getTotalLength();
                const double contentSec   = (sampleRate > 0.0)
                    ? (double) blk.contentStartSamples / sampleRate
                    : 0.0;
                const double fileRemainingSec = fileTotalSec - contentSec;
                if (fileTotalSec > 0.0 && fileRemainingSec > 0.0 && bpm > 0.0)
                {
                    const double maxLengthBeats = fileRemainingSec * bpm / 60.0;
                    if (newLengthBeats > maxLengthBeats)
                        newLengthBeats = maxLengthBeats;
                }
            }

            blk.setLengthBeats (newLengthBeats);
            blk.lengthBars  = juce::jmax (1, (int) std::ceil (newLengthBeats / 4.0));
            // contentStart + startTicks UNCHANGED.
        }

        repaint();
        // QA-Ea Task 0c (2026-05-20 chop fix): per-drag rebuildAudioClipPlayers
        // was the chop cause -- each rebuild opens the audio file fresh +
        // synchronously pre-fills a 2-second ring buffer per clip, stalling
        // the message thread mid-drag.  Mirror the move-drag pattern: defer
        // the rebuild to mouseUp's commitEdit() -> onArrangementChanged ->
        // rebuildAudioClipPlayers chain (single rebuild per drag).  Visual
        // updates smoothly per frame (independent of the audio snapshot);
        // live-playback during an active slip-drag plays the OLD offset
        // until mouseUp, then snaps -- same as move-drag.
        return;
    }

    // QA-Ea Task 0c (2026-05-20): legacy mSlipping stub mouseDrag branch
    // removed.  AGTool::SlipEdit + the dead "visual only (stub - no audio
    // data shift yet)" repaint-only handler are gone.  The EditMode
    // dropdown drives the real slip math via mSlipEditing above.

    if (mMarqueeActive)
    {
        mMarqueeRect = Rectangle<int>(mMarqueeStart, e.getPosition());
        repaint();
    }
}

void ArrangementGrid::mouseUp(const MouseEvent& e)
{
    // ── B-4/B-5: Slice drag-line release - apply the cut(s) ────────────────────
    if (mSlicing)
    {
        mSlicing = false;
        applySliceLine (mSliceStart, mSliceEnd);   // repaints on success
        repaint();
        return;
    }

    // ── Ruler time selection release ──────────────────────────────────────────
    if (mTimeSelDragging)
    {
        mTimeSelDragging = false;
        // Collapse tiny selections (< 0.1 bars) to no selection
        if (mTimeSelEnd - mTimeSelStart < 0.1f)
        {
            mTimeSelStart = mTimeSelEnd = -1.f;
        }
        else
        {
            // 2026-04-26 (D-7): auto-select every block that overlaps the
            // ruler range so subsequent ops (Delete, Ctrl+B duplicate, etc.)
            // operate on those blocks immediately.  The range itself stays
            // set so timeline-aware operations (Ctrl+B Duplicate Timeline)
            // continue to work.  Overlap rule: block_start < t1 AND
            // block_end > t0 - covers blocks that start before, end after,
            // or fully contain the range, not just blocks whose start lies
            // inside it.
            const float t0 = jmin(mTimeSelStart, mTimeSelEnd);
            const float t1 = jmax(mTimeSelStart, mTimeSelEnd);
            mSelection.clear();
            for (int i = 0; i < mPM.getNumBlocks(); ++i)
            {
                const auto& b = mPM.getBlock(i);
                const float bs = (float) effectiveStartBars (b);
                const float be = bs + (float) effectiveLengthBars(b);
                if (bs < t1 && be > t0)
                    mSelection.push_back(i);
            }
        }
        repaint();
        return;
    }

    // ── Automation curve handle drag release ─────────────────────────────────
    if (mAutomCurveHandleDrag >= 0)
    {
        commitEdit();
        mAutomCurveHandleDrag = -1;
        mAutomEditBlock       = -1;
        repaint();
        return;
    }

    // ── Automation point drag release ─────────────────────────────────────────
    if (mAutomDragging)
    {
        // Sort points by time on release so they're in correct order
        if (mAutomEditBlock >= 0 && mAutomEditBlock < mPM.getNumBlocks())
        {
            auto& lane = mPM.getBlock(mAutomEditBlock).automationLane;
            std::sort(lane.points.begin(), lane.points.end(),
                [](const ControlPoint& a, const ControlPoint& c)
                    { return a.timeTicks < c.timeTicks; });
        }
        commitEdit();
        mAutomDragging  = false;
        mAutomDragPoint = -1;
        mAutomEditBlock = -1;
        repaint();
        return;
    }

    if (mResizing)
    {
        // QA-Ec (F): Shift+drag re-fit commits here - scale the clip's tempo
        // identity by the beat-length ratio and the render ratio does the
        // rest (Stretch: originalBPM/bpm -> pitch-locked re-fit; Resample:
        // the 2b follow term bpm/originalBPM -> rate+pitch re-fit).  One
        // persisted field drives both modes.  Plain (unshifted) drags never
        // enter here with mStretching set, so trim/extend is untouched.
        if (mStretching && mResizeIdx >= 0 && mResizeIdx < mPM.getNumBlocks()
            && mStretchOrigBeats > 0.0)
        {
            auto&        blk     = mPM.getBlock (mResizeIdx);
            const double newBeats = effectiveLengthBeats (blk);
            if (blk.clipType == ClipType::Audio && blk.originalBPM > 0.f
                && newBeats > 0.0
                && std::abs (newBeats - mStretchOrigBeats) > 1e-9)
            {
                blk.originalBPM = (float) juce::jlimit (1.0, 999.0,
                    (double) blk.originalBPM * (newBeats / mStretchOrigBeats));
                // G1 boundary: a re-fit is a per-copy customization - detach
                // from the library master so the follow dot stays truthful.
                blk.isOverride = true;
            }
        }
        commitEdit(); mResizing = false; mStretching = false; mResizeIdx = -1;
        mStretchOrigBeats = 0.0;
        updateCursor(); return;
    }
    if (mMoving)
    {
        commitEdit();
        // QA-G Task 6: user-set patterns' linked markers relocate with the
        // move (commitEdit's cleanup removed the stale ones; re-spawn at the
        // new positions), then follower patterns whose governing signature
        // changed get the Proceed / Lock Previous TS prompt (docket #3).
        for (int mvIdx : mMoveIndices)
            if (mvIdx >= 0 && mvIdx < mPM.getNumBlocks())
                afterPatternBlockPlaced (mPM.getBlock (mvIdx));
        for (const auto& pre : mMovePreTs)
        {
            const int pi = pre[0];
            if (pi < 0 || pi >= mPM.getNumPatterns()) continue;
            const auto& p = mPM.getPattern (pi);
            if (p.tsLocked) continue;
            if (p.tsNum == pre[1] && p.tsDen == pre[2]) continue;
            auto* aw = new AlertWindow ("Time Signature Change",
                "\"" + p.name + "\" moved into a different time signature ("
                    + String (pre[1]) + "/" + String (pre[2]) + " -> "
                    + String (p.tsNum) + "/" + String (p.tsDen) + ").",
                AlertWindow::QuestionIcon);
            aw->addButton ("Proceed", 1, KeyPress (KeyPress::returnKey));
            aw->addButton ("Lock Previous TS", 2);
            aw->enterModalState (true,
                ModalCallbackFunction::create ([this, pi, pre] (int r)
                {
                    if (r == 2)
                        performPatternTsOp ("Lock Previous TS",
                                            [this, pi, &pre]
                                            { mPM.setPatternTimeSig (pi, pre[1], pre[2]); });
                }), true);
        }
        mMovePreTs.clear();
        mMoving = false; mMoveIndices.clear(); updateCursor(); return;
    }
    // QA-Ea Task 0c (FL pre-roll record): finalize the slip-edit drag.
    // commitEdit() snapshots the new contentStartSamples / lengthBeats for
    // undo + fires onArrangementChanged -> rebuildAudioClipPlayers so the
    // audio engine picks up the new contentStartSamples + lengthBeats /
    // clipStartBeat values.  Single rebuild per drag (chop fix 2026-05-20).
    if (mSlipEditing) {
        commitEdit();
        mSlipEditing = false;
        mSlipEditIdx = -1;
        updateCursor();
        return;
    }
    // QA-Ea Task 0c (2026-05-20): legacy mSlipping mouseUp handler removed
    // alongside the AGTool::SlipEdit tool.

    if (mPainting) {
        commitEdit();
        mPainting = false;
        mPaintRow = -1;
        resized(); repaint();
        return;
    }

    if (mDrawing)
    {
        mDrawing = false;
        float start = snapBarAlt(mDrawStart);
        float end   = snapBarAlt(mDrawEnd);
        int   len   = jmax(1, (int)(end - start));
        int   row   = mDrawRow;
        if (row >= 0 && row < kNumRows)
        {
            // QA-H Task 8 (#20): the empty-grid Draw places the last-clicked
            // browser entry's TYPE (pattern / audio clip / automation).
            if (mDropKind == BrowserDropKind::Audio)
            {
                if (mDropAudioIdx >= 0)
                {
                    // #26 (G-15): audio click-copy -- the placed block takes
                    // the clicked block's length + content start when a plain
                    // click has memory (count-guarded: the place path may
                    // decline/prompt without adding).
                    const int preCount = mPM.getNumBlocks();
                    placeAudioLibraryEntry (mDropAudioIdx, row, start);
                    if (! e.mouseWasDraggedSinceMouseDown()
                        && mClickMemoryLenBeats > 0.0
                        && mPM.getNumBlocks() == preCount + 1)
                    {
                        auto& nb = mPM.getBlock (mPM.getNumBlocks() - 1);
                        if (nb.clipType == ClipType::Audio)
                        {
                            nb.setLengthBeats (mClickMemoryLenBeats);
                            nb.lengthBars = jmax (1, (int) std::ceil (mClickMemoryLenBeats / 4.0));
                            nb.contentStartSamples = mClickMemoryContentStartSamples;
                            mPM.notifyContentChanged();
                        }
                    }
                }
            }
            else if (mDropKind == BrowserDropKind::Automation)
            {
                if (mDropAutomIdx >= 0
                    && mDropAutomIdx < mPM.getNumAutomationTemplates())
                {
                    beginEdit("Draw");
                    ArrangementBlock b;
                    b.clipType       = ClipType::Automation;
                    b.trackRow       = row;
                    b.startBeats     = (double)(int) start * 4.0;
                    b.lengthBars     = len;
                    b.automationLane = mPM.getAutomationTemplate(mDropAutomIdx);
                    mPM.addBlock(b);
                    commitEdit();
                    mSelection.clear();
                    mSelection.push_back(mPM.getNumBlocks() - 1);
                }
            }
            else
            {
                beginEdit("Draw");
                ArrangementBlock b;
                b.trackRow     = row;
                b.patternIndex = mBrowserSelection;
                b.startBeats   = (double)(int) start * 4.0;
                // #26 (G-15): a plain CLICK places a copy of the last-clicked
                // block -- length + content offset ride the click memory (the
                // identity already rides mBrowserSelection).  #25: with no
                // memory, a click sizes from the pattern's REAL content
                // length.  A real DRAG keeps the user-drawn length.
                if (! e.mouseWasDraggedSinceMouseDown() && mClickMemoryLenBeats > 0.0)
                {
                    b.setLengthBeats (mClickMemoryLenBeats);
                    b.lengthBars = jmax (1, (int) std::ceil (mClickMemoryLenBeats / 4.0));
                    b.contentOffsetTicks = mClickMemoryOffsetTicks;
                }
                else if (! e.mouseWasDraggedSinceMouseDown()
                         && mBrowserSelection >= 0
                         && mBrowserSelection < mPM.getNumPatterns())
                {
                    const double contentBeats = mPM.getPatternContentBeats (mBrowserSelection);
                    b.setLengthBeats (contentBeats);
                    b.lengthBars = jmax (1, (int) std::ceil (contentBeats / 4.0));
                }
                else
                    b.lengthBars = len;
                b.layerTrack   = true;
                mPM.addBlock(b);
                commitEdit();
                afterPatternBlockPlaced(b);   // QA-G Task 6
                mSelection.clear();
                mSelection.push_back(mPM.getNumBlocks() - 1);
            }
        }
        mDrawRow = -1;
        resized(); repaint();
        return;
    }

    if (mMarqueeActive) { finaliseMarquee(); }

    // 2026-04-26 (D-1): finalize Ctrl+RClick zoom-rect drag.
    if (mZoomRectActive)
    {
        if (mZoomRect.getWidth() > 8 && mPPBar > 0)
        {
            float vpW = 800.f;
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                vpW = (float)jmax(1, vp->getWidth());
            const float startBar = xToBar(mZoomRect.getX());
            const float endBar   = xToBar(mZoomRect.getRight());
            if (endBar > startBar)
            {
                const float minPP = minZoomPPBar (vpW), maxPP = maxZoomPPBar (vpW);
                mPPBar  = jlimit(minPP, maxPP, vpW / (endBar - startBar));
                // QA-Ea Task 0c (Option ii): allow negative-bar zoom-to-rect.
                mBarOff = jmax(-(float) maxRevealableNegativeBars(), startBar);
                resized(); repaint();
            }
        }
        mZoomRectActive = false;
        mZoomRect       = {};
        repaint();
    }

    (void)e;
}

void ArrangementGrid::mouseDoubleClick(const MouseEvent& e)
{
    if (e.y < rulerPinY() + kRulerH) return;
    int hit = blockAtPos(e.x, e.y);
    if (hit >= 0 && mPM.getBlock(hit).clipType == ClipType::Audio)
        showAudioClipProperties(hit);
    else if (hit >= 0 && mPM.getBlock(hit).clipType == ClipType::Automation)
    {
        // Check if double-clicking a curve handle diamond → reset its tension to 0
        if (mActiveTool == AGTool::Draw)
        {
            const auto& b  = mPM.getBlock(hit);
            int bx = barToX((float) effectiveStartBars(b));
            int bw = jmax(4, barToX((float)(effectiveStartBars(b) + effectiveLengthBars(b))) - bx - 1);
            int by = rowToY(b.trackRow) + 2;
            int bh = rowHeightPx(b.trackRow) - 4;
            int chIdx = hitTestAutomCurveHandleInBlock(b.automationLane,
                bx, bw, by, bh, e.x, e.y);
            if (chIdx >= 0)
            {
                beginEdit("Reset Curve Tension");
                mPM.getBlock(hit).automationLane.points[chIdx].tension = 0.f;
                commitEdit();
                repaint();
                return;
            }
        }

        // Open dedicated Event Editor window
        if (onOpenEventEditor) onOpenEventEditor(hit);
        else {
            // Fallback: re-enter in-place edit mode
            setTool(AGTool::Draw);
            mAutomEditBlock = hit;
            repaint();
        }
    }
}

void ArrangementGrid::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    // Compute viewport dimensions for dynamic zoom limits
    float vpW = 800.f, vpH = 600.f;
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        vpW = (float)jmax(1, vp->getWidth());
        vpH = (float)jmax(1, vp->getHeight());
    }

    if (e.mods.isCtrlDown())
    {
        // Horizontal zoom anchored to cursor: bar under mouse stays under
        // mouse. Limits: full out = 512 bars in viewport, full in = 1 bar.
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        float minPP  = minZoomPPBar (vpW);
        float maxPP  = maxZoomPPBar (vpW);
        const float anchorBar = xToBar(e.x);
        mPPBar = jlimit(minPP, maxPP, mPPBar * factor);
        // QA-Ea Task 0c (Option ii): allow negative-bar zoom anchor.
        mBarOff = jmax(-(float) maxRevealableNegativeBars(),
                       anchorBar - (float)e.x / jmax(1.f, mPPBar));
        resized(); repaint();
    }
    else if (e.mods.isAltDown())
    {
        // Alt+scroll = vertical zoom anchored to the cursor: the row under the
        // mouse stays under the mouse.  Range is fixed in pixels (see
        // kMinRowH / kMaxRowH) so it does not move with the window.
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        const float minRH = (float) kMinRowH;
        const float maxRH = (float) kMaxRowH;
        const float oldRH = mEffectiveRowH;
        mEffectiveRowH = jlimit(minRH, maxRH, mEffectiveRowH * factor);
        resized();
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            const float actual  = (oldRH > 0.f) ? mEffectiveRowH / oldRH : 1.f;
            const int   newVpY  = (int) std::round ((float) vp->getViewPositionY()
                                      + (float) (e.y - kRulerH) * (actual - 1.f));
            vp->setViewPosition (vp->getViewPositionX(), jmax (0, newVpY));
        }
        repaint();
        if (onRowHeightChanged) onRowHeightChanged();
    }
    else if (e.mods.isShiftDown())
    {
        // 2026-04-26 (B-4): Shift+wheel = horizontal scroll.  FL convention:
        // wheel up = scroll right (toward later bars).  Sign is flipped vs.
        // the vertical-scroll branch because timeline scrolling reads as
        // "advance through" instead of "scroll the surface".
        // QA-Ea Task 0c (Option ii): allow negative-bar horizontal scroll.
        mBarOff = jmax(-(float) maxRevealableNegativeBars(),
                       mBarOff + wheel.deltaY * 4.f);
        resized(); repaint();
    }
    else
    {
        // Bare wheel = vertical scroll via the parent Viewport.  Builder grid
        // is always hosted inside one (resized() math depends on it).
        // Standard Windows convention: wheel up (deltaY > 0) → view position
        // decreases → earlier rows come into view at the top.
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            const int dy = (wheel.deltaY > 0.f ? -1 : 1) * 40;   // ~1 row per click
            vp->setViewPosition(vp->getViewPositionX(),
                                vp->getViewPositionY() + dy);
        }
    }
}

bool ArrangementGrid::keyPressed(const KeyPress& key)
{
    // D-4: while typing-keyboard mode is on, mapped note keys (and the PgUp/
    // PgDn octave shift) must bubble up to StandaloneEditor's converter
    // instead of firing the single-letter tool shortcuts below.
    if (TypingKeyboardMap::shouldBypassLocalKeys (key)) return false;

    const bool ctrl  = key.getModifiers().isCtrlDown();
    const bool shift = key.getModifiers().isShiftDown();
    const bool alt   = key.getModifiers().isAltDown();

    // Tool shortcuts (no modifiers)
    // KeyPress::operator== does case-insensitive keyCode comparison - use it for bare letters
    if (!ctrl && !alt && !shift)
    {
        if (key == KeyPress('p')) { setTool(AGTool::Draw);        return true; }
        if (key == KeyPress('b')) { setTool(AGTool::Paint);       return true; }
        if (key == KeyPress('e')) { setTool(AGTool::Select);      return true; }
        if (key == KeyPress('d')) { setTool(AGTool::Delete);      return true; }
        if (key == KeyPress('t')) { setTool(AGTool::Mute);        return true; }
        // QA-Ea Task 0c (2026-05-20): page-local 'S' keybind removed.  'S'
        // now lives in the ApplicationCommandManager as
        // cmdToggleSlipStretchMode and toggles the Slip/Stretch dropdown
        // in the toolbar (see StandaloneEditor::perform + KeyBindings).
        if (key == KeyPress('c')) { setTool(AGTool::Slice);       return true; }
        if (key == KeyPress('y')) { setTool(AGTool::PlaySelected);return true; }
        // 2026-04-26 (B-4): bare Z = Zoom tool (was Shift+Z).
        if (key == KeyPress('z')) { setTool(AGTool::Zoom);        return true; }

        // 2026-04-26 (B-2 / B-6): F2 + F4 are now global keymap commands
        // (`cmdRenameActivePattern` / `cmdNewPattern`).  Builder used to
        // intercept them locally for "rename track row" + "find next empty
        // bar".  Track-row rename remains accessible via right-click on the
        // row label; "find next empty bar" was redundant with the new
        // `cmdNextEmptyPattern` (F3).
    }

    // 2026-04-26 (D-1 / D-2): Alt+letter handlers.
    // Normalize keycode to lowercase via `| 32` - JUCE on Windows can return
    // either case for Alt+letter depending on shift state, mirroring the
    // Piano Roll Alt-tool-letter pattern (`PianoRoll.cpp:886`).
    if (alt && !ctrl)
    {
        const int kcLower = key.getKeyCode() | 32;
        if (kcLower == 'm')   // Alt+M / Alt+Shift+M = mute / unmute selection
        {
            muteSelected (! shift);
            return true;
        }
        if (kcLower == 't')   // Alt+T = add marker; Alt+Shift+T = add TS change
        {
            const int barAtHead = juce::jmax (0, (int) std::floor (mPlayheadBar));
            if (shift) promptAddTimeSigChange (barAtHead);
            else       promptAddTimeMarker    (barAtHead);
            return true;
        }
    }

    // Ctrl shortcuts
    // JUCE on Windows stores letter keyCodes as the uppercase ASCII value (MapVirtualKey MAPVK_VK_TO_CHAR),
    // e.g. 'A'=65, 'Z'=90. Use uppercase literals with getKeyCode() inside modifier blocks.
    // 2026-04-26 (B-5): Ctrl+Z / Ctrl+Alt+Z migrated to global BSCommands -
    // page-local Ctrl+Z handler removed.  Other Ctrl shortcuts stay local.
    if (ctrl && !alt) {
        const int kc = key.getKeyCode();
        if (!shift && kc == 'A') { selectAll(); return true; }
        if (!shift && kc == 'C') { copySelected(); return true; }
        if (!shift && kc == 'V') { pasteClipboard(); return true; }
        if (!shift && kc == 'B') { duplicateSelected(); return true; }
        // 2026-04-26 (D-5): Ctrl+P migrated to global cmdToggleRecordingPrecount.
        // Import Audio stays in the Builder menu; no longer keybound here.

        // 2026-04-26 (D-7): Ctrl+Delete = remove the time region and slide
        // every later block left by the removed bar count.  isKeyCode(int)
        // forces the int comparison; `key == KeyPress::deleteKey` would
        // resolve to operator==(KeyPress&) and compare modifiers, never
        // matching a Ctrl-held press.
        if (!shift && (key.isKeyCode(KeyPress::deleteKey) || key.isKeyCode(KeyPress::backspaceKey)))
            { deleteTimeRegion(); return true; }
        // 2026-04-26 (D-7 sub-3): Ctrl+Left/Right shifts the ruler time-
        // selection box by its own length (contents stay put).
        if (!shift && key.isKeyCode(KeyPress::leftKey))
            { shiftTimeSelectionLeft();  return true; }
        if (!shift && key.isKeyCode(KeyPress::rightKey))
            { shiftTimeSelectionRight(); return true; }

        // Ctrl+Shift+1-6 = zoom presets (digit keyCodes match their ASCII values directly)
        if (shift) {
            static const float kZoomPresets[] = { 20.f, 40.f, 80.f, 120.f, 200.f, 300.f };
            for (int i = 0; i < 6; ++i)
                if (kc == '1' + i) {
                    float vpW = 800.f;
                    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                        vpW = (float)jmax(1, vp->getWidth());
                    mPPBar = jlimit(minZoomPPBar(vpW), maxZoomPPBar(vpW), kZoomPresets[i]);
                    resized(); repaint(); return true;
                }
        }
    }

    // (2026-04-26 B-4) Bare Z handled in the no-modifier tool letters block above.

    // Delete / Backspace
    if (!ctrl && (key == KeyPress::deleteKey || key == KeyPress::backspaceKey))
        { deleteSelected(); return true; }

    // Escape = deselect
    if (key == KeyPress::escapeKey) { clearSelection(); return true; }

    // Shift+Arrows = nudge
    if (shift && !ctrl && !alt) {
        if (key == KeyPress::leftKey)  { nudgeSelection(-1,  0); return true; }
        if (key == KeyPress::rightKey) { nudgeSelection(+1,  0); return true; }
        if (key == KeyPress::upKey)    { nudgeSelection( 0, -1); return true; }
        if (key == KeyPress::downKey)  { nudgeSelection( 0, +1); return true; }
    }

    // 2026-04-26 (B-4): Page Up/Down behavior is tool-dependent - when the
    // Zoom tool is active they zoom in/out (matching the spreadsheet spec);
    // otherwise they fall through to vertical viewport scroll.
    if (!ctrl && !shift && !alt) {
        if (key == KeyPress::pageUpKey || key == KeyPress::pageDownKey) {
            const bool zoomIn = (key == KeyPress::pageUpKey);
            if (mActiveTool == AGTool::Zoom) {
                float vpW = 800.f;
                if (auto* vp = findParentComponentOfClass<Viewport>())
                    vpW = (float)jmax(1, vp->getWidth());
                const float minPP = minZoomPPBar (vpW), maxPP = maxZoomPPBar (vpW);
                const float factor = zoomIn ? 1.15f : (1.f / 1.15f);
                mPPBar = jlimit(minPP, maxPP, mPPBar * factor);
                resized(); repaint();
                return true;
            }
            if (auto* vp = findParentComponentOfClass<Viewport>()) {
                int dy = (zoomIn ? -1 : 1) * vp->getHeight();
                vp->setViewPosition(vp->getViewPositionX(), vp->getViewPositionY() + dy);
                return true;
            }
        }
    }

    return false;
}

void ArrangementGrid::resized()
{
    // Get viewport size so we never leave blank space at the edges
    float vpW = 800.f, vpH = 600.f;
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        vpW = (float)jmax(1, vp->getWidth());
        vpH = (float)jmax(1, vp->getHeight());
    }
    // Horizontal panning is virtual (mBarOff shifts barToX's origin): the
    // component never exceeds the viewport width, so the viewport scrolls
    // VERTICAL only and BuilderPage's external scrollbar is the single
    // horizontal mechanism.
    int w = (int)vpW;
    int h = jmax((int)vpH, kRulerH + (int)(kNumRows * mEffectiveRowH));
    setSize(w, h);
}

// ─────────────────────────────────────────────────────────────────────────────
// TrackHeaderPanel
// ─────────────────────────────────────────────────────────────────────────────
TrackHeaderPanel::TrackHeaderPanel(ArrangementGrid& grid, PatternManager& pm)
    : mGrid(grid), mPM(pm)
{
}

void TrackHeaderPanel::setViewportYOffset(int yPixels)
{
    if (mYOffset != yPixels) { mYOffset = yPixels; repaint(); }
}

void TrackHeaderPanel::notifyArrangementChanged()
{
    if (mGrid.onArrangementChanged) mGrid.onArrangementChanged();
    mPM.notifyContentChanged();
}

// Default band colours for newly-created track groups, cycled by group id.
static constexpr juce::uint32 kGroupPalette[] = {
    0xffe06868, 0xff5aa8d8, 0xff6cc070, 0xffd8a050,
    0xffb078d0, 0xff50c0b0, 0xffcfcf5e, 0xffe07898,
};

int TrackHeaderPanel::yToRow(int y) const
{
    // Delegate to the grid's mapping so header rows can never drift from the
    // grid rows (int row-height truncation used to accumulate down the list).
    return mGrid.yToRow(y + mYOffset);
}

// Vertical offset of an LED from the top of its track row.
// Leaves room above for the "M" / "S" label when the row is tall enough;
// centers the LED if the row was shrunk below the label-fit threshold.
static int ledTopInRow(int rh)
{
    constexpr int kLabelH = 10;
    const bool drawLabels = (rh >= 26);
    return drawLabels ? (2 + kLabelH + 1)
                      : ((rh - TrackHeaderPanel::kLedSize) / 2);
}

void TrackHeaderPanel::paint(Graphics& g)
{
    auto b = getLocalBounds();

    // Ruler corner
    g.setColour(kHeaderBg.darker(0.2f));
    g.fillRect(0, 0, b.getWidth(), ArrangementGrid::kRulerH);
    g.setColour(VC::Accent.withAlpha(0.6f));
    g.drawHorizontalLine(ArrangementGrid::kRulerH - 1, 0.f, (float)b.getWidth());
    // Jeff, 2026-08-04: the corner square beside the ruler is BLANK.  It used
    // to carry a "BUILDER" caption AND -- worse -- the row loop below drew into
    // it, because a scrolled row 0 lands at kRulerH - mYOffset, which is
    // negative.  Track names scrolled up over the ruler line.  Clip the rows to
    // the band under the ruler so nothing can ever cross it.
    g.saveState();
    g.reduceClipRegion (0, ArrangementGrid::kRulerH,
                        b.getWidth(), jmax (0, b.getHeight() - ArrangementGrid::kRulerH));

    // Track rows -- geometry comes from the grid's rowToY/rowHeightPx so the
    // labels stay pixel-locked to the grid rows at every zoom level.
    const auto& names = mGrid.getRowNames();
    for (int r = 0; r < ArrangementGrid::kNumRows; ++r)
    {
        const int rh = jmax(1, mGrid.rowHeightPx(r));
        int y = mGrid.rowToY(r) - mYOffset;
        if (y + rh < 0 || y > b.getHeight()) continue;

        g.setColour(kHeaderBg);
        g.fillRect(0, y, b.getWidth(), rh);

        // Subtle alternating tint
        if (r % 2 == 0) {
            g.setColour(Colours::white.withAlpha(0.02f));
            g.fillRect(0, y, b.getWidth(), rh);
        }

        // Track-group signifier: colour band + soft row tint (visual-only V1).
        // Band sits at x 0..2; LEDs start at kLedMuteX = 4, no overlap.
        const int gid = mPM.getRowGroup(r);
        if (gid > 0)
        {
            Colour gc (mPM.getRowGroupColor(r));
            if (gc.getARGB() == 0)
                gc = Colour (kGroupPalette[(gid - 1) % (int) juce::numElementsInArray (kGroupPalette)]);
            g.setColour(gc.withAlpha(0.10f));
            g.fillRect(0, y, b.getWidth(), rh);
            g.setColour(gc.withAlpha(0.9f));
            g.fillRect(0, y, 3, rh);
        }

        // ── Mute / Solo LEDs ────────────────────────────────────────────
        const int  ledYOff = ledTopInRow(rh);
        const int  ledY    = y + ledYOff;
        const bool muted   = mPM.isRowMuted(r);
        const bool soloed  = mPM.isRowSoloed(r);

        // "M" / "S" text labels above each LED (skipped when row is too short)
        if (rh >= 26)
        {
            g.setColour(VC::TextDim);
            g.setFont(Font(8.f, Font::bold));
            g.drawText("M", kLedMuteX, y + 2, kLedSize, 10,
                       Justification::centred, false);
            g.drawText("S", kLedSoloX, y + 2, kLedSize, 10,
                       Justification::centred, false);
        }

        // Mute LED (red when on, dim when off)
        g.setColour(muted ? Colour(0xffd04040) : Colour(0xff3a3a42));
        g.fillEllipse((float)kLedMuteX, (float)ledY, (float)kLedSize, (float)kLedSize);
        g.setColour(Colour(0xff202028));
        g.drawEllipse((float)kLedMuteX, (float)ledY, (float)kLedSize, (float)kLedSize, 1.f);

        // Solo LED (yellow when on, dim when off)
        g.setColour(soloed ? Colour(0xffe8c040) : Colour(0xff3a3a42));
        g.fillEllipse((float)kLedSoloX, (float)ledY, (float)kLedSize, (float)kLedSize);
        g.setColour(Colour(0xff202028));
        g.drawEllipse((float)kLedSoloX, (float)ledY, (float)kLedSize, (float)kLedSize, 1.f);

        g.setColour(VC::TextDim);
        g.setFont(Font(10.f, Font::bold));
        g.drawText(names[r], kLabelXOff, y + 1,
                   b.getWidth() - kLabelXOff - 4, rh - 2,
                   Justification::centredLeft, true);

        g.setColour(kGridLine);
        g.drawHorizontalLine(y + rh - 1, 0.f, (float)b.getWidth());
    }

    g.restoreState();

    // Opaque corner square: repainted AFTER the rows so a partially-scrolled
    // row can never bleed through, and deliberately empty.
    g.setColour(kHeaderBg);
    g.fillRect(0, 0, b.getWidth(), ArrangementGrid::kRulerH);
    g.setColour(kGridLine);
    g.drawHorizontalLine(ArrangementGrid::kRulerH - 1, 0.f, (float)b.getWidth());

    // Right border
    g.setColour(VC::Accent.withAlpha(0.8f));
    g.drawVerticalLine(b.getWidth() - 1, (float)ArrangementGrid::kRulerH, (float)b.getHeight());
}

void TrackHeaderPanel::resized() {}

int TrackHeaderPanel::hitTestLed(int x, int y, int& kind) const
{
    kind = -1;
    if (y < ArrangementGrid::kRulerH) return -1;
    const int gridY = y + mYOffset;
    const int row   = mGrid.yToRow(gridY);
    if (row < 0 || row >= ArrangementGrid::kNumRows) return -1;
    const int rh     = jmax(1, mGrid.rowHeightPx(row));
    const int yInRow = gridY - mGrid.rowToY(row);
    const int ledTop = ledTopInRow(rh);
    if (yInRow < ledTop || yInRow >= ledTop + kLedSize) return -1;
    if (x >= kLedMuteX && x < kLedMuteX + kLedSize) { kind = 0; return row; }
    if (x >= kLedSoloX && x < kLedSoloX + kLedSize) { kind = 1; return row; }
    return -1;
}

void TrackHeaderPanel::mouseDown(const MouseEvent& e)
{
    if (e.y < ArrangementGrid::kRulerH) return;

    // LED hit-test (left-click only; right-click falls through to context menu)
    if (!e.mods.isRightButtonDown())
    {
        int kind = -1;
        int row  = hitTestLed(e.x, e.y, kind);
        if (row >= 0 && kind >= 0)
        {
            if (kind == 0) mPM.setRowMuted (row, !mPM.isRowMuted (row));
            else           mPM.setRowSoloed(row, !mPM.isRowSoloed(row));
            repaint();
            return;
        }
    }

    int row = yToRow(e.y);
    if (row < 0 || row >= ArrangementGrid::kNumRows) return;

    if (e.mods.isRightButtonDown()) {
        showTrackContextMenu(row);
    }
}

void TrackHeaderPanel::mouseDoubleClick(const MouseEvent& e)
{
    if (e.y < ArrangementGrid::kRulerH) return;
    int row = yToRow(e.y);
    if (row < 0 || row >= ArrangementGrid::kNumRows) return;

    // Double-click to rename
    auto* aw = new AlertWindow("Rename Track", "Enter a new name:", AlertWindow::NoIcon);
    aw->addTextEditor("name", mGrid.getRowNames()[row]);
    aw->addButton("OK", 1); aw->addButton("Cancel", 0);
    aw->enterModalState(true,
        ModalCallbackFunction::create([this, row, aw](int r) {
            if (r == 1) {
                auto n = aw->getTextEditorContents("name").trim();
                if (n.isNotEmpty()) {
                    // QA-UndoCoverage Task 4: renames bypassed the bracket the
                    // sibling Move/Delete handlers already use.
                    mGrid.beginEdit("Rename Track");
                    mGrid.setRowName(row, n);
                    mGrid.commitEdit();
                }
            }
        }), true);
}

void TrackHeaderPanel::groupSpan (int row, int& first, int& last) const
{
    const int gid = mPM.getRowGroup (row);
    first = last = row;
    if (gid <= 0) return;
    while (first > 0 && mPM.getRowGroup (first - 1) == gid) --first;
    while (last < ArrangementGrid::kNumRows - 1 && mPM.getRowGroup (last + 1) == gid) ++last;
}

void TrackHeaderPanel::showTrackContextMenu(int row)
{
    const int gid = mPM.getRowGroup(row);

    // TS7 §7.1: "Render Track to WAV" is offered on AUDIO rows only, and that is
    // a property of the arrangement rather than a UI preference.  An Audio row
    // maps to exactly one mixer strip (MixerChannelIds::audioInsert(row)), so the
    // render is one strip's tap; a Pattern row is not a channel at all (a pattern
    // plays EVERY tab) and an Automation row has no audio.  Shown DISABLED rather
    // than hidden on other row kinds, so the capability is discoverable instead of
    // appearing to be missing -- the same rule the plugin scanner follows for the
    // files it skips.
    bool rowHasAudio = false;
    for (int bi = 0; bi < mPM.getNumBlocks() && ! rowHasAudio; ++bi)
    {
        const auto& blk = mPM.getBlock (bi);
        rowHasAudio = (blk.trackRow == row && blk.clipType == ClipType::Audio);
    }

    PopupMenu m;
    m.addItem(1, "Rename...");
    m.addSeparator();
    m.addItem(2, "Move Up");
    m.addItem(3, "Move Down");
    m.addItem(8, "Insert Track Above");
    m.addSeparator();
    m.addItem(5, "Group with Above", row > 0);
    m.addItem(6, "Remove from Group", gid > 0);
    m.addItem(7, "Color Group...", gid > 0);
    m.addSeparator();
    m.addItem(9, "Render Track to WAV...", rowHasAudio);
    m.addSeparator();
    m.addItem(4, "Delete Track Clips");

    m.showMenuAsync(PopupMenu::Options(), [this, row](int result)
    {
        switch (result)
        {
            case 1:
            {
                auto* aw = new AlertWindow("Rename Track", "New name:", AlertWindow::NoIcon);
                aw->addTextEditor("n", mGrid.getRowNames()[row]);
                aw->addButton("OK", 1); aw->addButton("Cancel", 0);
                aw->enterModalState(true,
                    ModalCallbackFunction::create([this, row, aw](int r) {
                        if (r == 1) {
                            auto n = aw->getTextEditorContents("n").trim();
                            if (n.isNotEmpty()) {
                                mGrid.beginEdit("Rename Track");
                                mGrid.setRowName(row, n);
                                mGrid.commitEdit();
                            }
                        }
                    }), true);
                break;
            }
            case 2:
            case 3:
            {
                // #21/#22 (QA-G3Smoke): moving a grouped track moves its WHOLE
                // contiguous group span past the NEIGHBOR'S full span (spans
                // hop each other as units); group ids + colors travel with
                // their rows -- the old per-row id swap is gone (it tore
                // groups apart and left ids behind).
                int aFirst, aLast;
                groupSpan (row, aFirst, aLast);
                const int nbrRow = (result == 2) ? aFirst - 1 : aLast + 1;
                if (nbrRow < 0 || nbrRow >= ArrangementGrid::kNumRows) break;
                int bFirst, bLast;
                groupSpan (nbrRow, bFirst, bLast);
                mGrid.beginEdit((result == 2) ? "Move Track Up" : "Move Track Down");
                const int lo   = jmin (aFirst, bFirst);
                const int hi   = jmax (aLast,  bLast);
                const int lenA = aLast - aFirst + 1;
                auto newRowOf = [&] (int r) -> int
                {
                    if (r >= aFirst && r <= aLast)
                        return (result == 2) ? lo + (r - aFirst)
                                             : (hi - lenA + 1) + (r - aFirst);
                    return (result == 2) ? r + lenA : r - lenA;
                };
                // Capture the whole affected range, then rewrite through the
                // rotation map.  Positional-default names ("Track N") stay
                // positional -- only custom names travel with their track.
                struct RowState { juce::String name; bool custom;
                                  bool mute; bool solo; int gid2; juce::uint32 col; };
                std::vector<RowState> st ((size_t)(hi - lo + 1));
                auto names = mGrid.getRowNames();
                for (int r = lo; r <= hi; ++r)
                {
                    auto& s  = st[(size_t)(r - lo)];
                    s.name   = names[r];
                    s.custom = (names[r] != mPM.defaultRowName (r));
                    s.mute   = mPM.isRowMuted (r);
                    s.solo   = mPM.isRowSoloed (r);
                    s.gid2   = mPM.getRowGroup (r);
                    s.col    = mPM.getRowGroupColor (r);
                }
                for (int r = lo; r <= hi; ++r)
                {
                    const auto& s = st[(size_t)(r - lo)];
                    const int  nr = newRowOf (r);
                    mGrid.setRowName (nr, s.custom ? s.name : mPM.defaultRowName (nr));
                    mPM.setRowMuted (nr, s.mute);
                    mPM.setRowSoloed (nr, s.solo);
                    mPM.setRowGroup (nr, s.gid2);
                    mPM.setRowGroupColor (nr, s.col);
                }
                for (int i = 0; i < mPM.getNumBlocks(); ++i)
                {
                    auto& b = mPM.getBlock(i);
                    if (b.trackRow >= lo && b.trackRow <= hi)
                        b.trackRow = newRowOf (b.trackRow);
                }
                mGrid.commitEdit();
                notifyArrangementChanged();
                mGrid.repaint();
                repaint();
                break;
            }
            case 4:
            {
                mGrid.beginEdit("Delete Track Clips");
                for (int i = mPM.getNumBlocks() - 1; i >= 0; --i)
                    if (mPM.getBlock(i).trackRow == row) mPM.removeBlock(i);
                mGrid.commitEdit();
                notifyArrangementChanged();
                mGrid.repaint();
                repaint();
                break;
            }
            case 9:
            {
                // TS7 §7.1: consolidate this row to one WAV.  The intent is a
                // chopped-and-spliced vocal becoming a single stem, so the render
                // is that row's STRIP tapped through the one-pass mechanism TS2's
                // stems already use -- which keeps sidechain-driven behaviour by
                // construction rather than muting everything else.
                if (onRenderTrackRow) onRenderTrackRow (row);
                break;
            }
            case 5:
            {
                // #23 (QA-G3Smoke): group edits are undoable -- the existing
                // begin/commit bracket already snapshots row group/color state.
                const int above = row - 1;
                if (above < 0) break;
                mGrid.beginEdit("Group with Above");
                int g = mPM.getRowGroup(above);
                if (g <= 0)
                {
                    g = mPM.allocateRowGroupId();
                    mPM.setRowGroup(above, g);
                    mPM.setRowGroupColor(above,
                        kGroupPalette[(g - 1) % (int) juce::numElementsInArray(kGroupPalette)]);
                }
                mPM.setRowGroup(row, g);
                mPM.setRowGroupColor(row, mPM.getRowGroupColor(above));
                mGrid.commitEdit();
                repaint();
                break;
            }
            case 6:
            {
                mGrid.beginEdit("Remove from Group");   // #23
                mPM.setRowGroup(row, 0);
                mPM.setRowGroupColor(row, 0);
                mGrid.commitEdit();
                repaint();
                break;
            }
            case 7:
            {
                const int g = mPM.getRowGroup(row);
                if (g <= 0) break;
                // #23: capture the before-state BEFORE the async picker opens;
                // commit inside the callback.  A canceled picker never commits
                // -- the pending snapshot is harmlessly overwritten by the
                // next beginEdit (no undo entry pushed).
                mGrid.beginEdit("Color Group");
                PatternColorPicker::showAsync(this, Colour(mPM.getRowGroupColor(row)),
                    [this, g](Colour c)
                    {
                        for (int r = 0; r < ArrangementGrid::kNumRows; ++r)
                            if (mPM.getRowGroup(r) == g)
                                mPM.setRowGroupColor(r, c.getARGB());
                        mGrid.commitEdit();
                        repaint();
                    });
                break;
            }
            case 8:
            {
                // QA-G (owner rev 2026-07-17, with the Split work): the
                // full-grid edge PROMPTS instead of silently no-oping.
                if (! mGrid.canInsertRows(1))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Maximum Tracks Reached",
                        "All 500 tracks are in use - cannot insert another track.");
                    break;
                }
                mGrid.beginEdit("Insert Track Above");
                mGrid.insertBlankRowsAt(row, 1);
                mGrid.commitEdit();
                notifyArrangementChanged();
                mGrid.repaint();
                repaint();
                break;
            }
            default: break;
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// ArrangementToolbar
// ─────────────────────────────────────────────────────────────────────────────
ArrangementToolbar::ArrangementToolbar()
{
    // QA-Ea Task 0c (2026-05-20): SlipEdit tool removed (8 tools now).  The
    // slip-edit mode lives in a separate Slip/Stretch dropdown button placed
    // flush after Play(Y), see mEditModeBtn below.
    static const char* kLabels[] = {
        "Draw(P)", "Paint(B)", "Select(E)", "Delete(D)",
        "Mute(T)", "Slice(C)", "Zoom(Z)", "Play(Y)"
    };
    static const ArrangementGrid::AGTool kTools[] = {
        ArrangementGrid::AGTool::Draw,   ArrangementGrid::AGTool::Paint,
        ArrangementGrid::AGTool::Select, ArrangementGrid::AGTool::Delete,
        ArrangementGrid::AGTool::Mute,
        ArrangementGrid::AGTool::Slice,  ArrangementGrid::AGTool::Zoom,
        ArrangementGrid::AGTool::PlaySelected
    };

    for (int i = 0; i < kNumTools; ++i)
    {
        mToolBtns[i] = std::make_unique<TextButton>(kLabels[i]);
        const auto tool = kTools[i];
        mToolBtns[i]->onClick = [this, tool] {
            if (onToolSelected) onToolSelected(tool);
        };
        addAndMakeVisible(*mToolBtns[i]);
    }
    mToolBtns[0]->setToggleState(true, dontSendNotification);

    // QA-Ea Task 0c (2026-05-20): Slip/Stretch dropdown.  Placed flush after
    // Play(Y) in the resized() layout (no preceding gap).  Click opens a
    // PopupMenu (Slip / Stretch); label reflects current mode + 'S' hint.
    // Default Stretch (matches today's right-edge resize UX before QA-Ec).
    mEditModeBtn = std::make_unique<TextButton>("Stretch (S) v");
    mEditModeBtn->setTooltip ("Toggle Slip/Stretch Editing -- 'S' to toggle. "
                              "Slip: drag clip edge to expose/trim pre-roll. "
                              "Stretch: drag right edge to resize (time-stretch "
                              "coming in a future update).");
    mEditModeBtn->onClick = [this] {
        juce::PopupMenu m;
        m.addItem (1, "Slip");
        m.addItem (2, "Stretch");
        m.showMenuAsync (
            juce::PopupMenu::Options().withTargetComponent (mEditModeBtn.get()),
            [this](int r) {
                if (r == 1 && onEditModeRequested) onEditModeRequested (ArrangementGrid::EditMode::Slip);
                else if (r == 2 && onEditModeRequested) onEditModeRequested (ArrangementGrid::EditMode::Stretch);
            });
    };
    addAndMakeVisible (*mEditModeBtn);

    mUndoBtn = std::make_unique<TextButton>("Undo");
    mUndoBtn->onClick = [this] { if (onUndo) onUndo(); };
    mUndoBtn->setEnabled(false);
    addAndMakeVisible(*mUndoBtn);

    mRedoBtn = std::make_unique<TextButton>("Redo");
    mRedoBtn->onClick = [this] { if (onRedo) onRedo(); };
    mRedoBtn->setEnabled(false);
    addAndMakeVisible(*mRedoBtn);

    // 2026-04-26 (D-1b): History button - opens the global undo-history list.
    mHistoryBtn = std::make_unique<TextButton>("H");
    mHistoryBtn->setTooltip("Show undo history");
    mHistoryBtn->onClick = [this] { if (onShowHistory) onShowHistory(); };
    addAndMakeVisible(*mHistoryBtn);

    mZoomInBtn  = std::make_unique<TextButton>("+");
    mZoomOutBtn = std::make_unique<TextButton>("-");
    mZoomInBtn ->onClick = [this] { if (onZoom) onZoom(1.15f); };
    mZoomOutBtn->onClick = [this] { if (onZoom) onZoom(1.f / 1.15f); };
    addAndMakeVisible(*mZoomInBtn);
    addAndMakeVisible(*mZoomOutBtn);

    // G1 boundary (2026-07-08, missed locked scope from QA-UICleanup capture):
    // the Builder's own snap control (param Unified_BuilderSnapDiv - fully
    // independent of the rolls' shared div) restyled to the magnet pattern
    // the rolls got in QA-UICleanup Task 3: click opens the 11-value menu
    // (current pick ticked), button highlight = snap active / dim = Off.
    // Replaces the old "Snap:" label + plain ComboBox; sits FIRST on the bar
    // (ahead of Draw) per Jeff's placement pick.
    mSnapBtn = std::make_unique<TextButton>("Snap");
    mSnapBtn->setToggleState(true, dontSendNotification);   // default Line = active; re-synced via setSnapDivIndex on load
    mSnapBtn->setTooltip("Snap resolution - click to choose");
    mSnapBtn->onClick = [this] {
        PopupMenu m;
        const int cur = onGetSnapDiv ? onGetSnapDiv() : 1;
        for (int i = 0; i < kNumUnifiedSnapDivs; ++i)
            m.addItem(i + 1, kUnifiedSnapLabels[i], true, i == cur);
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(mSnapBtn.get()),
            [this](int r) {
                if (r > 0) {
                    const int d = r - 1;
                    if (onSnapChanged) onSnapChanged(d);   // 0..10
                    if (mSnapBtn) mSnapBtn->setToggleState(d != 0, dontSendNotification);
                }
            });
    };
    addAndMakeVisible(*mSnapBtn);

    // ── Context label (right-aligned) ────────────────────────────────────
    mContextLabel = std::make_unique<juce::Label>();
    mContextLabel->setColour(juce::Label::textColourId, VC::TextDim);
    mContextLabel->setFont(juce::Font(11.f, juce::Font::bold));
    mContextLabel->setJustificationType(juce::Justification::centredRight);
    mContextLabel->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(*mContextLabel);
}

void ArrangementToolbar::setActiveTool(ArrangementGrid::AGTool t)
{
    static const ArrangementGrid::AGTool kTools[] = {
        ArrangementGrid::AGTool::Draw,    ArrangementGrid::AGTool::Paint,
        ArrangementGrid::AGTool::Select,  ArrangementGrid::AGTool::Delete,
        ArrangementGrid::AGTool::Mute,
        ArrangementGrid::AGTool::Slice,   ArrangementGrid::AGTool::Zoom,
        ArrangementGrid::AGTool::PlaySelected
    };
    for (int i = 0; i < kNumTools; ++i)
        mToolBtns[i]->setToggleState(kTools[i] == t, dontSendNotification);
}

void ArrangementToolbar::setContextText(const juce::String& text)
{
    if (mContextLabel) mContextLabel->setText(text, juce::dontSendNotification);
}

// QA-Ea Task 0c (2026-05-20): refresh the dropdown button label after a
// mode change.  Format mirrors the other tool buttons ("Draw(P)" / "Play(Y)")
// for visual consistency.  ASCII-only ('v' chevron substitute) per the
// ascii-only-ui-strings convention.
void ArrangementToolbar::setEditModeLabel (ArrangementGrid::EditMode m)
{
    if (! mEditModeBtn) return;
    mEditModeBtn->setButtonText (m == ArrangementGrid::EditMode::Slip
                                     ? "Slip (S) v"
                                     : "Stretch (S) v");
}

void ArrangementToolbar::paint(Graphics& g)
{
    g.fillAll(kHeaderBg);
    g.setColour(kGridLine);
    g.drawHorizontalLine(getHeight() - 1, 0.f, (float)getWidth());
}

void ArrangementToolbar::resized()
{
    auto b = getLocalBounds().reduced(2);
    // Snap magnet first (ahead of Draw) - Jeff's placement pick, G1 boundary.
    if (mSnapBtn)
    {
        mSnapBtn->setBounds(b.removeFromLeft(52).reduced(1));
        b.removeFromLeft(8);
    }
    for (auto& btn : mToolBtns)
        btn->setBounds(b.removeFromLeft(62).reduced(1));
    // QA-Ea Task 0c (2026-05-20): Slip/Stretch dropdown flush after Play(Y),
    // no preceding gap.  Width 95 px to fit "Stretch (S) v" comfortably.
    // The existing 8 px gap that separated the tool group from Undo now
    // sits between this dropdown and Undo.
    if (mEditModeBtn)
        mEditModeBtn->setBounds(b.removeFromLeft(95).reduced(1));
    b.removeFromLeft(8);
    mUndoBtn ->setBounds(b.removeFromLeft(48).reduced(1));
    mRedoBtn ->setBounds(b.removeFromLeft(48).reduced(1));
    b.removeFromLeft(2);
    mHistoryBtn->setBounds(b.removeFromLeft(28).reduced(1));   // 2026-04-26 (D-1b): 22→28 so the "H" glyph fits
    b.removeFromLeft(8);
    mZoomOutBtn->setBounds(b.removeFromLeft(22).reduced(1));
    mZoomInBtn ->setBounds(b.removeFromLeft(22).reduced(1));
    b.removeFromLeft(8);
    // Context label fills remaining right-side space.
    if (mContextLabel)
    {
        b.removeFromLeft(8);
        mContextLabel->setBounds(b.reduced(4, 1));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BuilderPage
// ─────────────────────────────────────────────────────────────────────────────
BuilderPage::BuilderPage(VibeSynthProcessor& p, PatternManager& pm)
    : mProcessor(p), mPM(pm)
{
    setWantsKeyboardFocus(true);
    mAFM.registerBasicFormats();

    // Source Picker / Browser
    mBrowser = std::make_unique<BrowserPanel>(pm, mAFM, mThumbCache);

    // QA-Fe2: draggable browser right edge (min = default 180, max = 3x).
    // Jeff, 2026-08-04: the floor is now a MAGNETIC RAMP.  Dragging into the
    // last kRampPx before the floor snaps to the floor; pushing kCollapsePx
    // past it collapses the browser to its pull-back strip.  Replaces the "<<"
    // button and the View > Toggle Browser entry, which were two click-paths to
    // one action with no drag path at all.
    mBrowserGrip = std::make_unique<BrowserEdgeGrip>();
    mBrowserGrip->getWidth    = [this] { return mBrowserWidth; };
    mBrowserGrip->isCollapsed = [this] { return mBrowser && mBrowser->isCollapsed(); };
    mBrowserGrip->setWidth = [this](int w)
    {
        constexpr int kRampPx     = 14;
        constexpr int kCollapsePx = 44;

        // Jeff 2026-08-06 (replaces the 28px click-strip): collapsed, the
        // divider itself is the handle -- drag it back out.  Ruling 2B: the
        // panel opens once the drag passes the collapse threshold and lands
        // on the magnetic default (no width tracking below it).
        if (mBrowser && mBrowser->isCollapsed())
        {
            if (w < kBrowserDefaultW - kCollapsePx) return;   // not out far enough yet
            mBrowser->setCollapsed (false);
        }
        else if (w < kBrowserDefaultW - kCollapsePx)
        {
            if (mBrowser) mBrowser->setCollapsed (true);
            return;
        }
        const int snapped = (w < kBrowserDefaultW + kRampPx) ? kBrowserDefaultW : w;
        const int clamped = juce::jlimit (kBrowserDefaultW, kBrowserDefaultW * 3, snapped);
        if (clamped != mBrowserWidth) { mBrowserWidth = clamped; resized(); }
    };
    addAndMakeVisible (*mBrowserGrip);
    // QA-H Task 8 (#20): browser click -> the grid's active drop type.
    mBrowser->onDropTypeChanged = [this] (int tab, int refIdx) {
        if (! mGrid) return;
        using K = ArrangementGrid::BrowserDropKind;
        mGrid->setActiveDropKind (tab == 1 ? K::Audio
                                : tab == 2 ? K::Automation : K::Pattern, refIdx);
    };
    mBrowser->onPatternSelected = [this](int idx) {
        mPM.setCurrentPattern(idx);
        if (mGrid) mGrid->setSelectedPatternIndex(idx);
        if (mToolbar)
        {
            const juce::String name = (idx >= 0 && idx < mPM.getNumPatterns())
                ? mPM.getPattern(idx).name : juce::String();
            mToolbar->setContextText("Playlist > " + name);
        }
    };
    // TS7 §7.2: the render now asks Per Track / Full Mix / Select Tracks first.
    mBrowser->onRenderPattern = [this](int idx) { showPatternRenderOptions(idx); };
    // (Split completion may be async behind its group prompt; the browser's
    // diff-based timer refresh picks up the new pattern list either way.)
    mBrowser->onSplitPattern  = [this](int idx) {
        if (mGrid) mGrid->splitPatternByEngine(idx);
    };
    mBrowser->onImportAudio   = [this](const String& path) {
        if (mGrid) mGrid->importAudioFile(path, 0, 0.f);
    };
    mBrowser->onArrangementChanged = [this] { notifyArrangementChanged(); };
    // QA-UndoCoverage Task 4: pattern-list ops cascade into blocks, so the
    // browser borrows the grid's slice capture/apply.
    mBrowser->onCapturePatternSlice = [this]
    { return mGrid ? mGrid->capturePatternSlice() : PatternListSnapshot{}; };
    mBrowser->onApplyPatternSlice   = [this] (const PatternListSnapshot& s)
    {
        if (mGrid) mGrid->applyPatternSlice (s);
        if (mBrowser) mBrowser->refreshPatternTab();
    };
    addAndMakeVisible(*mBrowser);

    // Grid + viewport
    mGrid = std::make_unique<ArrangementGrid>(pm, mAFM, mThumbCache);
    mGrid->onToolChanged = [this](ArrangementGrid::AGTool t) {
        if (mToolbar) mToolbar->setActiveTool(t);
    };
    mGrid->onUndoRedoStateChanged = [this] { syncToolbar(); };
    // TS7 §11.5a: a render dragged onto the grid raises the SAME route prompt as
    // the browser's "Add to Project...", then places the block where it landed.
    mGrid->onRenderFileDropped = [this] (const String& path, int row, float bar)
    {
        if (! mBrowser) return;
        mBrowser->beginAddRenderToProject (File (path),
            [this, row, bar] (int libIdx)
            {
                if (mGrid && libIdx >= 0)
                    mGrid->placeAudioLibraryEntry (libIdx, row, bar);
            });
    };
    mGrid->onImportAudioRequested = [this] { doImportAudio(); };
    mGrid->onRowHeightChanged     = [this]
    {
        if (mTrackHeader && mGridViewport)   // zoom leg of the NAV-01 sync
            mTrackHeader->setViewportYOffset (mGridViewport->getViewPositionY());
        if (mTrackHeader) mTrackHeader->repaint();
    };

    // NAV-01: header <-> grid vertical sync must be event-driven -- the timer
    // tick alone left the header rows visibly desynced until the next tick
    // after a scroll/resize/zoom.  visibleAreaChanged fires on every
    // view-position change INCLUDING the re-clamp a resize or zoom causes, so
    // the offset pushes immediately; the timer call stays as backstop.
    struct HeaderSyncViewport : Viewport
    {
        std::function<void()> onViewMoved;
        void visibleAreaChanged (const juce::Rectangle<int>&) override
        {
            if (onViewMoved) onViewMoved();
        }
    };
    {
        auto vp = std::make_unique<HeaderSyncViewport>();
        vp->onViewMoved = [this]
        {
            if (mTrackHeader && mGridViewport)
                mTrackHeader->setViewportYOffset (mGridViewport->getViewPositionY());
        };
        mGridViewport = std::move (vp);
    }
    mGridViewport->setViewedComponent(mGrid.get(), false);
    // Vertical only -- horizontal panning is the grid's virtual mBarOff,
    // driven by the external scrollbar below (one mechanism, not two).
    mGridViewport->setScrollBarsShown(true, false);
    mGridViewport->setScrollBarThickness(10);
    addAndMakeVisible(*mGridViewport);

    mGridHScroll = std::make_unique<ScrollBar>(false);
    mGridHScroll->setAutoHide(false);
    mGridHScroll->addListener(this);
    addAndMakeVisible(*mGridHScroll);

    // Track header panel (Task 10 - fixed left label column)
    mTrackHeader = std::make_unique<TrackHeaderPanel>(*mGrid, pm);
    // TS7 §7.1: the panel owns no render machinery; it just asks.
    mTrackHeader->onRenderTrackRow = [this] (int row) { renderTrackRowToWav (row); };
    addAndMakeVisible(*mTrackHeader);

    // Toolbar
    mToolbar = std::make_unique<ArrangementToolbar>();
    mToolbar->onToolSelected = [this](ArrangementGrid::AGTool t) {
        if (mGrid) mGrid->setTool(t);
    };
    mToolbar->onSnapChanged = [this](int snapDiv) {
        // QA-Ee Stage 2: write the unified param (the grid reads it live) +
        // repaint so the grid lines follow the new division immediately.
        if (mGrid && mGrid->onSnapDivChanged) mGrid->onSnapDivChanged (snapDiv);
        if (mGrid) mGrid->repaint();
    };
    // G1 boundary: the magnet menu ticks the LIVE div (same APVTS reader the
    // grid uses), so external writes (project load) can never leave a stale tick.
    mToolbar->onGetSnapDiv = [this]() -> int {
        return (mGrid && mGrid->onGetSnapDiv) ? mGrid->onGetSnapDiv() : 1;
    };
    mToolbar->onZoom = [this](float factor) {
        if (!mGrid || !mGridViewport) return;
        float vpW  = (float)jmax(1, mGridViewport->getWidth());
        float minPP = mGrid->minZoomPPBar (vpW), maxPP = mGrid->maxZoomPPBar (vpW);
        // Anchor zoom around the bar currently at viewport center so content
        // doesn't slide off-screen on each click.
        const float centreVpX = vpW * 0.5f;
        const float centreGridX = centreVpX;   // viewport X is always 0 (mBarOff pans)
        const float anchorBar = mGrid->xToBarF(centreGridX);
        mGrid->mPPBar = jlimit(minPP, maxPP, mGrid->mPPBar * factor);
        // Keep anchorBar at the same grid-local x by adjusting mBarOff.
        // QA-Ea Task 0c (Option ii): allow negative-bar viewport.
        mGrid->mBarOff = jmax(-(float) mGrid->maxRevealableNegativeBars(),
                              anchorBar - centreGridX / jmax(1.f, mGrid->mPPBar));
        mGrid->resized();
    };
    mToolbar->onUndo = [this] { if (mGrid) mGrid->undo(); };
    mToolbar->onRedo = [this] { if (mGrid) mGrid->redo(); };
    // 2026-04-26 (D-1b): History button → open the global undo-history window
    // through the same UndoContext callable used by the piano-roll history btn.
    mToolbar->onShowHistory = [this] { if (mGrid) mGrid->showHistory(); };
    // QA-Ea Task 0c (2026-05-20): Slip/Stretch dropdown wiring.  Click on
    // a menu item -> set the grid's EditMode.  Grid fires onEditModeChanged
    // -> toolbar refreshes its button label.  Initial label sync below.
    mToolbar->onEditModeRequested = [this](ArrangementGrid::EditMode m) {
        if (mGrid) mGrid->setEditMode(m);
    };
    mGrid->onEditModeChanged = [this](ArrangementGrid::EditMode m) {
        if (mToolbar) mToolbar->setEditModeLabel(m);
    };
    mToolbar->setEditModeLabel (mGrid->getEditMode());   // initial label
    addAndMakeVisible(*mToolbar);

    // T16: no in-page menu bar -- Edit / View are title-strip headings and
    // Clips folds into the window Menu (StandaloneEditor installs all three).

    // Initial context label
    {
        const int idx = mPM.getCurrentPatternIndex();
        const juce::String name = (idx >= 0 && idx < mPM.getNumPatterns())
            ? mPM.getPattern(idx).name : juce::String();
        if (mToolbar) mToolbar->setContextText("Playlist > " + name);
    }

    // Timer start is owned by parentHierarchyChanged -- see the comment there.
}

void BuilderPage::parentHierarchyChanged()
{
    // Peer-keyed suspend, matching MixerPage and the meter widgets.  Deliberately
    // SEPARATE from visibilityChanged below, which manages the key listener --
    // different concern, different trigger, and merging them would tie the key
    // routing to peer state for no reason.
    if (getPeer() != nullptr)
    {
        if (! isTimerRunning()) startTimerHz (30);   // performance-mode animations
    }
    else if (isTimerRunning())
    {
        stopTimer();
    }
}

BuilderPage::~BuilderPage()
{
    stopTimer();
    if (auto* top = getTopLevelComponent())
        top->removeKeyListener(this);
}

void BuilderPage::setUndoContext(const UndoContext& ctx)
{
    if (mGrid)    { mGrid->setUndoContext(ctx); syncToolbar(); }
    if (mBrowser) mBrowser->setUndoContext(ctx);
}

void BuilderPage::syncToolbar()
{
    if (mToolbar && mGrid) {
        mToolbar->setUndoEnabled(mGrid->canUndo());
        mToolbar->setRedoEnabled(mGrid->canRedo());
    }
}

void BuilderPage::visibilityChanged()
{
    if (auto* top = getTopLevelComponent())
    {
        if (isVisible())
            top->addKeyListener(this);
        else
            top->removeKeyListener(this);
    }
}

bool BuilderPage::keyPressed(const juce::KeyPress& key, juce::Component* /*origin*/)
{
    // Route all key events to the grid regardless of which component has focus.
    if (isVisible() && mGrid) return mGrid->keyPressed(key);
    return false;
}

void BuilderPage::paint(Graphics& g) { g.fillAll(kGridBg); }

void BuilderPage::resized()
{
    auto b = getLocalBounds();

    // Browser panel (left).  Jeff 2026-08-06: collapsed = width ZERO -- only
    // the 5px divider remains (with its chevron), draggable back out.  The
    // 28px click-strip is gone.
    int browserW = mBrowser->isCollapsed() ? 0 : mBrowserWidth;   // QA-Fe2 resizable
    mBrowser->setBounds(b.removeFromLeft(browserW));
    if (mBrowserGrip)
        mBrowserGrip->setBounds (b.removeFromLeft (5));

    // T16: the 20px menu row is gone; the toolbar starts at the top and the
    // grid gains that height.

    // Toolbar (right of browser)
    mToolbar->setBounds(b.removeFromTop(ArrangementToolbar::kHeight));

    // Bottom band: external horizontal scrollbar (spans the grid area only;
    // the strip under the label column stays page background).
    auto hScrollRow = b.removeFromBottom(mGridViewport->getScrollBarThickness());

    // Track header (fixed left label column)
    mTrackHeader->setBounds(b.removeFromLeft(ArrangementGrid::kLabelW));

    // Grid viewport (remaining area)
    mGridViewport->setBounds(b);
    if (mTrackHeader)   // resize leg of the NAV-01 sync (no-clamp resizes too)
        mTrackHeader->setViewportYOffset (mGridViewport->getViewPositionY());
    if (mGridHScroll)
        mGridHScroll->setBounds(hScrollRow.removeFromRight(b.getWidth()));
    // Grid content sized by ArrangementGrid::resized() - just trigger it
    if (mGrid) mGrid->resized();
    syncGridHScroll();
}

void BuilderPage::setPlayHead(StandalonePlayHead* ph)
{
    mPlayHead = ph;
    if (mGrid && ph)
        mGrid->onSeek = [ph](double beat) { ph->seekTo(beat); };
}

void BuilderPage::setBrowserTab(int idx)
{
    if (mBrowser) mBrowser->selectTab(juce::jlimit(0, 2, idx));
}

void BuilderPage::notifyArrangementChanged()
{
    if (! mGrid) return;
    mGrid->repaint();
    if (mGrid->onArrangementChanged) mGrid->onArrangementChanged();
    // 2026-05-05 dirty-flag wiring: every Builder grid mutation routes
    // through here, so chaining into the PatternManager hook covers them.
    mPM.notifyContentChanged();
}

void BuilderPage::timerCallback()
{
    // refresh() is now diff-based: it only rebuilds BrowserItems when the
    // underlying pattern / audio-library / automation-template content
    // actually changed. Idle ticks are a cheap hash compare, so timer-
    // driven rebuilds no longer destroy items mid-drag.
    mBrowser->refresh();

    if (mGrid && mPlayHead)
    {
        double bar;
        // Hide the Builder playhead in Pattern mode (it lives on the piano
        // roll). Push -1 so the grid paints without the cursor line.
        if (!mProcessor.isSongMode())
        {
            bar = -1.0;
        }
        else if (mPlayHead->isPlaying())
        {
            // Shift the visual playhead backward by total output latency so the
            // line hits the waveform exactly when the user hears it (FL Mixer mode).
            const double sampleRate  = mProcessor.getSampleRate();
            const double bpm         = mPlayHead->getBPM();
            const int    latSamples  = mProcessor.getTotalOutputLatency();
            const double latencyBars = (sampleRate > 0.0 && bpm > 0.0)
                ? (double) latSamples / sampleRate * bpm / 60.0 / 4.0
                : 0.0;
            bar = mPlayHead->getCurrentBeat() / 4.0 - latencyBars;
        }
        else
        {
            // Stopped: show cursor at current (seeked) position, no latency offset
            bar = mPlayHead->getCurrentBeat() / 4.0;
        }
        mGrid->setPlayheadBar(bar);

        // Advance performance mode pulse
        if (mPerfMode) {
            mGrid->mPulsePhi += 0.2f;
            if (mGrid->mPulsePhi > juce::MathConstants<float>::twoPi)
                mGrid->mPulsePhi -= juce::MathConstants<float>::twoPi;
            mGrid->repaint();
        }
    }

    // Keep track header in sync (belt-and-suspenders, in case scroll happened)
    if (mTrackHeader && mGridViewport)
        mTrackHeader->setViewportYOffset(mGridViewport->getViewPositionY());

    // Keep the horizontal scrollbar tracking the grid view (range is dynamic:
    // it re-tightens when the view returns from past-the-content territory).
    syncGridHScroll();
}

void BuilderPage::syncGridHScroll()
{
    if (!mGrid || !mGridHScroll || !mGridViewport) return;
    const float vpW      = (float) jmax(1, mGridViewport->getWidth());
    const float viewBars = vpW / jmax(1.f, mGrid->mPPBar);
    const float lo = jmin(0.f, jmin(mGrid->mBarOff,
                                    -(float) mGrid->maxRevealableNegativeBars()));
    const float hi = jmax((float) mGrid->totalVisibleBars(),
                          mGrid->mBarOff + viewBars);
    mGridHScroll->setRangeLimits(lo, hi, dontSendNotification);
    mGridHScroll->setCurrentRange(mGrid->mBarOff, viewBars, dontSendNotification);
}

void BuilderPage::scrollBarMoved(ScrollBar* sb, double newRangeStart)
{
    if (sb != mGridHScroll.get() || !mGrid) return;
    mGrid->mBarOff = (float) newRangeStart;
    mGrid->repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// BuilderPage actions (called from ≡ toolbar menu / keyboard shortcuts)
// ─────────────────────────────────────────────────────────────────────────────
void BuilderPage::doImportAudio()
{
    // Matches the Clip "+"-add entry point -- both audio imports open My
    // Samples, whose Core Library shortcut reaches the factory content.
    SampleLibrary::ensureUserSamplesDir();
    auto chooser = std::make_shared<FileChooser>(
        "Import Audio File",
        SampleLibrary::getUserSamplesDir(),
        "*.wav;*.mp3;*.aiff;*.flac;*.ogg;*.aif");

    chooser->launchAsync(
        FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
        [this, chooser](const FileChooser& fc) {
            auto f = fc.getResult();
            if (f == File()) return;
            if (mGrid) mGrid->importAudioFile(f.getFullPathName(), 0, 0.f);
        });
}

void BuilderPage::doNew()
{
    // Stub: would show unsaved-changes dialog then clear the project
}

void BuilderPage::doSave()
{
    // Stub: project save/load is Phase 5
}

void BuilderPage::doOpen()
{
    // Stub: project save/load is Phase 5
}

void BuilderPage::doExport()
{
    // Stub: export functionality is Phase 5
}

void BuilderPage::doFindNextEmptyPattern()
{
    if (mGrid) mGrid->keyPressed(KeyPress(KeyPress::F4Key));
}

void BuilderPage::doRenamePattern()
{
    if (mGrid) mGrid->keyPressed(KeyPress(KeyPress::F2Key));
}

void BuilderPage::doPerformanceModeToggle()
{
    mPerfMode = !mPerfMode;
    if (mGrid) mGrid->setPerformanceMode(mPerfMode);
}

void BuilderPage::doZoom(float factor)
{
    if (!mGrid || !mGridViewport) return;
    const float vpW = (float)jmax(1, mGridViewport->getWidth());
    const float minPP = mGrid->minZoomPPBar (vpW), maxPP = mGrid->maxZoomPPBar (vpW);
    const float centreVpX = vpW * 0.5f;
    const float centreGridX = centreVpX;   // viewport X is always 0 (mBarOff pans)
    const float anchorBar = mGrid->xToBarF(centreGridX);
    mGrid->mPPBar = jlimit(minPP, maxPP, mGrid->mPPBar * factor);
    // QA-Ea Task 0c (Option ii): allow negative-bar viewport.
    mGrid->mBarOff = jmax(-(float) mGrid->maxRevealableNegativeBars(),
                          anchorBar - centreGridX / jmax(1.f, mGrid->mPPBar));
    mGrid->resized();
    resized();
}

void BuilderPage::doNewAutomationClip()
{
    // Collect all registered APVTS parameter IDs
    StringArray paramIds;
    for (auto* p : mProcessor.getParameters())
        if (auto* rap = dynamic_cast<RangedAudioParameter*>(p))
            paramIds.add(rap->getParameterID());
    paramIds.sort(true);

    auto* aw = new AlertWindow("New Automation Clip",
                               "Select or type the parameter ID to automate:",
                               AlertWindow::NoIcon);
    if (!paramIds.isEmpty())
        aw->addComboBox("params", paramIds, "Parameter:");
    aw->addTextEditor("paramId",
                      paramIds.isEmpty() ? "" : paramIds[0],
                      "Or type ID:");
    aw->addButton("Create", 1);
    aw->addButton("Cancel", 0);

    aw->enterModalState(true,
        ModalCallbackFunction::create([this, aw, paramIds](int r) {
            if (r != 1) return;

            String paramId = aw->getTextEditorContents("paramId").trim();
            if (auto* cb = aw->getComboBoxComponent("params"))
            {
                int sel = cb->getSelectedItemIndex();
                if (sel >= 0 && sel < paramIds.size())
                    paramId = paramIds[sel];
            }
            if (paramId.isEmpty()) return;

            // Determine placement: use time selection if available, else end of arrangement
            int  row = 0;
            int  bar = 0;
            int  len = 4;
            if (mGrid)
            {
                if (mGrid->hasTimeSelection())
                {
                    bar = (int)mGrid->getTimeSelStart();
                    len = jmax(1, (int)(mGrid->getTimeSelEnd() - mGrid->getTimeSelStart()));
                }
                else
                {
                    for (int i = 0; i < mPM.getNumBlocks(); ++i)
                    {
                        const auto& b = mPM.getBlock(i);
                        bar = jmax(bar, (int) std::ceil(effectiveStartBars(b) + (double) b.lengthBars - 1e-9));
                    }
                }
            }

            // Get current normalized value for the default points (flat line = no change)
            float currentVal = 0.5f;
            if (auto* param = mProcessor.getParameters().getUnchecked(0))
            {
                // Find the parameter by ID
                for (auto* p : mProcessor.getParameters())
                {
                    if (auto* rap = dynamic_cast<juce::RangedAudioParameter*>(p))
                    {
                        if (rap->getParameterID() == paramId)
                        {
                            currentVal = rap->getValue();
                            break;
                        }
                    }
                }
            }

            if (mGrid)
            {
                mGrid->beginEdit("New Automation Clip");
                // QA-UndoCoverage Task 4: the template registration rides the
                // block's transaction (rider appended after commitEdit) so
                // undoing the clip no longer orphans a template row.
                AutomationTemplateSnapshot tplBefore;
                tplBefore.templates = mPM.getAutomationTemplatesRaw();
                ArrangementBlock b;
                b.clipType               = ClipType::Automation;
                b.trackRow               = row;
                b.startBeats             = (double) bar * 4.0;
                b.lengthBars             = len;
                b.automationLane.paramId = paramId;
                // Add start and end points at current param value (flat neutral line)
                b.automationLane.points.push_back({ 0.f, currentVal, CurveType::Linear, 0.f });
                b.automationLane.points.push_back({ 1.f, currentVal, CurveType::Linear, 0.f });
                // Register the lane in the persistent template library so it
                // stays in the Browser even if this block is later deleted.
                mPM.addAutomationTemplate(b.automationLane);
                mPM.addBlock(b);
                mGrid->commitEdit();
                if (auto* mgr = mGrid->riderManager())
                {
                    AutomationTemplateSnapshot tplAfter;
                    tplAfter.templates = mPM.getAutomationTemplatesRaw();
                    mgr->perform (new AutomationTemplateAction (
                        std::move (tplBefore), std::move (tplAfter),
                        [pb = juce::Component::SafePointer<BuilderPage> (this)]
                        (const AutomationTemplateSnapshot& s)
                        {
                            if (pb == nullptr) return;
                            pb->mPM.restoreAutomationTemplates (s.templates);
                            if (auto* bp = pb->getBrowserPanel()) bp->refreshAutomationTab();
                        }));
                }
                mGrid->clearTimeSelection();
                mGrid->resized();
                mGrid->repaint();
            }
        }), true);
}

void BuilderPage::doNavigatePage(int pageIndex)
{
    // Post a command to StandaloneEditor to switch to the appropriate tab
    // (0=Layers, 1=Bass, 2=Drums, 3=Builder)
    static const int kCmds[] = { 1, 2, 3, 4 };
    if (pageIndex >= 0 && pageIndex < 4)
        if (auto* top = getTopLevelComponent())
            top->postCommandMessage(kCmds[pageIndex]);
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-Export Task 2 -- offline render harness.
//
// Generalised out of the old pattern-only renderPatternToWav.  The processor
// clone + block loop are unchanged in spirit; what song mode adds is the
// arrangement sequence, song-only automation, arrangement audio clips (which a
// fresh processor has no editor to publish for it), and a tempo-map-aware clock.
//
// May run on a BACKGROUND thread (export / measure) or the MESSAGE thread (a
// freeze render).  There is no separate render processor since TS2 -- the loop
// drives the LIVE processor with the device suspended, which is exactly why
// the restore set and the one-render-at-a-time begin guard exist.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    // Beat position for an offline render.
    //
    // Deliberately NOT `bpm * elapsedSeconds`: with a tempo map that linear form
    // drifts from the live playhead the moment a tempo change lands, so an
    // exported file would disagree with what was auditioned.  Instead it reads
    // the SAME published timeline the live playhead derives from (TempoMap, see
    // TempoMapRead.h).
    //
    // Sample-rate trap: TempoMap's sample domain is the LIVE device rate, which
    // need not be the render rate (exporting 48k from a 44.1k session).  Beats
    // are therefore resolved through SECONDS rather than by handing the render
    // sample index straight to beatAtSample.
    // QA-ModelShell TS2: the offline transport.  Integrates beats BLOCKWISE so
    // the render clock follows both the ruler tempo map and a "global_tempo"
    // automation lane with the LIVE semantics: the lane is a live override
    // while a lane clip covers the position (the 30 Hz applicator's
    // truncate-and-append behavior -- ruler flags rule wherever no lane clip
    // is active), and the lane layer is SONG SCOPE ONLY, matching the engine
    // replay's song-mode gate.  Stepped per block like the live 30 Hz
    // override -- never integrated in one closed-form jump.
    struct OfflineHead : public juce::AudioPlayHead
    {
        OfflineHead (PatternManager& pm, double baseBpm, double renderSr,
                     bool followTempoLane)
            : mPM (pm),
              mCurBpm  (juce::jmax (1.0, baseBpm)),
              mBaseBpm (juce::jmax (1.0, baseBpm)),
              mRenderSr (renderSr),
              mFollowLane (followTempoLane)
        {
            if (mFollowLane)
                for (int bi = 0; bi < mPM.getNumBlocks(); ++bi)
                {
                    const auto& blk = mPM.getBlock (bi);
                    if (blk.clipType != ClipType::Automation)         continue;
                    if (blk.muted)                                     continue;
                    if (! mPM.isRowAudible (blk.trackRow))             continue;
                    if (blk.automationLane.paramId != "global_tempo")  continue;
                    if (blk.automationLane.points.empty())             continue;
                    mTempoLanes.push_back (&blk);
                }
        }

        double bpmAtCurrentBeat() const
        {
            const double bar = mBeatPos / 4.0;
            for (const auto* blk : mTempoLanes)
            {
                const double clipStart = effectiveStartBars (*blk);
                const double len       = effectiveLengthBars (*blk);
                if (len <= 0.0 || bar < clipStart || bar >= clipStart + len) continue;
                const float rel = juce::jlimit (0.f, 1.f, (float) ((bar - clipStart) / len));
                const float v01 = evalAutomationPointsAt (blk->automationLane.points, rel);
                // The live applicator's linear 20..300 BPM map.
                return juce::jlimit (20.0, 300.0, 20.0 + (double) v01 * 280.0);
            }
            if (TempoMap::isActive())
                return TempoMap::bpmAtSample (TempoMap::sampleAtBeat (mBeatPos));
            return mBaseBpm;
        }

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo pi;
            pi.setBpm (mCurBpm);
            pi.setPpqPosition (mBeatPos);
            pi.setIsPlaying (true);
            pi.setIsRecording (false);
            pi.setTimeInSeconds (mTimeSec);
            // TS7 (2026-07-30): timeInSamples was NEVER published here, and an
            // ALREADY-FROZEN track reads exactly this field to know where in its
            // freeze file to read.  Missing, it fell back to 0 on every block, so
            // every frozen track replayed its first blockful on a loop for the
            // whole render -- corrupting exports and measurements, and getting
            // worse with each track frozen.  Silent: the file was valid, just
            // wrong.  Matters more for the planned per-pattern freeze, where one
            // freeze action means several renders back to back.
            pi.setTimeInSamples ((juce::int64) std::llround (mSamplePos));
            return pi;
        }

        void advance (int n) noexcept
        {
            mCurBpm    = bpmAtCurrentBeat();
            mBeatPos  += (double) n / mRenderSr * (mCurBpm / 60.0);
            mTimeSec  += (double) n / mRenderSr;
            mSamplePos += n;
        }

        // Stepped fast-forward to `targetBeats`; returns the samples consumed.
        // The final partial step is linearly corrected at that step's BPM, so
        // span math and the render walk the same clock.
        juce::int64 advanceToBeat (double targetBeats, int stepSamples) noexcept
        {
            juce::int64 samples = 0;
            while (mBeatPos < targetBeats)
            {
                const double bpm          = bpmAtCurrentBeat();
                const double beatsPerStep = (double) stepSamples / mRenderSr * (bpm / 60.0);
                if (beatsPerStep <= 0.0) break;
                if (mBeatPos + beatsPerStep >= targetBeats)
                {
                    const double frac = (targetBeats - mBeatPos) / beatsPerStep;
                    const int    part = juce::jmax (1, (int) std::ceil (frac * (double) stepSamples));
                    advance (part);
                    samples += part;
                    break;
                }
                advance (stepSamples);
                samples += stepSamples;
            }
            return samples;
        }

        PatternManager& mPM;
        std::vector<const ArrangementBlock*> mTempoLanes;
        juce::int64 mSamplePos { 0 };
        double      mBeatPos   { 0.0 };
        double      mTimeSec   { 0.0 };
        double      mCurBpm;
        double      mBaseBpm;
        double      mRenderSr;
        bool        mFollowLane;
    };
}

// QA-ModelShell TS2: the ONE offline render loop.  Everything position- and
// lifecycle-related lives here -- span/scope math, the offline drive
// (begin/endOfflineRender + the full restore set), the lane-aware clock, the
// per-block lane replay, tail-decay handling.  Consumers differ only in what
// they do with each rendered block (write files / feed meters / tap a strip).
bool BuilderPage::runOfflineLoop (const RenderOptions& opts,
                                  juce::String& outErr,
                                  std::function<bool()> shouldAbort,
                                  std::function<void(double)> onProgress,
                                  const std::function<bool (const juce::AudioBuffer<float>&, int)>& consumeBlock)
{
    using Scope = RenderOptions::Scope;

    // QA-UndoCoverage: the whole offline loop is one programmatic-write phase.
    // The message thread blocks for the duration, so every lane replay
    // (applyOfflineAutomationAt + processBlock's internal pass), restore-set
    // write, and normalize write is marked before any flush can run -- an
    // export leaves the undo history byte-identical.
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;

    const double sr       = opts.sampleRate > 0.0 ? opts.sampleRate : 44100.0;
    // CL-056: offline block size.  2048 renders markedly faster than the live
    // 512 while keeping the lane-replay step (~46 ms at 44.1k) in the same
    // class as the live 30 Hz applicator tick, so automation granularity in
    // the file matches what live playback produces.
    constexpr int kBlk    = kOfflineBlock;   // CL-056; one home, see the header
    const double baseBpm  = juce::jmax (1.0, mPM.getGlobalTempo());

    // ── Content span, in beats ───────────────────────────────────────────────
    double startBeats = 0.0;
    double endBeats   = 0.0;

    if (opts.scope == Scope::Song)
    {
        // Shared with the transport's loop end so export and playback agree.
        // NOT getTotalArrangementBars(), which floors to a 16-bar grid.
        endBeats = mPM.getSongEndBeats();
        if (endBeats <= 0.0)
        {
            outErr = "The arrangement is empty - nothing to export.";
            return false;
        }
    }
    else if (opts.scope == Scope::Section)
    {
        startBeats = opts.startBeats;
        endBeats   = opts.endBeats;
        if (endBeats <= startBeats)
        {
            outErr = "No section is selected on the Builder ruler.";
            return false;
        }
    }
    else
    {
        if (opts.patternIndex < 0 || opts.patternIndex >= mPM.getNumPatterns())
        {
            outErr = "That pattern no longer exists.";
            return false;
        }
        // Pattern.bars is DEAD DATA for length and must never be used here.
        // Nothing writes it from content -- only deserialize and clone touch it
        // (PatternManager.cpp:1796, BuilderPage.cpp:4028) -- so it sits at
        // DEFAULT_BARS = 4 forever, and `* 4.0` additionally assumes 4/4.  An
        // 8-bar pattern rendered 4 bars and dropped every note past the clamp,
        // which is the SECOND half of the bug that produced "one note then
        // silence": fixing the loop-wrap alone just moved the truncation from
        // bar 1 to bar 4.  getPatternContentBeats is the note-for-note extent at
        // the pattern's own time signature, and is what the tiler, the song
        // scheduler and live pattern playback all already use.
        endBeats = mPM.getPatternContentBeats (opts.patternIndex);
    }

    // Spans resolve through the SAME lane-aware clock the render walks -- a
    // tempo lane changes real-time length, so closed-form map-only seconds
    // math would mislabel the span.  The head fast-forwards to the section
    // start here; the render loop continues the same clock.
    const bool followLane = (opts.scope != Scope::Pattern);
    OfflineHead head (mPM, baseBpm, sr, followLane);

    juce::int64 contentSamples = 0;
    {
        OfflineHead probe (mPM, baseBpm, sr, followLane);
        const juce::int64 endSample = probe.advanceToBeat (endBeats, kBlk);
        const juce::int64 startSmp  = (startBeats > 0.0)
                                    ? head.advanceToBeat (startBeats, kBlk) : 0;
        contentSamples = endSample - startSmp;
    }

    if (contentSamples <= 0)
    {
        outErr = "Computed a zero-length render.";
        return false;
    }

    // Tail::Included keeps going past the content until it decays; the loop
    // below decides when.  Cap is a ceiling, not a length.
    const juce::int64 maxTailSamples = (opts.tail == RenderOptions::Tail::Included)
                                     ? (juce::int64) (kMaxTailSeconds * sr) : 0;

    // ── Offline drive ────────────────────────────────────────────────────────
    // The LIVE processor renders itself: same engines, same graph, same racks
    // as playback.  The old replica VibeSynthProcessor here had no pages and
    // therefore no instrument engines or strips -- vox/inst/instrument
    // exports rendered SILENT (the batch's origin finding).
    if (onOfflineRenderActive) onOfflineRenderActive (true);

    // Pattern-scope exports used to mutate the LIVE current pattern with no
    // restore; part of the locked restore set now.
    const int prevPatternIndex = mPM.getCurrentPatternIndex();

    // Stopwatch split (see renderFreezeFile): enter/leave offline mode is FIXED
    // cost paid regardless of render length, so it must be timed separately from
    // the per-block loop or a short render reads as catastrophically slow.
    const double offlineT0 = juce::Time::getMillisecondCounterHiRes();
    if (! mProcessor.beginOfflineRender (sr, kBlk))
    {
        if (onOfflineRenderActive) onOfflineRenderActive (false);
        outErr = "Could not enter offline render mode.";
        return false;
    }
    mFreezeSetupMs = juce::Time::getMillisecondCounterHiRes() - offlineT0;

    // TS7 §7.3 FIX -- the pattern render produced one note then silence, in a file
    // shorter than the pattern.  ONE root cause behind both symptoms.
    //
    // Pattern-mode scheduling bounds its window with mLoopStartBeats /
    // mCachedPatternLoopBeats (PluginProcessor.cpp:2282-2283) and clamps every
    // note-off to that loop end.  Those atomics are written ONLY by
    // StandaloneEditor's transport, so an offline render inherited whatever the
    // live session last set -- 4.0 (one bar) by default.  Notes past that stale
    // bound never fired, because the offline head advances MONOTONICALLY and never
    // performs the loop wrap the live playhead does.  The render then went silent,
    // and Tail::Included's decay detection ended the file early -- which is why it
    // was also short.  Setting the bounds to the pattern's own span fixes both.
    const double prevLoopStart = mProcessor.mLoopStartBeats.load (std::memory_order_relaxed);
    const double prevLoopEnd   = mProcessor.mCachedPatternLoopBeats.load (std::memory_order_relaxed);

    if (opts.scope == Scope::Pattern)
    {
        mPM.setCurrentPattern (opts.patternIndex);
        mProcessor.setSongMode (false);
        mProcessor.mLoopStartBeats.store (0.0, std::memory_order_relaxed);
        mProcessor.mCachedPatternLoopBeats.store (endBeats, std::memory_order_relaxed);
    }
    else
    {
        mProcessor.setSongMode (true);
    }

    // The head was fast-forwarded to the section start by the span math above
    // (same clock, same stepping), so section exports open at the selection.
    mProcessor.setPlayHead (&head);

    // ── Block loop ───────────────────────────────────────────────────────────
    constexpr int kNumCh = 2;
    juce::AudioBuffer<float> buf (kNumCh, kBlk);
    juce::MidiBuffer         midi;
    juce::int64              written  = 0;
    double lastProgressMs = 0.0;   // progress throttle, see the onProgress call
    juce::int64              tailDone = 0;
    bool                     aborted  = false;

    // Near-silence, and how long it must hold before the tail is called dead.
    // -100 dBFS sits below the noise floor of any real 24-bit content, and a
    // quarter-second window stops a decaying reverb being cut at a zero
    // crossing between peaks.
    constexpr float kSilenceMag      = 1.0e-5f;
    const juce::int64 kQuietRunNeeded = (juce::int64) (0.25 * sr);
    juce::int64 quietRun = 0;

    for (;;)
    {
        if (shouldAbort && shouldAbort()) { aborted = true; break; }

        const bool inContent = written < contentSamples;
        if (! inContent)
        {
            if (maxTailSamples <= 0)          break;   // Tail::Cut
            if (tailDone >= maxTailSamples)   break;   // safety ceiling reached
        }

        const int chunk = inContent
                        ? (int) juce::jmin ((juce::int64) kBlk, contentSamples - written)
                        : (int) juce::jmin ((juce::int64) kBlk, maxTailSamples - tailDone);

        buf.setSize (kNumCh, chunk, false, false, true);
        buf.clear();
        midi.clear();

        // Apply every non-main-APVTS lane class at this block position
        // (engine / rack / legacy fader).  Main-APVTS lanes replay INSIDE
        // processBlock exactly as in live playback, and the tempo lane
        // drives the clock itself.  Song scope only -- automation clips are
        // song-grid data (same gate as the live replay).
        if (followLane)
            applyOfflineAutomationAt (head.mBeatPos);

        mProcessor.processBlock (buf, midi);
        head.advance (chunk);

        if (! consumeBlock (buf, chunk))
        {
            if (outErr.isEmpty()) outErr = "Writing the export file(s) failed.";
            aborted = true;
            break;
        }

        if (inContent)
        {
            written += chunk;
            // THROTTLED to ~10 Hz (2026-07-30).  Reporting every block was not a
            // cheap notification: the overlay's progress setter forces a
            // synchronous full-window SOFTWARE repaint of the whole editor (JUCE
            // 8's Direct2D context implements performAnyPendingRepaintsNow as an
            // empty function, so the overlay deliberately promotes to the
            // software renderer to make mid-freeze painting work at all).  At
            // 2048-sample blocks that was a full-screen raster every ~46 ms,
            // costing MORE than the block's realtime budget -- so the progress
            // bar I added an hour earlier was itself most of the 1.17x "render is
            // slow" measurement.  The instrument became the cost it was measuring.
            if (onProgress)
            {
                const double nowMs = juce::Time::getMillisecondCounterHiRes();
                if (nowMs - lastProgressMs >= 100.0)
                {
                    lastProgressMs = nowMs;
                    onProgress (0.9 * (double) written / (double) contentSamples);
                }
            }
        }
        else
        {
            tailDone += chunk;

            // Stop once the output has been at near-silence long enough.  Only
            // evaluated during the tail: a song with a quiet intro or a gap
            // between sections must not end the render early.
            if (buf.getMagnitude (0, chunk) < kSilenceMag) quietRun += chunk;
            else                                           quietRun  = 0;

            if (quietRun >= kQuietRunNeeded) break;

            if (onProgress)
            {
                const double nowMs = juce::Time::getMillisecondCounterHiRes();
                if (nowMs - lastProgressMs >= 100.0)
                {
                    lastProgressMs = nowMs;
                    onProgress (0.9 + 0.1 * (double) tailDone / (double) maxTailSamples);
                }
            }
        }
    }

    // Restore the render's OWN transport mutations FIRST, while the device is
    // still suspended: endOfflineRender is what resumes callbacks, and a
    // resumed block could otherwise play against the render's pattern and loop
    // bounds in the gap.  §7.3: the loop bounds are LIVE transport state, so
    // leaving a render's values behind would silently re-loop the user's
    // session over the pattern they just exported.
    if (opts.scope == Scope::Pattern)
        mPM.setCurrentPattern (prevPatternIndex);
    mProcessor.mLoopStartBeats       .store (prevLoopStart, std::memory_order_relaxed);
    mProcessor.mCachedPatternLoopBeats.store (prevLoopEnd,  std::memory_order_relaxed);

    // Leave offline mode on EVERY exit path: restore-set back (device config,
    // playhead, song mode), tails cleared, device resumed.
    const double teardownT0 = juce::Time::getMillisecondCounterHiRes();
    mProcessor.endOfflineRender();
    mFreezeTeardownMs = juce::Time::getMillisecondCounterHiRes() - teardownT0;
    if (onOfflineRenderActive) onOfflineRenderActive (false);

    if (aborted)
    {
        if (outErr.isEmpty()) outErr = "Export cancelled.";
        return false;
    }
    return true;
}

bool BuilderPage::renderToFile (const RenderOptions& opts,
                                juce::String& outErr,
                                std::function<bool()> shouldAbort,
                                std::function<void(double)> onProgress)
{
    if (opts.destination == juce::File())
    {
        outErr = "No destination file was chosen.";
        return false;
    }

    const double sr = opts.sampleRate > 0.0 ? opts.sampleRate : 44100.0;

    // ── Writers (opened BEFORE the offline drive so file failures never
    //    touch the live processor).  One sink per output file: the main mix
    //    plus one per ticked stem strip, all fed by the SAME render pass. ──
    struct FileSink
    {
        juce::File dest;
        Mp3Writer  mp3;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        juce::AudioBuffer<float> scratch;
        juce::Random rng;
        bool  isMp3   { false };
        // CL-043 + CL-045: per-sink post-gain (uniform across main + stems so
        // the stem sum still matches the mix) and SELECTABLE dither at the
        // 16-bit WAV boundary.
        float gainLin { 1.0f };
        RenderOptions::Dither dither { RenderOptions::Dither::Off };
        // Noise-shaping error feedback, per sink and per channel: the
        // quantisation error of the previous two samples, fed back so the
        // shaped noise sits where the ear is least sensitive.
        float shapeErr[2][2] { { 0.0f, 0.0f }, { 0.0f, 0.0f } };

        bool open (const RenderOptions& o, double sampleRate, const juce::File& d,
                   juce::String& err)
        {
            dest    = d;
            isMp3   = (o.format == RenderOptions::Format::Mp3);
            gainLin = juce::Decibels::decibelsToGain (o.postGainDb);
            dither  = (o.format == RenderOptions::Format::Wav && o.bitDepth == 16)
                        ? o.dither : RenderOptions::Dither::Off;
            dest.deleteFile();
            if (isMp3)
                return mp3.open (dest, sampleRate, 2, o.mp3Kbps, err);

            std::unique_ptr<juce::AudioFormat> fmt;
            if (o.format == RenderOptions::Format::Ogg) fmt = std::make_unique<juce::OggVorbisAudioFormat>();
            else                                        fmt = std::make_unique<juce::WavAudioFormat>();

            auto os = dest.createOutputStream();
            if (os == nullptr)
            {
                err = "Could not write to " + dest.getFullPathName();
                return false;
            }
            const int bits = (o.format == RenderOptions::Format::Ogg) ? o.oggQuality : o.bitDepth;
            writer.reset (fmt->createWriterFor (os.release(), sampleRate, 2u, bits, {}, 0));
            if (writer == nullptr)
            {
                err = "Could not create the audio writer for these settings.";
                return false;
            }
            return true;
        }
        bool write (const juce::AudioBuffer<float>& b, int n)
        {
            const juce::AudioBuffer<float>* src = &b;
            if (gainLin != 1.0f || dither != RenderOptions::Dither::Off)
            {
                scratch.setSize (2, n, false, false, true);
                for (int c = 0; c < 2; ++c)
                    scratch.copyFrom (c, 0, b, juce::jmin (c, b.getNumChannels() - 1), 0, n);
                if (gainLin != 1.0f)
                    scratch.applyGain (gainLin);

                // DOMAIN REFERENCE (Rule 6 cat. 3).  Both options are TPDF at
                // +-1 LSB of the 16-bit grid -- the difference of two
                // independent uniforms gives the triangular pdf, which
                // decorrelates the quantisation error from the signal.  Flat
                // stops there.  Noise-shaped additionally feeds the previous
                // two samples' quantisation error forward with a second-order
                // highpass response, moving the (same total) noise power up
                // out of the ear's most sensitive band around 3-4 kHz.
                // Coefficients 1.0 / -0.5 are the textbook minimal shaper:
                // audibly quieter than flat, and unconditionally stable, which
                // aggressive psychoacoustic curves are not at every rate.
                if (dither != RenderOptions::Dither::Off)
                {
                    constexpr float kLsb   = 1.0f / 32768.0f;
                    constexpr float kQuant = 32768.0f;
                    const bool shaped = (dither == RenderOptions::Dither::NoiseShaped);

                    for (int c = 0; c < 2; ++c)
                    {
                        float* p = scratch.getWritePointer (c);
                        for (int i = 0; i < n; ++i)
                        {
                            const float tpdf = (rng.nextFloat() - rng.nextFloat()) * kLsb;

                            if (! shaped) { p[i] += tpdf; continue; }

                            const float fed = p[i]
                                            + shapeErr[c][0] * 1.0f
                                            - shapeErr[c][1] * 0.5f;
                            const float out = fed + tpdf;
                            // The error THIS sample will incur once the writer
                            // rounds to the 16-bit grid, measured here so the
                            // next samples can correct for it.
                            const float quantised = std::round (out * kQuant) / kQuant;
                            shapeErr[c][1] = shapeErr[c][0];
                            shapeErr[c][0] = quantised - fed;
                            p[i] = out;
                        }
                    }
                }
                src = &scratch;
            }
            if (isMp3) return mp3.write (src->getArrayOfReadPointers(), n);
            return writer->writeFromAudioSampleBuffer (*src, 0, n);
        }
        void close()
        {
            writer.reset();
            if (isMp3) mp3.close();
        }
    };

    std::vector<std::unique_ptr<FileSink>> sinks;   // [0] = the main mix, when written
    // §7.2 Per Track writes stems ONLY, so the main sink is not opened at all --
    // creating and then deleting an unwanted file would leave a window where it
    // exists on disk.  stemSinks record their index at push time, so the numbering
    // stays correct with or without [0]; the consumeBlock below only touches
    // sinks[0] under the same writeMainFile flag.
    if (opts.writeMainFile)
    {
        auto main = std::make_unique<FileSink>();
        if (! main->open (opts, sr, opts.destination, outErr))
            return false;
        sinks.push_back (std::move (main));
    }

    // Stem files: "<destBase> - <stripName>.<ext>" beside the main file
    // (inside the project's Exports folder like everything else).
    struct StemSink { int channelId; size_t sinkIdx; };
    std::vector<StemSink> stemSinks;
    for (const auto& st : opts.stems)
    {
        if (st.channelId < 0) continue;
        const juce::String safe = juce::File::createLegalFileName (
            st.name.isNotEmpty() ? st.name : ("Strip " + juce::String (st.channelId)));
        const juce::File d = opts.destination.getSiblingFile (
            opts.destination.getFileNameWithoutExtension() + " - " + safe
            + opts.destination.getFileExtension());

        auto s = std::make_unique<FileSink>();
        if (! s->open (opts, sr, d, outErr))
        {
            for (auto& x : sinks) { x->close(); x->dest.deleteFile(); }
            return false;
        }
        stemSinks.push_back ({ st.channelId, sinks.size() });
        sinks.push_back (std::move (s));
    }

    // §7.1/§7.2: scratch for the summed-tap main file.  Allocated ONCE here, not
    // per block -- the render thread has no real-time deadline but a per-block
    // allocation in the inner loop is still waste.
    juce::AudioBuffer<float> mixScratch;
    if (! opts.mixTapChannels.empty())
        mixScratch.setSize (2, kOfflineBlock, false, true, true);

    const bool ok = runOfflineLoop (opts, outErr, std::move (shouldAbort),
                                    std::move (onProgress),
        [this, &opts, &sinks, &stemSinks, &mixScratch]
        (const juce::AudioBuffer<float>& buf, int chunk) -> bool
        {
            if (opts.writeMainFile)
            {
                if (opts.mixTapChannels.empty())
                {
                    // Master output -- the original behaviour.
                    if (! sinks[0]->write (buf, chunk))
                        return false;
                }
                else
                {
                    // Sum the requested strips' post-chain taps.  One entry is a
                    // single consolidated track; several is Full Mix.
                    mixScratch.clear (0, chunk);
                    for (int chId : opts.mixTapChannels)
                    {
                        auto* src = mProcessor.getStripOutputForTap (chId);
                        if (src == nullptr) continue;
                        const int nch = juce::jmin (2, src->getNumChannels());
                        for (int c = 0; c < nch; ++c)
                            mixScratch.addFrom (c, 0, *src, c, 0, chunk);
                    }
                    if (! sinks[0]->write (mixScratch, chunk))
                        return false;
                }
            }

            // Stems: copy each ticked strip's arena slot -- its render task's
            // post-chain output for THIS block -- into that stem's file.
            // Same single pass, so sends are separate stems and sidechain-
            // driven content (a bass comp keyed by the kick) stays in the stem.
            for (const auto& ss : stemSinks)
            {
                auto* src = mProcessor.getStripOutputForTap (ss.channelId);
                if (src == nullptr) continue;
                if (! sinks[ss.sinkIdx]->write (*src, chunk))
                    return false;
            }
            return true;
        });

    // Release files before any delete: on Windows an open handle blocks it.
    for (auto& s : sinks) s->close();

    if (! ok)
    {
        for (auto& s : sinks) s->dest.deleteFile();
        return false;
    }
    return true;
}

// TS7 §6.1: the freeze render.  runOfflineLoop's THIRD consumer -- the loop is
// never copied, which is the rule TS2 established when it extracted it.
//
// Writes 24-bit WAV at the project rate.  Float would avoid any quantisation of
// an intermediate, but a freeze file is also the thing the user can drag out and
// keep, and 24-bit is half the disk for a difference nothing downstream can hear
// after the rack it still passes through.
bool BuilderPage::renderFreezeFile (VibeGraph::InsertKind kind, int index,
                                    RenderTask* target,
                                    int patternIndex,
                                    const juce::File& dest,
                                    juce::String& outErr,
                                    std::function<bool()> shouldAbort,
                                    std::function<void(double)> onProgress)
{
    RenderOptions opts;
    // §6.8: pattern scope renders the loop itself, so the file's length IS the
    // pattern's and reading it at a loop-local position lines up exactly.
    opts.scope        = patternIndex >= 0 ? RenderOptions::Scope::Pattern
                                          : RenderOptions::Scope::Song;
    opts.patternIndex = patternIndex;
    // Tail::Cut, deliberately.  A freeze must be sample-aligned with the
    // arrangement it stands in for; rendering PAST the song end would make the
    // frozen file longer than the timeline and shift nothing but confuse the
    // swap.  Wet tails inside the song still render, because the engine keeps
    // producing them up to the end.
    opts.tail        = RenderOptions::Tail::Cut;
    opts.format      = RenderOptions::Format::Wav;
    opts.sampleRate  = mProcessor.getSampleRate() > 0.0 ? mProcessor.getSampleRate() : 44100.0;
    opts.bitDepth    = 24;
    opts.destination = dest;

    auto& graph = mProcessor.mVibeGraph;
    // ── TS7: the stopwatch (Jeff, 2026-07-30) ────────────────────────────────
    // How long a freeze render actually takes had NEVER been measured -- every
    // estimate in the no-dropout review was an assumption, and three of the five
    // candidate fixes are only worth doing if the render is genuinely slow.
    // Reported as wall-clock AND as a ratio of the audio rendered, because the
    // ratio is the number that decides it: >1.0x means slower than just playing
    // the song, which would rule out several routes outright.
    const double freezeT0 = juce::Time::getMillisecondCounterHiRes();

    graph.armFreezeTap (kind, index);

    // TS7 §6.9 PRUNE.  A freeze render of one track was rendering the ENTIRE
    // project every block -- every other engine, every rack, every clip stream --
    // and then throwing all of it away.  The dispatcher now skips run() on
    // everything the target does not depend on.  Armed and cleared on exactly
    // the same lines as the tap, and NEVER inside runOfflineLoop: leaving it set
    // when real-time playback resumes would silence the whole project except
    // this one track.
    mProcessor.setFreezePrune (target);

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatWriter> writer;
    {
        dest.getParentDirectory().createDirectory();
        dest.deleteFile();                       // §6.7: overwrite in place
        auto stream = std::make_unique<juce::FileOutputStream> (dest);
        if (! stream->openedOk())
        {
            graph.disarmFreezeTap();
            mProcessor.setFreezePrune (nullptr);
            outErr = "Could not open the freeze file for writing.";
            return false;
        }
        juce::WavAudioFormat wav;
        writer.reset (wav.createWriterFor (stream.release(), opts.sampleRate, 2, 24, {}, 0));
        if (writer == nullptr)
        {
            graph.disarmFreezeTap();
            mProcessor.setFreezePrune (nullptr);
            outErr = "Could not create the freeze writer.";
            return false;
        }
    }

    juce::int64  writtenSamples = 0;
    juce::uint32 lastTapSeq     = graph.getFreezeTapSeq();
    juce::int64  blocksWithTap  = 0;

    const bool ok = runOfflineLoop (opts, outErr, std::move (shouldAbort),
                                    std::move (onProgress),
        [&graph, &writer, &writtenSamples, &lastTapSeq, &blocksWithTap]
        (const juce::AudioBuffer<float>&, int chunk) -> bool
        {
            // The MASTER buffer the loop hands us is ignored on purpose: freeze
            // wants this insert's PRE-RACK signal, which the armed tap holds.
            auto* src = graph.getFreezeTapBuffer();
            if (src == nullptr) return false;

            // STALE-TAP GUARD.  The tapped node's processBlock does NOT run every
            // block -- a Clips row with a gap between clips skips it, an
            // idle-suspended engine skips it -- and the tap buffer then still
            // holds the PREVIOUS block's audio.  Writing that repeated the last
            // block into the gap: a stutter baked into the freeze file, valid
            // WAV, wrong sound, and silent.  A sequence number is the only way to
            // tell a fresh capture from leftovers, since silence is legitimate
            // content and cannot be detected by looking at the samples.
            const juce::uint32 seq = graph.getFreezeTapSeq();
            if (seq != lastTapSeq) { lastTapSeq = seq; ++blocksWithTap; }
            else                   { src->clear(); }

            if (! writer->writeFromAudioSampleBuffer (*src, 0, chunk)) return false;
            writtenSamples += chunk;
            return true;
        });

    graph.disarmFreezeTap();
    mProcessor.setFreezePrune (nullptr);
    writer.reset();          // close before any delete: an open handle blocks it

    if (! ok)
    {
        dest.deleteFile();
        return false;
    }

    // NEVER SILENTLY SHIP A FILE THE TAP NEVER FILLED.  If the tap never fired,
    // the render "succeeded" and produced a perfectly valid file of pure silence
    // -- which then plays in place of the track, and the user hears a part
    // vanish with nothing to explain it.  Failing loudly is the only safe
    // outcome, because silence is indistinguishable from a quiet part.
    if (blocksWithTap == 0)
    {
        dest.deleteFile();
        outErr = "The freeze render captured no audio for this track.";
        return false;
    }

    // The measurement.  Ratio is the decisive number: the audio is silent for
    // this whole span, so anything near or above 1.0x means freezing costs about
    // as long as listening to the part being frozen.
    {
        const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - freezeT0;
        const double audioSecs = (writtenSamples > 0 && opts.sampleRate > 0.0)
                                   ? (double) writtenSamples / opts.sampleRate : 0.0;
        const double ratio     = (audioSecs > 0.0)
                                   ? (elapsedMs / 1000.0) / audioSecs : 0.0;
        // FIXED vs MARGINAL is the whole question (2026-07-30).  The first
        // measurement was 1.7 s of audio in 4.18 s -- 2.4x realtime -- but on a
        // render that short, entering and leaving offline mode (a full
        // prepareToPlay of every engine, incl. NAM's oversampling rebuild and
        // model prewarm, then the same again in reverse) plausibly IS the whole
        // figure.  If the cost is fixed, a 3-minute song costs about the same
        // 4 s and the fix is to stop re-preparing; if it is per-sample, that song
        // costs 7 minutes and only a background render helps.  Opposite
        // conclusions from one ratio, so the split is what makes it decidable.
        const double setupMs    = mFreezeSetupMs;
        const double teardownMs = mFreezeTeardownMs;
        const double loopMs     = juce::jmax (0.0, elapsedMs - setupMs - teardownMs);
        const double loopRatio  = (audioSecs > 0.0) ? (loopMs / 1000.0) / audioSecs : 0.0;
        juce::String line;
        line << juce::Time::getCurrentTime().toString (true, true, false)
             << "  [TS7 FREEZE] " << juce::String (audioSecs, 2) << "s audio | total "
             << juce::String (elapsedMs, 0) << "ms = setup "
             << juce::String (setupMs, 0) << "ms + loop "
             << juce::String (loopMs, 0) << "ms + teardown "
             << juce::String (teardownMs, 0) << "ms | LOOP ONLY "
             << juce::String (loopRatio, 4) << "x realtime -> " << dest.getFileName();

        DBG (line);
        // TO A FILE, not only DBG: DBG compiles to NOTHING in Release, so the
        // Debug figure was the only one obtainable -- and Debug can be several
        // times slower than Release on DSP code, which is exactly the variable
        // that decides whether the loop is genuinely too slow or just unoptimised.
        // A measurement you cannot take in the build that ships is not a
        // measurement.  Appended so successive runs accumulate and can be
        // compared; lands beside the app per the existing artifact convention.
        AppPaths::appRoot().getChildFile ("freeze_timing.txt")
            .appendText (line + juce::newLine);
    }
    return true;
}

// ── TS7 §6.9: the kit's strips, all in ONE offline pass ──────────────────────
// Does NOT use the freeze tap.  The tap is single-arm -- one insert at a time --
// so capturing thirteen strips through it would mean thirteen full renders.  The
// kit engine already exposes each strip's audio directly at exactly the point
// the tap would have copied it (before that strip's insert chain), and it is
// valid between processBlock returning and the next call, which is precisely
// when consumeBlock runs.  So one pass fills all thirteen writers.
bool BuilderPage::renderKitFreezeFiles (const std::vector<juce::File>& dests,
                                        RenderTask* target,
                                        int patternIndex,
                                        juce::String& outErr,
                                        std::function<bool()> shouldAbort,
                                        std::function<void(double)> onProgress)
{
    auto* kit = mProcessor.getBaySickRustyDrums();
    if (kit == nullptr) { outErr = "No drum kit is loaded."; return false; }

    const int n = (int) dests.size();
    if (n <= 0) { outErr = "The drum kit has no pieces to freeze."; return false; }

    RenderOptions opts;
    // §6.8: the kit is one instrument and gets per-pattern renders like the rest.
    opts.scope        = patternIndex >= 0 ? RenderOptions::Scope::Pattern
                                          : RenderOptions::Scope::Song;
    opts.patternIndex = patternIndex;
    opts.tail         = RenderOptions::Tail::Cut;
    opts.sampleRate   = mProcessor.getSampleRate();

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::WavAudioFormat wav;

    std::vector<std::unique_ptr<juce::AudioFormatWriter>> writers;
    writers.reserve ((size_t) n);
    for (const auto& dest : dests)
    {
        dest.getParentDirectory().createDirectory();
        dest.deleteFile();
        auto stream = std::make_unique<juce::FileOutputStream> (dest);
        if (! stream->openedOk())
        {
            outErr = "Could not open a kit freeze file for writing.";
            return false;
        }
        writers.emplace_back (wav.createWriterFor (stream.release(), opts.sampleRate,
                                                   2, 24, {}, 0));
        if (writers.back() == nullptr)
        {
            outErr = "Could not create a kit freeze file writer.";
            return false;
        }
    }

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    juce::int64  writtenSamples = 0;
    juce::uint32 lastRenderSeq  = kit->getStripRenderSeq();
    juce::int64  blocksRendered = 0;

    // Silence to write on a block where the engine did not render.  Allocated
    // once, outside the loop: this runs per block.
    juce::AudioBuffer<float> silence (2, 1024);
    silence.clear();

    // Armed AFTER the writer setup above, so none of its early returns can leave
    // the prune set.  See renderFreezeFile for why leaking it is dangerous.
    mProcessor.setFreezePrune (target);

    const bool ok = runOfflineLoop (opts, outErr, std::move (shouldAbort),
                                    std::move (onProgress),
        [kit, &writers, n, &writtenSamples, &lastRenderSeq, &blocksRendered, &silence]
        (const juce::AudioBuffer<float>&, int chunk) -> bool
        {
            // Same stale-buffer hazard the single-track tap has: the producer
            // task skips processStrips entirely on an idle block, and the strip
            // views then still hold the previous block's audio.
            const juce::uint32 seq   = kit->getStripRenderSeq();
            const bool         fresh = (seq != lastRenderSeq);
            if (fresh) { lastRenderSeq = seq; ++blocksRendered; }
            else if (silence.getNumSamples() < chunk)
                silence.setSize (2, chunk, false, true, false);

            for (int i = 0; i < n; ++i)
            {
                const juce::AudioBuffer<float> stripBuf = kit->getStripBuffer (i, chunk);
                auto& src = fresh ? stripBuf : silence;
                if (! writers[(size_t) i]->writeFromAudioSampleBuffer (src, 0, chunk))
                    return false;
            }
            writtenSamples += chunk;
            return true;
        });

    mProcessor.setFreezePrune (nullptr);
    writers.clear();   // close every handle before any delete

    if (! ok || blocksRendered == 0)
    {
        for (const auto& d : dests) d.deleteFile();
        if (ok) outErr = "The freeze render captured no audio for the drum kit.";
        return false;
    }

    {
        const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - t0;
        const double audioSecs = (writtenSamples > 0 && opts.sampleRate > 0.0)
                                   ? (double) writtenSamples / opts.sampleRate : 0.0;
        juce::String line;
        line << juce::Time::getCurrentTime().toString (true, true, false)
             << "  [TS7 FREEZE] KIT " << n << " strips | "
             << juce::String (audioSecs, 2) << "s audio in "
             << juce::String (elapsedMs, 0) << "ms";
        DBG (line);
        AppPaths::appRoot().getChildFile ("freeze_timing.txt")
            .appendText (line + juce::newLine);
    }
    return true;
}

// CL-227: append a violation, or extend the trailing one when it is the same kind
// and lands inside kViolationGapSeconds of it.  Coalescing here rather than in the
// report face means the cap counts SPANS, so a sustained overshoot cannot exhaust
// the row budget and hide later, unrelated breaches.
// Thin adapter onto the shared rule in LoudnessSpec.h -- kept so the existing
// call sites below read unchanged.
static void addOrExtendViolation (BuilderPage::MeasureResult& out,
                                  LoudnessViolation::Kind kind,
                                  double startSecs, double endSecs, float value,
                                  bool& truncated)
{
    LoudnessViolation::addOrExtend (out.violations, kind, startSecs, endSecs,
                                    value, truncated);
}

bool BuilderPage::measureRender (const RenderOptions& opts,
                                 MeasureResult& out,
                                 juce::String& outErr,
                                 std::function<bool()> shouldAbort,
                                 std::function<void(double)> onProgress,
                                 LoudnessSpec::Id specId,
                                 float customLufs)
{
    const double sr = opts.sampleRate > 0.0 ? opts.sampleRate : 44100.0;
    // §4.2: resolved, not get() -- Custom's target is the user's typed number.
    const LoudnessSpec spec = LoudnessSpec::resolved (specId, customLufs);
    out.customTargetLufs = customLufs;

    LufsMeterDSP lufs;
    lufs.prepareToPlay (sr);
    lufs.resetIntegrated();

    // BLU-108: real true-peak, not an estimate.  Per block the running max is
    // reset (filter history is NOT, so there is no seam) which gives both the
    // programme maximum and a per-block value the violation log can timecode.
    TruePeakMeter tp;
    tp.prepare (2);

    LoudnessRangeAccumulator lra;
    lra.reset();

    float  programmeTpLin = 0.0f;
    double elapsedSecs    = 0.0;
    // EBU Tech 3342 samples Short-Term at 10 Hz.  The offline block is 2048
    // (~46 ms at 44.1k), so gate on elapsed time rather than per block, or the
    // percentile weighting would follow the block size instead of the clock.
    double nextStSampleAt = 0.0;
    bool   truncated      = false;

    // §5.1 spectrum snapshot.  One transform per offline block (2048-sample
    // block, 2048-point FFT) accumulated as POWER and averaged at the end --
    // averaging in dB would let a single silent block drag a band toward the
    // floor far harder than it deserves.
    constexpr int kFftSize = 1 << MeasureResult::kSpectrumOrder;
    juce::dsp::FFT specFft (MeasureResult::kSpectrumOrder);
    juce::dsp::WindowingFunction<float> specWin ((size_t) kFftSize,
        juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> specScratch ((size_t) kFftSize * 2, 0.0f);
    std::vector<double> bandPower ((size_t) MeasureResult::kSpectrumBands, 0.0);
    std::vector<int>    bandCount ((size_t) MeasureResult::kSpectrumBands, 0);
    int specFrames = 0;

    const bool ok = runOfflineLoop (opts, outErr, std::move (shouldAbort),
                                    std::move (onProgress),
        [&] (const juce::AudioBuffer<float>& buf, int chunk) -> bool
        {
            lufs.process (buf);

            const double blockStart = elapsedSecs;
            elapsedSecs += (double) chunk / sr;

            tp.resetPeak();
            tp.process (buf);
            const float blockTpLin = tp.truePeakLinear();
            programmeTpLin = juce::jmax (programmeTpLin, blockTpLin);

            const float blockTpDb = juce::Decibels::gainToDecibels (blockTpLin, -144.0f);
            if (blockTpDb > spec.maxTruePeakDb)
                addOrExtendViolation (out, MeasureResult::Violation::Kind::TruePeak,
                                      blockStart, elapsedSecs, blockTpDb, truncated);

            out.maxMomentaryLufs = juce::jmax (out.maxMomentaryLufs, lufs.momentary());

            // §5.1: mono-sum a full transform's worth of this block, window,
            // transform, and fold the bins into log bands.  Blocks shorter than
            // the transform are skipped rather than zero-padded -- padding would
            // smear a partial tail block across every band.
            if (chunk >= kFftSize && buf.getNumChannels() > 0)
            {
                const float* sL = buf.getReadPointer (0);
                const float* sR = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : sL;
                for (int i = 0; i < kFftSize; ++i)
                    specScratch[(size_t) i] = 0.5f * (sL[i] + sR[i]);
                std::fill (specScratch.begin() + kFftSize, specScratch.end(), 0.0f);

                specWin.multiplyWithWindowingTable (specScratch.data(), (size_t) kFftSize);
                specFft.performFrequencyOnlyForwardTransform (specScratch.data());

                const double binHz = sr / (double) kFftSize;
                for (int b = 1; b < kFftSize / 2; ++b)
                {
                    const double hz = (double) b * binHz;
                    if (hz < MeasureResult::kSpectrumMinHz
                        || hz > MeasureResult::kSpectrumMaxHz) continue;

                    const double t = std::log (hz / MeasureResult::kSpectrumMinHz)
                                   / std::log ((double) MeasureResult::kSpectrumMaxHz
                                               / MeasureResult::kSpectrumMinHz);
                    const int band = juce::jlimit (0, MeasureResult::kSpectrumBands - 1,
                        (int) (t * (MeasureResult::kSpectrumBands - 1) + 0.5));

                    const double mag = (double) specScratch[(size_t) b];
                    bandPower[(size_t) band] += mag * mag;
                    ++bandCount[(size_t) band];
                }
                ++specFrames;
            }

            if (elapsedSecs >= nextStSampleAt)
            {
                nextStSampleAt = elapsedSecs + 0.1;
                const float st = lufs.shortTerm();
                lra.push (st);
                out.lufsCurve.push_back (st);   // §2.7: the drawable curve
                out.maxShortTermLufs = juce::jmax (out.maxShortTermLufs, st);

                // Only Custom carries a short-term ceiling today -- none of the
                // published specs in the table defines one, so this stays quiet
                // for them rather than inventing a threshold.
                const float stCeiling = spec.maxShortTermLufs;
                if (spec.checksShortTerm && st > stCeiling)
                    addOrExtendViolation (out, MeasureResult::Violation::Kind::ShortTerm,
                                          blockStart, elapsedSecs, st, truncated);
            }
            return true;
        });

    if (! ok) return false;

    out.violationsTruncated = truncated;
    out.specId          = specId;
    out.integratedLufs  = lufs.integrated();
    out.truePeakDb      = juce::Decibels::gainToDecibels (programmeTpLin, -144.0f);
    out.lraLu           = lra.lra();
    out.durationSeconds = elapsedSecs;
    out.truePeakInSpec  = (out.truePeakDb <= spec.maxTruePeakDb);
    out.integratedInSpec = (! spec.checksIntegrated)
                         || (std::abs (out.integratedLufs - spec.integratedLufs) <= spec.toleranceLu);

    // §5.1: collapse the accumulated power into one dB value per band.  Left
    // EMPTY when no full transform ever ran (a render shorter than 2048 samples),
    // so the report can omit the plot rather than draw a flat line at the floor
    // and imply the mix had no content.
    if (specFrames > 0)
    {
        out.spectrumDb.assign ((size_t) MeasureResult::kSpectrumBands, -120.0f);
        for (int i = 0; i < MeasureResult::kSpectrumBands; ++i)
        {
            if (bandCount[(size_t) i] <= 0) continue;
            const double meanPower = bandPower[(size_t) i] / (double) bandCount[(size_t) i];
            out.spectrumDb[(size_t) i] = (float) juce::Decibels::gainToDecibels (
                std::sqrt (meanPower) / (double) (1 << MeasureResult::kSpectrumOrder), -120.0);
        }
    }
    return true;
}

// QA-ModelShell TS2: the offline automation walk.  Same block filters as the
// live engine replay (song-grid clips, muted, row-audible), evaluated with
// the SAME shared point evaluator -- but resolution covers the lane classes
// the in-processBlock replay never could.
void BuilderPage::applyOfflineAutomationAt (double songBeat)
{
    // C.5b: Builder grid is uniform 4-beat-per-bar (matches the live replay).
    const double bar = songBeat / 4.0;

    for (int bi = 0; bi < mPM.getNumBlocks(); ++bi)
    {
        const auto& blk = mPM.getBlock (bi);
        if (blk.clipType != ClipType::Automation)   continue;
        if (blk.muted)                               continue;
        if (! mPM.isRowAudible (blk.trackRow))       continue;
        const auto& pid = blk.automationLane.paramId;
        if (pid.isEmpty())                           continue;
        if (blk.automationLane.points.empty())       continue;
        if (pid == "global_tempo")                   continue;   // the clock's job
        // Main-APVTS lanes replay inside processBlock, identical to live.
        if (mProcessor.apvts.getParameter (pid) != nullptr) continue;

        const double clipStart = effectiveStartBars (blk);
        const double len       = effectiveLengthBars (blk);
        if (len <= 0.0 || bar < clipStart || bar >= clipStart + len) continue;

        const float rel = juce::jlimit (0.f, 1.f, (float) ((bar - clipStart) / len));
        const float v01 = juce::jlimit (0.f, 1.f,
            evalAutomationPointsAt (blk.automationLane.points, rel));
        applyOfflineLaneValue (pid, v01);
    }
}

void BuilderPage::applyOfflineLaneValue (const juce::String& pid, float v01)
{
    auto applyToApvts = [v01] (juce::AudioProcessorValueTreeState* ap,
                               const juce::String& paramId) -> bool
    {
        if (ap == nullptr) return false;
        if (auto* param = ap->getParameter (paramId))
        {
            param->setValueNotifyingHost (v01);
            return true;
        }
        return false;
    };

    auto& rig = mProcessor.engineRig();

    // Plugins-tab instrument lanes: "plugtab<N>_vst_<paramId>" (2026-08-02,
    // registered in StandaloneEditor::registerPluginTabAutomation).  Same
    // landing rule as the rack's vst_ fork below: the offline branch ships in
    // the SAME pass as the live registration or exports silently drop the
    // lane class.
    if (pid.startsWith ("plugtab"))
    {
        const juce::String rest = pid.substring (7);
        const int us = rest.indexOfChar (0, '_');
        if (us > 0 && rest.substring (0, us).containsOnly ("0123456789")
            && rest.substring (us + 1).startsWith ("vst_"))
        {
            if (auto* inst = dynamic_cast<Hosting::HostedPluginInstance*> (
                    rig.engineFor (TabKind::Plugins,
                                   rest.substring (0, us).getIntValue())))
                inst->applyParamNorm (rest.substring (us + 1 + 4), v01);
            return;
        }
    }

    // Vox/Inst per-page lanes: "vox<N>_" / "inst<N>_" + the engine's bare id
    // (digits IMMEDIATELY after the word -- "vox_bus"/"inst_0" rack prefixes
    // carry an underscore first and fall through by construction).
    auto tryPageLane = [&] (const juce::String& word, TabKind kind) -> bool
    {
        if (! pid.startsWith (word)) return false;
        const juce::String tail = pid.substring (word.length());
        int digits = 0;
        while (digits < tail.length()
               && juce::CharacterFunctions::isDigit (tail[digits])) ++digits;
        if (digits == 0 || digits >= tail.length() || tail[digits] != '_') return false;
        const int idx = tail.substring (0, digits).getIntValue();
        const juce::String bare = tail.substring (digits + 1);
        auto* t = rig.findTab (kind, idx);
        if (t == nullptr) return false;
        if (kind == TabKind::Vox)
        {
            if (auto* v = dynamic_cast<BaySickVocalProcessor*> (t->engine.get()))
            {
                if (applyToApvts (EngineRig::apvtsOf (v), bare))                       return true;
                if (applyToApvts (EngineRig::apvtsOf (&v->getNamIrProcessor()), bare)) return true;
            }
            return false;
        }
        // Inst: the NAM/IR stage's bare ids.  A pedal-board lane also starts
        // "inst<N>_" but its bare id is "pedals_<uuid>_<suffix>", which is not a
        // NAM param -- it falls through to the pedal branch below.
        return applyToApvts (EngineRig::apvtsOf (t->namIr), bare);
    };
    if (tryPageLane ("vox",  TabKind::Vox))  return;
    if (tryPageLane ("inst", TabKind::Inst)) return;

    // Pedal-board lanes: "inst<N>_pedals_<slotUuid>_<suffix>".  The board is not
    // an EffectRack on a graph channel, so the rack walk below cannot see it --
    // without this branch pedal automation is simply absent from every export.
    // Slot by UUID, never index (a board reorder carries the uuid with the Slot),
    // and PanelContext::Pedal because the board builds the pedal FACE of the 7
    // dual-panel types, whose knobs share labels with the rack face at different
    // ranges and different setters.
    {
        const juce::String word = "inst";
        if (pid.startsWith (word))
        {
            const juce::String tail = pid.substring (word.length());
            int digits = 0;
            while (digits < tail.length()
                   && juce::CharacterFunctions::isDigit (tail[digits])) ++digits;
            const juce::String marker = "_pedals_";
            if (digits > 0 && tail.substring (digits).startsWith (marker))
            {
                const int idx  = tail.substring (0, digits).getIntValue();
                const juce::String rest = tail.substring (digits + marker.length());
                if (auto* t = rig.findTab (TabKind::Inst, idx))
                {
                    if (t->pedals != nullptr)
                    {
                        for (int s = 0; s < BaySickPedalsProcessor::kNumSlots; ++s)
                        {
                            const juce::String uuid = t->pedals->getSlotUuid (s);
                            if (uuid.isEmpty() || ! rest.startsWith (uuid + "_")) continue;
                            const juce::String suffix = rest.substring (uuid.length() + 1);
                            auto*     dsp  = t->pedals->getSlotEffect (s);
                            const auto type = t->pedals->getSlotType (s);
                            EffectParamMap::applyNorm (
                                type,
                                EffectParamMap::variantOf (type, dsp,
                                    EffectParamMap::PanelContext::Pedal),
                                dsp, suffix, v01);
                            return;
                        }
                    }
                }
            }
        }
    }

    // Engine lanes carry the engine's own globally-unique APVTS param id
    // ("tk_<trackId>_<fam>_*") -- sweep the rig for the owner.
    {
        bool applied = false;
        rig.forEachEngine ([&] (juce::AudioProcessor& p)
        {
            if (! applied && applyToApvts (EngineRig::apvtsOf (&p), pid))
                applied = true;
        });
        if (applied) return;
    }

    // The sfizz trio (Guitars / Basses / RustyDrums) is processor-owned, not
    // rig-owned, so the sweep above cannot see it.  Its ids are globally unique
    // too ("bgg_<idx>_", "bbb_<idx>_", "brd_"), so the same bare-id match works.
    {
        bool applied = false;
        mProcessor.forEachSfizzApvts ([&] (juce::AudioProcessorValueTreeState& ap)
        {
            if (! applied && applyToApvts (&ap, pid))
                applied = true;
        });
        if (applied) return;
    }

    // BLU-344 Harmless mod-editor lanes: "<targetParamId>_mod<N>_depth|length".
    // Not APVTS params -- fields on HarmlessModRegistry -- so nothing above can
    // resolve them, and without this branch the DEPTH/LENGTH automation that
    // plays live would be silently missing from every export.  The target id is
    // itself an engine param id, so the search is a rig sweep like the one above.
    if (pid.contains ("_mod"))
    {
        const juce::String tail = pid.fromLastOccurrenceOf ("_", false, false);
        const bool isDepth  = (tail == "depth");
        const bool isLength = (tail == "length");
        if (isDepth || isLength)
        {
            const juce::String head = pid.upToLastOccurrenceOf ("_", false, false);
            const juce::String srcTok = head.fromLastOccurrenceOf ("_mod", false, false);
            const juce::String targetId = head.upToLastOccurrenceOf ("_mod", false, false);
            if (srcTok.isNotEmpty() && srcTok.containsOnly ("0123456789"))
            {
                const int srcIdx = srcTok.getIntValue();
                bool applied = false;
                rig.forEachEngine ([&] (juce::AudioProcessor& p)
                {
                    if (applied) return;
                    auto* h = dynamic_cast<HarmlessProcessor*> (&p);
                    if (h == nullptr) return;
                    auto& reg = h->getModRegistry();
                    auto* tgt = reg.findTarget (targetId);
                    if (tgt == nullptr) return;
                    if (srcIdx < 0 || srcIdx >= (int) ModSource::NumSources) return;
                    auto& src = tgt->sources[(size_t) srcIdx];
                    if (isDepth)
                    {
                        src.depth = -1.0f + juce::jlimit (0.0f, 1.0f, v01) * 2.0f;
                    }
                    else
                    {
                        const int last = HarmlessModLength::kNumSteps - 1;
                        const int i = juce::jlimit (0, last,
                            (int) std::lround ((double) juce::jlimit (0.0f, 1.0f, v01) * last));
                        src.length = HarmlessModLength::kBeats[i];
                    }
                    reg.publishSnapshot();
                    applied = true;
                });
                if (applied) return;
            }
        }
    }

    // Legacy mixer fader spelling: "<mixerPrefix>_fader" -> the real _level
    // param.  ("_pan" lanes already carry the real param id and were consumed
    // by the main-APVTS branch upstream.)  TS3 retires the legacy spelling.
    if (pid.endsWith ("_fader"))
    {
        const juce::String levelId =
            pid.upToLastOccurrenceOf ("_fader", false, false) + "_level";
        if (auto* param = mProcessor.apvts.getParameter (levelId))
        {
            param->setValueNotifyingHost (v01);
            return;
        }
    }

    // Rack lanes: "<channelPrefix>_<slotUuid>_<suffix>".  Channel by prefix,
    // slot by UUID (never index -- rack reorder carries the uuid), DSP through
    // EffectParamMap keyed (type, variant).  "output_vol" is the ONE
    // rack-level non-DSP control: the rack's per-slot output gain,
    // -24..+12 dB mirroring EditorPanelBase's slider.
    auto tryRackChannel = [&] (int chId) -> bool
    {
        const juce::String prefix = EffectsPage::channelPrefixForId (chId) + "_";
        if (! pid.startsWith (prefix)) return false;
        auto* rack = EffectsPage::rackForChannelId (mProcessor.mVibeGraph, chId);
        if (rack == nullptr) return false;
        const juce::String rest = pid.substring (prefix.length());
        for (int s = 0; s < EffectRack::kNumSlots; ++s)
        {
            const juce::String uuid = rack->getSlotUuid (s);
            if (uuid.isEmpty() || ! rest.startsWith (uuid + "_")) continue;
            const juce::String suffix = rest.substring (uuid.length() + 1);
            if (suffix == "output_vol")
            {
                constexpr float kLo = -24.0f, kHi = 12.0f;
                rack->setSlotOutputGain (s,
                    kLo + juce::jlimit (0.0f, 1.0f, v01) * (kHi - kLo));
                return true;
            }
            // QA-ModelShell TS6: hosted plugin params are a lane class with no
            // EffectParamMap table, so they fork here.  Landing this in the
            // SAME pass as the live registration is the batch's standing rule
            // (fact 5) -- a lane class that plays live and is missing from the
            // export is exactly how vox/inst broke before TS2.
            if (suffix.startsWith ("vst_"))
            {
                Hosting::HostedPluginEffect::applyParamNorm (rack->getSlotEffect (s),
                                                             suffix.substring (4), v01);
                return true;
            }

            const EffectType type    = rack->getSlotType (s);
            const int        variant = EffectParamMap::variantOf (type, rack->getSlotEffect (s));
            EffectParamMap::applyNorm (type, variant, rack->getSlotEffect (s), suffix, v01);
            return true;
        }
        return false;
    };

    // The Effects dropdown's channel-id vocabulary (the ids lane paramIds
    // embed): buses 1-12, drums 100+, layers 200+, basses 300+, audio 400+,
    // aux 600+, vox 700+, inst 800+, rusty 900+.
    for (int id = 1; id <= 12; ++id)                                 if (tryRackChannel (id))       return;
    for (int i = 0; i < kMaxDrumPages; ++i)                          if (tryRackChannel (100 + i))  return;
    for (int i = 0; i < kMaxLayerPages; ++i)                         if (tryRackChannel (200 + i))  return;
    for (int i = 0; i < kMaxBassPages; ++i)                          if (tryRackChannel (300 + i))  return;
    for (int i = 0; i < MixerState::kMaxAudioRows; ++i)              if (tryRackChannel (400 + i))  return;
    for (int i = 0; i < (int) MixerChannelIds::kMaxAuxStrips; ++i)   if (tryRackChannel (600 + i))  return;
    for (int i = 0; i < (int) MixerChannelIds::kMaxVoxStrips; ++i)   if (tryRackChannel (700 + i))  return;
    for (int i = 0; i < (int) MixerChannelIds::kMaxInstStrips; ++i)  if (tryRackChannel (800 + i))  return;
    for (int i = 0; i < (int) MixerChannelIds::kMaxRustyStrips; ++i) if (tryRackChannel (900 + i))  return;
    // QA-ModelShell TS6: hosted VST3 instrument strips (dropdown 1000+).
    for (int i = 0; i < (int) MixerChannelIds::kMaxPluginStrips; ++i) if (tryRackChannel (1000 + i)) return;
}

void BuilderPage::runExportWithProgress (const RenderOptions& opts)
{
    // ThreadWithProgressWindow owns the window + Cancel; the render itself knows
    // nothing about UI beyond the two callbacks.
    //
    // launchThread() rather than runThread(): the latter spins a modal loop and
    // only exists under JUCE_MODAL_LOOPS_PERMITTED, which this project does not
    // enable.  launchThread is async, so the job heap-allocates and retires
    // itself in threadComplete.  SafePointer on the page because a long export
    // can outlive the tab that started it.
    struct RenderJob : public juce::ThreadWithProgressWindow
    {
        RenderJob (BuilderPage& owner, const RenderOptions& o)
            : juce::ThreadWithProgressWindow ("Exporting audio...", true, true),
              mOwner (&owner), mOpts (o) {}

        void run() override
        {
            if (auto* page = mOwner.getComponent())
                mOk = page->renderToFile (
                    mOpts, mErr,
                    [this] { return threadShouldExit(); },
                    [this] (double p) { setProgress (p); });
            else
                mErr = "The Builder page closed before the export started.";
        }

        void threadComplete (bool userPressedCancel) override
        {
            if (! userPressedCancel && ! mOk)
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "Export failed", mErr, "OK");
            delete this;
        }

        juce::Component::SafePointer<BuilderPage> mOwner;
        RenderOptions mOpts;
        juce::String  mErr;
        bool          mOk { false };
    };

    (new RenderJob (*this, opts))->launchThread();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern render -- now a thin wrapper over the shared harness.
// ─────────────────────────────────────────────────────────────────────────────
// TS7 §7.2: the pattern render's options popup.
//
// Three picks over TWO operations and two track sets, which is the whole model:
//   Per Track    -> one WAV per track          (set = every track in the pattern)
//   Full Mix     -> one WAV, stems SUMMED      (set = every track in the pattern)
//   Select Tracks-> choose the set, then choose which of the two operations.
//
// "Full Mix" is stems summed, NEVER the master output (Jeff 2026-07-29), so no
// master chain rides on it in either case -- the two Full Mix paths are the same
// operation over a different set, and Select Tracks adds no semantics of its own.
void BuilderPage::showPatternRenderOptions (int patternIndex)
{
    auto tracks = getPatternTracks (patternIndex);
    if (tracks.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon, "Nothing to render",
            "This pattern has no notes on any track.");
        return;
    }

    juce::PopupMenu m;
    m.addSectionHeader ("Render pattern");
    m.addItem (1, "Per Track  (" + juce::String ((int) tracks.size()) + " files)");
    m.addItem (2, "Full Mix  (1 file)");
    m.addSeparator();
    m.addItem (3, "Select Tracks...");

    m.showMenuAsync (juce::PopupMenu::Options{}, [this, patternIndex] (int r)
    {
        if (r == 1) startPatternRender (patternIndex, {}, /*perTrack*/ true);
        else if (r == 2) startPatternRender (patternIndex, {}, /*perTrack*/ false);
        else if (r == 3) showPatternTrackPicker (patternIndex);
    });
}

// The Select Tracks list: which tracks, then which way.  Both questions live in
// one box so the user is not walked through two modal steps for one decision.
void BuilderPage::showPatternTrackPicker (int patternIndex)
{
    auto tracks = getPatternTracks (patternIndex);
    if (tracks.empty()) return;

    // AlertWindow has addTextEditor / addComboBox / addCustomComponent, but NO
    // toggle-button helper, so the checkbox list is a component we own and read
    // back directly.  It is kept alive by the AlertWindow for the modal's
    // lifetime (addCustomComponent does not take ownership, so the list is held
    // in a shared_ptr the callback captures).
    struct TrackPickList : public juce::Component
    {
        std::vector<std::unique_ptr<juce::ToggleButton>> boxes;
        explicit TrackPickList (const std::vector<PatternTrackEntry>& entries)
        {
            for (const auto& e : entries)
            {
                auto tb = std::make_unique<juce::ToggleButton> (e.name);
                tb->setToggleState (true, juce::dontSendNotification);
                addAndMakeVisible (*tb);
                boxes.push_back (std::move (tb));
            }
            setSize (300, juce::jmax (24, (int) boxes.size() * 22));
        }
        void resized() override
        {
            auto b = getLocalBounds();
            for (auto& tb : boxes) tb->setBounds (b.removeFromTop (22));
        }
    };

    auto list = std::make_shared<TrackPickList> (tracks);

    auto* aw = new juce::AlertWindow ("Select Tracks",
                                      "Choose the tracks to render, and how:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addCustomComponent (list.get());
    aw->addComboBox ("how", { "One file per track", "One file, mixed together" }, "Output:");
    aw->addButton ("Render", 1);
    aw->addButton ("Cancel", 0);

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, patternIndex, aw, tracks, list] (int res)
        {
            if (res != 1) return;
            std::vector<int> picked;
            for (size_t i = 0; i < tracks.size() && i < list->boxes.size(); ++i)
                if (list->boxes[i]->getToggleState())
                    picked.push_back (tracks[i].channelId);

            if (picked.empty()) return;   // nothing ticked: silently do nothing
            const bool perTrack = (aw->getComboBoxComponent ("how")->getSelectedItemIndex() == 0);
            startPatternRender (patternIndex, picked, perTrack);
        }), true);
}

// One entry point for all three picks.  `channelIds` empty means "every track in
// the pattern"; perTrack chooses between N files and one summed file.
void BuilderPage::startPatternRender (int patternIndex,
                                      std::vector<int> channelIds,
                                      bool perTrack)
{
    auto tracks = getPatternTracks (patternIndex);
    if (tracks.empty()) return;

    // Resolve the set once, so the two branches below cannot disagree about it.
    std::vector<PatternTrackEntry> set;
    if (channelIds.empty()) set = tracks;
    else
        for (const auto& t : tracks)
            if (std::find (channelIds.begin(), channelIds.end(), t.channelId) != channelIds.end())
                set.push_back (t);
    if (set.empty()) return;

    auto& pat = mPM.getPattern (patternIndex);
    const juce::String base = pat.name.replaceCharacter (' ', '_');

    auto chooser = std::make_shared<juce::FileChooser> (
        "Render \"" + pat.name + "\" to WAV",
        mProcessor.getProjectExportsDir().getChildFile (base + ".wav"),
        "*.wav");

    chooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, patternIndex, set, perTrack, chooser] (const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest == juce::File()) return;

            RenderOptions opts;
            opts.scope        = RenderOptions::Scope::Pattern;
            opts.tail         = RenderOptions::Tail::Included;
            opts.patternIndex = patternIndex;
            opts.format       = RenderOptions::Format::Wav;
            opts.sampleRate   = 44100.0;
            opts.bitDepth     = 24;
            opts.destination  = dest;

            if (perTrack)
            {
                // Stems only: N files, no combined one the user did not ask for.
                opts.writeMainFile = false;
                for (const auto& t : set)
                    opts.stems.push_back ({ t.channelId, t.name });
            }
            else
            {
                // Stems SUMMED into the one file -- not the master output.
                for (const auto& t : set)
                    opts.mixTapChannels.push_back (t.channelId);
            }

            runExportWithProgress (opts);
        });
}

void BuilderPage::renderPatternToWav(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= mPM.getNumPatterns()) return;
    auto& pat = mPM.getPattern(patternIndex);
    String defaultName = pat.name.replaceCharacter(' ', '_') + ".wav";

    auto chooser = std::make_shared<FileChooser>(
        "Render \"" + pat.name + "\" to WAV",
        // QA-ModelShell TS2: exports live in <project>\Exports\ (locked
        // destination spec; created on demand).
        mProcessor.getProjectExportsDir().getChildFile(defaultName),
        "*.wav");

    chooser->launchAsync(
        FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
        [this, patternIndex, chooser](const FileChooser& fc) {
            auto dest = fc.getResult();
            if (dest == File()) return;

            // Same format defaults this path always used (44.1k / 24-bit); it
            // now shares the song render's harness, progress window and cancel
            // instead of blocking the message thread with its own loop.
            // The old fixed 2 s tail becomes Tail::Included, which renders until
            // the sound actually decays -- strictly better than guessing, and it
            // stops truncating patterns that end on a long reverb.
            RenderOptions opts;
            opts.scope        = RenderOptions::Scope::Pattern;
            opts.tail         = RenderOptions::Tail::Included;
            opts.patternIndex = patternIndex;
            opts.format       = RenderOptions::Format::Wav;
            opts.sampleRate   = 44100.0;
            opts.bitDepth     = 24;
            opts.destination  = dest;

            runExportWithProgress (opts);
        });
}

// TS7 §7.2: which tabs actually carry notes in this pattern.
//
// Rolls with no notes are SKIPPED rather than rendered silent: "5 tabs 5 wavs"
// means five tabs that play, and a folder of silent files per idle tab would be
// noise dressed as completeness.  Rusty is a singleton, hence its single roll.
std::vector<BuilderPage::PatternTrackEntry>
BuilderPage::getPatternTracks (int patternIndex) const
{
    std::vector<PatternTrackEntry> out;
    if (patternIndex < 0 || patternIndex >= mPM.getNumPatterns()) return out;
    const auto& pat = mPM.getPattern (patternIndex);

    auto addIf = [&out] (const PianoRollData& roll, int chId, const juce::String& name)
    {
        if (! roll.notes.empty())
            out.push_back ({ chId, name });
    };

    for (int i = 0; i < (int) pat.layerRoll.size(); ++i)
        addIf (pat.layerRoll[(size_t) i], MixerChannelIds::layerInsert (i),
               "Layer " + juce::String (i + 1));
    for (int i = 0; i < (int) pat.bassRoll.size(); ++i)
        addIf (pat.bassRoll[(size_t) i], MixerChannelIds::bassInsert (i),
               "Bass " + juce::String (i + 1));
    for (int i = 0; i < (int) pat.drumRolls.size(); ++i)
        addIf (pat.drumRolls[(size_t) i], MixerChannelIds::drumInsert (i),
               "Drums " + juce::String (i + 1));
    for (int i = 0; i < (int) pat.clipRoll.size(); ++i)
        addIf (pat.clipRoll[(size_t) i], MixerChannelIds::audioInsert (i),
               "Clip " + juce::String (i + 1));
    for (int i = 0; i < (int) pat.voxRoll.size(); ++i)
        addIf (pat.voxRoll[(size_t) i], MixerChannelIds::voxInsert (i),
               "Vox " + juce::String (i + 1));
    for (int i = 0; i < (int) pat.instRoll.size(); ++i)
        addIf (pat.instRoll[(size_t) i], MixerChannelIds::instInsert (i),
               "Inst " + juce::String (i + 1));
    for (int i = 0; i < (int) pat.pluginRoll.size(); ++i)
        addIf (pat.pluginRoll[(size_t) i], MixerChannelIds::pluginInsert (i),
               "Plugin " + juce::String (i + 1));

    // The kit is ONE tab across 13 strips, so it gets ONE entry ("1 tab 1
    // wav").  The tap is the kit BUS -- the only point where the kit exists as
    // a single signal; the 13 inserts cannot be told apart from the shared
    // roll without re-deriving the note-to-piece map.  Omitting it silently
    // dropped the entire drum kit from pattern Per Track and Full Mix exports.
    addIf (pat.baySickRustyDrumsRoll, MixerChannelIds::kRustyDrumsBus, "Drum Kit");

    return out;
}

// TS7 §7.1: consolidate one arrangement row to a single WAV.
//
// Scope is the whole SONG deliberately, not just the row's occupied span: the
// point is a stem you can drop straight back in, and a song-length file aligned
// to bar 1 does that.  A span-trimmed file would need its offset carried
// separately and would silently misplace itself.
void BuilderPage::renderTrackRowToWav (int row)
{
    const int chId = MixerChannelIds::audioInsert (row);

    juce::String rowName = mPM.getRowNames()[(size_t) juce::jlimit (0, ArrangementGrid::kNumRows - 1, row)];
    if (rowName.isEmpty()) rowName = "Track " + juce::String (row + 1);
    const juce::String defaultName = rowName.replaceCharacter (' ', '_') + ".wav";

    auto chooser = std::make_shared<juce::FileChooser> (
        "Render \"" + rowName + "\" to WAV",
        mProcessor.getProjectExportsDir().getChildFile (defaultName),
        "*.wav");

    chooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chId, chooser] (const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest == juce::File()) return;

            RenderOptions opts;
            opts.scope       = RenderOptions::Scope::Song;
            opts.tail        = RenderOptions::Tail::Included;
            opts.format      = RenderOptions::Format::Wav;
            opts.sampleRate  = 44100.0;
            opts.bitDepth    = 24;
            opts.destination = dest;
            // ONE tap: the row's own strip, summed alone.  Everything else still
            // plays during the pass, so a compressor on this row keyed off another
            // track keeps its ducking -- the reason stems are one-pass.
            opts.mixTapChannels = { chId };

            runExportWithProgress (opts);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Builder menus - hosted by the WINDOW title strip since T16 (Jeff, 2026-08-04).
// Tools is gone: Draw / Paint / Select / Delete / Mute / Slice / Zoom / Play
// Selected all have a toolbar button one row below, so the menu was a duplicate
// of controls already on screen.
// ─────────────────────────────────────────────────────────────────────────────
void BuilderPage::buildEditMenu (PopupMenu& m)
{
    const bool canUndo = mGrid && mGrid->canUndo();
    const bool canRedo = mGrid && mGrid->canRedo();
    auto key = [this] (KeyPress k) { if (mGrid) mGrid->keyPressed (k); };

    m.addItem ("Undo", canUndo, false, [this] { if (mGrid) mGrid->undo(); });
    m.addItem ("Redo", canRedo, false, [this] { if (mGrid) mGrid->redo(); });
    m.addSeparator();
    m.addItem ("Select All\tCtrl+A", [key] { key (KeyPress ('a', ModifierKeys::ctrlModifier, 0)); });
    m.addItem ("Deselect\tEsc",      [key] { key (KeyPress (KeyPress::escapeKey)); });
    m.addSeparator();
    m.addItem ("Copy\tCtrl+C",       [key] { key (KeyPress ('c', ModifierKeys::ctrlModifier, 0)); });
    m.addItem ("Paste\tCtrl+V",      [key] { key (KeyPress ('v', ModifierKeys::ctrlModifier, 0)); });
    m.addItem ("Delete\tDel",        [key] { key (KeyPress (KeyPress::deleteKey)); });
    m.addItem ("Duplicate\tCtrl+B",  [key] { key (KeyPress ('b', ModifierKeys::ctrlModifier, 0)); });
}

void BuilderPage::buildClipsMenu (PopupMenu& m)
{
    m.addItem ("Import Audio...",        [this] { doImportAudio(); });
    m.addSeparator();
    m.addItem ("Rename Pattern\tF2",     [this] { doRenamePattern(); });
    m.addItem ("Find Next Empty\tF4",    [this] { doFindNextEmptyPattern(); });
    m.addItem ("New Automation Clip...", [this] { doNewAutomationClip(); });
    m.addSeparator();
    m.addItem ("Render Pattern to WAV...", [this]
    {
        if (mGrid && mPM.getNumPatterns() > 0)
            showPatternRenderOptions (mPM.getCurrentPatternIndex());
    });
}

void BuilderPage::buildViewMenu (PopupMenu& m)
{
    // Jeff, 2026-08-04: "Toggle Browser" removed.  It was a second click-path to
    // the same setCollapsed() the "<<" button called, and the browser now
    // collapses by dragging its edge past the magnetic floor -- one gesture, not
    // a button and a menu entry doing the same thing.
    m.addItem ("Zoom In\t+",  [this] { doZoom (1.15f); });
    m.addItem ("Zoom Out\t-", [this] { doZoom (1.f / 1.15f); });
    m.addSeparator();
    m.addItem ("Performance Mode\tCtrl+P", true, mPerfMode,
               [this] { doPerformanceModeToggle(); });
}
