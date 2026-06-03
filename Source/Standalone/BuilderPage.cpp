#include "BuilderPage.h"
#include "PatternColorPicker.h"
#include "../ClipDropDiag.h"        // QA-ClipDrop: diagnostic trap (2026-06-02)
using namespace juce;

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
                                  std::shared_ptr<PendingRoute>&);
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
    // Single left click: TreeView default selection behavior.
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
    // triangle icon).  Right-click is a no-op on category nodes.
    if (e.mods.isPopupMenu()) return;
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
    mCollapseBtn = std::make_unique<TextButton>("<<");
    mCollapseBtn->onClick = [this] { setCollapsed(!mCollapsed); };
    addAndMakeVisible(*mCollapseBtn);

    static const char* kTabLabels[] = { "Patterns", "Audio", "Auto" };
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
        mPM.addPattern();
        mSelectedPat = mPM.getNumPatterns() - 1;
        mPM.setCurrentPattern(mSelectedPat);
        rebuildPatternRows();
        if (onPatternSelected) onPatternSelected(mSelectedPat);
    };
    addAndMakeVisible(*mAddBtn);

    mDeleteBtn = std::make_unique<TextButton>("Delete");
    mDeleteBtn->onClick = [this] {
        if (mPM.getNumPatterns() > 1) {
            mPM.removePattern(mSelectedPat);
            mSelectedPat = jlimit(0, mPM.getNumPatterns() - 1, mSelectedPat);
            mPM.setCurrentPattern(mSelectedPat);
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
    mAudioRoot->addSubItem (clipsCat.release());
    mAudioRoot->addSubItem (voxCat  .release());
    mAudioRoot->addSubItem (instCat .release());
    mAudioTree->setRootItem (mAudioRoot.get());
    mAudioTree->setRootItemVisible (false);

    rebuildPatternRows();
    switchTab(0);
}

void BrowserPanel::setCollapsed(bool c)
{
    mCollapsed = c;
    mCollapseBtn->setButtonText(c ? ">>" : "<<");
    for (auto& t : mTabBtns) t->setVisible(!c);
    for (auto& r : mPatItems)   r->setVisible(!c);
    for (auto& r : mAudioItems) r->setVisible(!c);
    for (auto& r : mAutomItems) r->setVisible(!c);
    mAddBtn->setVisible(!c);
    mDeleteBtn->setVisible(!c);
    // G-5: tree visibility tracks the collapsed state on the Audio tab.
    if (mAudioTree) mAudioTree->setVisible (! c && mActiveTab == 1);
    if (auto* p = getParentComponent()) p->resized();
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

    for (int i = 0; i < 3; ++i)
        mTabBtns[i]->setToggleState(i == t, dontSendNotification);

    if (isAud)  rebuildAudioRows();
    if (isAuto) rebuildAutomationRows();
    resized();
}

void BrowserPanel::selectPattern(int idx)
{
    mSelectedPat = idx;
    mPM.setCurrentPattern(idx);
    for (int i = 0; i < (int)mPatItems.size(); ++i)
        if (mPatItems[i]) mPatItems[i]->setSelected(i == idx);
    if (onPatternSelected) onPatternSelected(idx);
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
    for (auto& e : entries)
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

        if      (e.category == "Clips") mClipsCat->addSubItem (leaf);
        else if (e.category == "Vox")   mVoxCat  ->addSubItem (leaf);
        else if (e.category == "Inst")  mInstCat ->addSubItem (leaf);
        else                            delete leaf;   // unknown category - drop
    }
}

void BrowserPanel::showAudioTreeContextMenu (AudioBrowserItem& item, Point<int> globalPt)
{
    const int libIdx = item.getAudioLibIdx();
    if (libIdx < 0 || libIdx >= mPM.getNumAudioLibrary()) return;

    constexpr int kIdRename     = 1;
    constexpr int kIdDuplicate  = 2;
    constexpr int kIdDelete     = 3;
    constexpr int kIdProperties = 4;   // QA-E Task 7 (FILE-02)
    constexpr int kIdReveal     = 7;
    constexpr int kIdChokeBase  = 200;

    PopupMenu m;
    m.addItem (kIdRename,    "Rename...");
    m.addItem (kIdDuplicate, "Duplicate...");
    m.addItem (kIdReveal,    "Reveal in Explorer");
    // QA-E Task 7 (FILE-02): the library entry is the source of truth for
    // routing.  Editing this moves every grid copy still following it.
    m.addItem (kIdProperties, "Properties...");
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
        [this, libIdx, absPath, onRename] (int result)
        {
            if (result == 0) return;

            if (result == kIdRename)
            {
                if (onRename) onRename();
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
                mPM.setAudioLibraryChokeGroup (libIdx, result - kIdChokeBase);
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
            snapshot << mPM.getAudioLibraryPath(i) << "/" << mPM.getAudioLibraryAlias(i) << "\n";
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
                mPM.setAudioLibraryChokeGroup(idx, result - kIdChokeBase);
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
            if (result == 6 && kind == BrowserItem::Kind::Pattern)
            {
                // F-1 (2026-04-26): live-preview colour picker.
                if (idx < 0 || idx >= mPM.getNumPatterns()) return;
                const juce::Colour curCol = mPM.getPattern(idx).color;
                PatternColorPicker::showAsync (raw, curCol,
                    [this, idx] (juce::Colour newCol)
                    {
                        if (idx < 0 || idx >= mPM.getNumPatterns()) return;
                        mPM.getPattern(idx).color = newCol;
                        rebuildPatternRows();
                        repaint();
                    });
                return;
            }
            if (result == 4 && kind == BrowserItem::Kind::Pattern)
            {
                int newIdx = mPM.duplicatePattern(idx);
                if (newIdx >= 0)
                {
                    mSelectedPat = newIdx;
                    mPM.setCurrentPattern(newIdx);
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
                        mPM.removePattern(idx);
                        mSelectedPat = jlimit(0, mPM.getNumPatterns() - 1, mSelectedPat);
                        mPM.setCurrentPattern(mSelectedPat);
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
                            // Cascade-remove any blocks created from this
                            // template (matched by paramId).
                            const String pid = mPM.getAutomationTemplate(tplIdx).paramId;
                            for (int i = mPM.getNumBlocks() - 1; i >= 0; --i)
                                if (mPM.getBlock(i).clipType == ClipType::Automation
                                    && mPM.getBlock(i).automationLane.paramId == pid)
                                    mPM.removeBlock(i);
                            mPM.removeAutomationTemplate(tplIdx);
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
    mPM.renamePattern(idx, finalName);
    rebuildPatternRows();
}

void BrowserPanel::renameAudioAt(int idx, const String& newName)
{
    if (idx < 0 || idx >= mAudioPaths.size()) return;
    const String path = mAudioPaths[idx];
    // Dup-check against other audio library aliases (not against self).
    const int libIdx = [&]() -> int
    {
        for (int i = 0; i < mPM.getNumAudioLibrary(); ++i)
            if (mPM.getAudioLibraryPath(i) == path) return i;
        return -1;
    }();
    const juce::String finalName = ensureUniqueBrowserName (
        newName, libIdx, mPM.getNumAudioLibrary(),
        [this] (int i) { return mPM.getAudioLibraryAlias(i); });
    // Persist the alias in the library so it survives block deletion.
    if (libIdx >= 0) mPM.setAudioLibraryAlias(libIdx, finalName);
    // Also stamp every currently-placed block so the clip title matches.
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        auto& bb = mPM.getBlock(i);
        if (bb.clipType == ClipType::Audio && bb.audioFilePath == path)
            bb.displayAlias = finalName;
    }
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
    rebuildAutomationRows();
}

void BrowserPanel::paint(Graphics& g)
{
    g.fillAll(kBrowserBg);
    g.setColour(VC::Accent.withAlpha(0.6f));
    g.fillRect(getWidth() - 1, 0, 1, getHeight());

    if (mCollapsed) return;

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
    mCollapseBtn->setBounds(b.removeFromTop(22).removeFromRight(28).reduced(1));
    if (mCollapsed) return;

    auto tabRow = b.removeFromTop(22).reduced(2, 1);
    int  tabW   = tabRow.getWidth() / 3;
    for (int t = 0; t < 3; ++t)
        mTabBtns[t]->setBounds(tabRow.removeFromLeft(tabW).reduced(1));
    b.removeFromTop(2);

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
    for (int r = 0; r < kNumRows; ++r)
        mRowNames[r] = "Track " + String(r + 1);

    setWantsKeyboardFocus(true);
}

ArrangementGrid::~ArrangementGrid() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Coordinate helpers  (no kLabelW offset - labels are external)
// ─────────────────────────────────────────────────────────────────────────────
int ArrangementGrid::barToX(float bar) const
{
    return (int)((bar - mBarOff) * mPPBar);
}

float ArrangementGrid::xToBar(int x) const
{
    return mBarOff + (float)x / mPPBar;
}

int ArrangementGrid::rowToY(int row) const
{
    return kRulerH + (int)(row * mEffectiveRowH);
}

int ArrangementGrid::yToRow(int y) const
{
    int rh = jmax(1, (int)mEffectiveRowH);
    return (y - kRulerH) / rh;
}

float ArrangementGrid::snapBar(float bar) const
{
    if (mAltSnapActive) return bar;
    return snapBarAlt(bar);
}

float ArrangementGrid::snapBarAlt(float bar) const
{
    switch (mSnapMode)
    {
        case SnapMode::Bar:    return std::round(bar);
        case SnapMode::Beat:   return std::round(bar * (float)mTimeSig.numerator) / (float)mTimeSig.numerator;
        case SnapMode::Cell:   return std::round(bar * 2.f) / 2.f;
        case SnapMode::Steps:  return std::round(bar * 16.f) / 16.f;
        case SnapMode::Events: return bar;   // snap to existing events -- simplify to no-snap
        case SnapMode::Line:
        case SnapMode::None:   return bar;
        default:               return std::round(bar);
    }
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
    if (y < kRulerH) return -1;
    int row = yToRow(y);
    for (int i = mPM.getNumBlocks() - 1; i >= 0; --i)
    {
        const auto& b = mPM.getBlock(i);
        if (b.trackRow != row) continue;
        // QA-Ea Task 0c (2026-05-20): use effectiveStartBars so slip-edited
        // clips (possibly negative-start, sub-bar precision) hit-test on
        // their actual visible position, not the legacy int-bar startBar.
        int bx = barToX((float) effectiveStartBars(b));
        int bw = (int)(effectiveLengthBars(b) * mPPBar);
        if (x >= bx && x < bx + bw) return i;
    }
    return -1;
}

bool ArrangementGrid::nearRightEdge(int blockIdx, int x) const
{
    const auto& b = mPM.getBlock(blockIdx);
    int bx = barToX((float) effectiveStartBars(b));
    int bw = (int)(effectiveLengthBars(b) * mPPBar);
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

    int bx = barToX((float)b.startBar);
    int bw = jmax(4, (int)(effectiveLengthBars(b) * mPPBar) - 1);
    int by = rowToY(b.trackRow) + 2;
    int bh = (int)mEffectiveRowH - 4;

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
    if (row >= 0 && row < kNumRows) { mRowNames[row] = name; repaint(); }
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
    g.setColour(kHeaderBg);
    g.fillRect(0, 0, b.getWidth(), kRulerH);
    g.setColour(VC::Accent.withAlpha(0.6f));
    g.drawHorizontalLine(kRulerH - 1, 0.f, (float)b.getWidth());

    int totalBars = totalVisibleBars();
    int maxBar    = (int)mBarOff + b.getWidth() / jmax(1, (int)mPPBar) + 2;

    // Subdivision tick marks in ruler (adaptive: beat / 1/8 / 1/16 / 1/32).
    // C.5b (post-revert): Builder grid is uniform 4-beat-per-bar.  Song-level
    // TS markers are decorative flags drawn separately, not driving the grid.
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
            // Skip positions that are whole bars (drawn below with labels)
            if (std::fmod(bar, 1.f) < 1e-5f) continue;
            int rx = barToX(bar);
            if (rx < 0 || rx > b.getWidth()) continue;
            g.drawVerticalLine(rx, (float)(kRulerH - (int)rl.tickH), (float)kRulerH);
        }
    }

    // Bar lines + labels (always shown). No upper cap on bar number - the
    // ruler extends as far as the viewport reaches, regardless of song length.
    // QA-Ea Task 0c (Option ii): use std::floor instead of (int) so negative
    // mBarOff (viewport scrolled into negative-bar territory for slip-edited
    // clips) doesn't lose the first label due to int-truncation-toward-zero.
    (void)totalBars;
    for (int bar = (int)std::floor(mBarOff); bar <= maxBar; ++bar)
    {
        int rx = barToX((float)bar);
        if (rx < 0 || rx > b.getWidth()) continue;

        bool isMajor = (bar % (mTimeSig.numerator) == 0);
        g.setColour(isMajor ? VC::Accent.brighter(0.5f) : VC::Accent.withAlpha(0.5f));
        g.drawVerticalLine(rx, 0, (float)kRulerH);

        if (isMajor || mPPBar >= 30) {
            g.setColour(VC::Text);
            g.setFont(Font(9));
            // QA-Ea Task 0c (2026-05-20): ruler labels are 0-indexed so the
            // song downbeat (bar 0 in code) shows as "0", and slip-edited
            // clips in negative-bar territory show "-1", "-2", etc.
            g.drawText(String(bar), rx + 2, 1, 24, kRulerH - 2,
                       Justification::centredLeft, false);
        }
    }

    // Performance mode: pulse animation on current bar
    if (mPerfMode && mPlayheadBar >= 0.0)
    {
        float pulse = 0.5f + 0.5f * std::sin(mPulsePhi);
        int px = barToX((float)mPlayheadBar);
        g.setColour(VC::Highlight.withAlpha(0.55f * pulse));
        g.fillRect(jmax(0, px - 4), 0, 8, kRulerH);
    }

    // Time selection highlight
    if (mTimeSelStart >= 0.f && mTimeSelEnd > mTimeSelStart)
    {
        int sx = barToX(mTimeSelStart);
        int ex = barToX(mTimeSelEnd);
        g.setColour(VC::Highlight.withAlpha(0.30f));
        g.fillRect(sx, 0, ex - sx, kRulerH);
        g.setColour(VC::Highlight.withAlpha(0.85f));
        g.drawVerticalLine(sx, 0.f, (float)kRulerH);
        g.drawVerticalLine(ex, 0.f, (float)kRulerH);

        // Also shade the full grid column
        g.setColour(VC::Highlight.withAlpha(0.07f));
        g.fillRect(sx, kRulerH, ex - sx, getHeight() - kRulerH);
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
            g.drawVerticalLine(rx, 1.f, flagH + 1.f);
            // Pennant (small triangle pointing right).
            juce::Path flag;
            flag.addTriangle((float) rx + 1, 2.f,
                             (float) rx + 1 + flagW, 2.f + flagH * 0.45f,
                             (float) rx + 1, 2.f + flagH * 0.45f);
            g.fillPath(flag);
        }

        for (int i = 0; i < mPM.getNumTimeSigChanges(); ++i)
        {
            const auto& ts = mPM.getTimeSigChange(i);
            int rx = barToX((float) ts.bar);
            if (rx < -32 || rx > b.getWidth()) continue;
            const juce::String txt = juce::String(ts.num) + "/" + juce::String(ts.den);
            const int textW = 28;
            // Background pill.
            g.setColour(VC::Blue.withAlpha(0.85f));
            g.fillRoundedRectangle((float)(rx + 1), 1.f, (float) textW, (float)(kRulerH - 2), 2.f);
            // Text.
            g.setColour(juce::Colours::white);
            g.setFont(Font(9.f, Font::bold));
            g.drawText(txt, rx + 1, 1, textW, kRulerH - 2,
                       Justification::centred, false);
        }
    }

    // Zoom hint in ruler
    g.setColour(VC::TextDim.withAlpha(0.35f));
    g.setFont(Font(8.f));
    g.drawText("Ctrl+Scroll=zoom  Alt+Scroll=vZoom  P=Draw  B=Paint  E=Select  D=Delete  T=Mute",
               4, 1, 500, kRulerH - 2, Justification::centredLeft);

    // Playhead triangle arrow - drawn last so it's always on top of ruler content
    if (mPlayheadBar >= 0.0)
    {
        int px = barToX((float)mPlayheadBar);
        if (px >= 0 && px < b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.9f));
            juce::Path tri;
            tri.addTriangle((float)px,       0.f,
                            (float)(px - 5), (float)kRulerH,
                            (float)(px + 5), (float)kRulerH);
            g.fillPath(tri);
        }
    }
}

void ArrangementGrid::drawRowBgs(Graphics& g) const
{
    auto b  = getLocalBounds();
    int  rh = (int)mEffectiveRowH;
    for (int r = 0; r < kNumRows; ++r)
    {
        int y = rowToY(r);
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

    // Adaptive subdivisions: show finer lines as you zoom in.
    // A level is shown when its pixel spacing >= 5 px.
    // Draw finest first; coarser lines overdraw at the same pixel.
    static constexpr float kMinSpacing = 5.f;
    struct Level { int denom; Colour col; };
    const Level levels[] = {
        { 32, kGridLine.withAlpha(0.10f)  },  // 1/32 note  (8 per beat)
        { 16, kGridLine.withAlpha(0.18f)  },  // 1/16 note
        {  8, kGridLine.withAlpha(0.28f)  },  // 1/8 note
        {  4, kGridLine.withAlpha(0.50f)  },  // quarter-note / beat
        {  1, kGridLineMj.withAlpha(0.85f)},  // bar line (always shown)
    };

    int maxBar = (int)mBarOff + b.getWidth() / jmax(1, (int)mPPBar) + 2;

    for (const auto& lv : levels)
    {
        float spacing = mPPBar / (float)lv.denom;
        if (lv.denom > 1 && spacing < kMinSpacing) continue;

        float step     = 1.f / (float)lv.denom;
        float startBar = std::floor(mBarOff * (float)lv.denom) / (float)lv.denom;
        g.setColour(lv.col);
        for (float bar = startBar; bar <= (float)(maxBar + 1); bar += step)
        {
            int x = barToX(bar);
            if (x < 0 || x > b.getWidth()) continue;
            g.drawVerticalLine(x, (float)yTop, (float)yBot);
        }
    }
}

// MIDI note shading inside a pattern clip.
// Maps MIDI 72 (C5) to vertical centre; ±24 semitones (2 octaves) span the
// full clip height. Notes outside that range are clamped to the edges.
// Aggregates layer rolls, bass rolls, and the drum roll so the clip preview
// reflects the whole pattern's content.
void ArrangementGrid::drawMidiShading(Graphics& g, const ArrangementBlock& b,
                                      int bx, int by, int bw, int bh) const
{
    if (b.patternIndex < 0 || b.patternIndex >= mPM.getNumPatterns()) return;
    const auto& pat = mPM.getPattern(b.patternIndex);

    // C.5b: block MIDI shading uses the pattern's intrinsic TS to map note
    // beats to fractional positions inside the block.
    double beatsPerBar = mPM.getPatternBeatsPerBar (b.patternIndex);
    double totalBeats  = (double)pat.bars * beatsPerBar;
    if (totalBeats <= 0.0) return;

    constexpr int   kCentreNote   = 72;   // C5
    constexpr float kHalfRangeSt  = 24.f; // ±2 octaves spans full clip height
    const int innerH = jmax(1, bh - 4);
    const int innerY = by + 2;

    Colour base   = blockColour(b);
    Colour noteCol = base.brighter(0.7f).withAlpha(0.6f);

    auto paintRoll = [&](const PianoRollData& roll)
    {
        if (roll.notes.empty()) return;
        for (const auto& n : roll.notes)
        {
            const float startFrac = (float)(n.startBeat / totalBeats);
            const float lenFrac   = (float)(n.durationBeats / totalBeats);
            // Centre = 0.5; semitones above centre move up (smaller y), below move down.
            const float relSt    = (float)(n.midiNote - kCentreNote);
            const float yFrac    = jlimit(0.f, 1.f, 0.5f - 0.5f * (relSt / kHalfRangeSt));
            const int nx = bx + (int)(startFrac * bw);
            const int nw = jmax(1, (int)(lenFrac * bw));
            const int ny = innerY + (int)(yFrac * (float)(innerH - 2));
            g.setColour(noteCol);
            g.fillRect(nx, ny, nw, 2);
        }
    };

    for (const auto& lr : pat.layerRoll) paintRoll(lr);
    for (const auto& br : pat.bassRoll)  paintRoll(br);
    paintRoll(pat.drumRoll);
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
        if      (rc >= 600 && rc < 606)  base = Colour (0xff0fafa5);   // Vox
        else if (rc >= 700 && rc < 706)  base = Colour (0xff1c3a8a);   // Inst
        else if (rc >= 400 && rc < 450)  base = Colour (0xffd4a017);   // Clips
        else                             base = kAudioGeneric;          // unrouted
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
    int n  = mPM.getNumBlocks();
    int rh = (int)mEffectiveRowH;
    for (int i = 0; i < n; ++i)
    {
        const auto& b = mPM.getBlock(i);
        // QA-Ea Task 0c (2026-05-20): use effectiveStartBars so slip-edited
        // clips render at their actual visual position (sub-bar precision +
        // possibly negative for clips slipped into the pre-roll zone).
        int bx = barToX((float) effectiveStartBars(b));
        int bw = jmax(4, (int)(effectiveLengthBars(b) * mPPBar) - 1);
        int by = rowToY(b.trackRow) + 2;
        int bh = rh - 4;

        if (bx + bw < 0 || bx > getWidth()) continue;
        if (by + bh < kRulerH || by > getHeight()) continue;

        bool sel = isSelected(i);
        switch (b.clipType)
        {
            case ClipType::Pattern:    drawPatternClip   (g, b, bx, by, bw, bh, sel); break;
            case ClipType::Audio:      drawAudioClip     (g, b, bx, by, bw, bh, sel); break;
            case ClipType::Automation: drawAutomationClip(g, b, bx, by, bw, bh, sel); break;
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
}

void ArrangementGrid::drawPreviewBlock(Graphics& g) const
{
    if (!mDrawing || mDrawRow < 0 || mDrawRow >= kNumRows) return;
    float len = snapBarAlt(mDrawEnd) - snapBarAlt(mDrawStart);
    if (len < 1.f) len = 1.f;
    int x = barToX(snapBarAlt(mDrawStart));
    int w = jmax(4, (int)(len * mPPBar) - 1);
    int y = rowToY(mDrawRow) + 2;
    int h = (int)mEffectiveRowH - 4;

    g.setColour(VC::Highlight.withAlpha(0.45f));
    g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.f);
    g.setColour(VC::Highlight.withAlpha(0.85f));
    g.drawRoundedRectangle((float)x + 0.5f, (float)y + 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 1.f);
}

void ArrangementGrid::drawGhostClip(Graphics& g) const
{
    if (!mHasGhost) return;
    const auto& b = mGhostBlock;
    int x = barToX((float)b.startBar);
    int w = jmax(4, (int)(effectiveLengthBars(b) * mPPBar) - 1);
    int y = rowToY(b.trackRow) + 2;
    int h = (int)mEffectiveRowH - 4;

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

    // Green line from ruler bottom to grid bottom only (arrow drawn in drawRuler so it's on top)
    g.setColour(VC::Green.withAlpha(0.8f));
    g.fillRect(px, kRulerH, 2, getHeight() - kRulerH);
}

void ArrangementGrid::drawPerformanceOverlays(Graphics& g) const
{
    if (!mPerfMode || mPlayheadBar < 0.0) return;

    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        const auto& b = mPM.getBlock(i);
        if (b.startBar > mPlayheadBar || b.startBar + effectiveLengthBars(b) <= mPlayheadBar) continue;

        int bx = barToX((float)b.startBar);
        int bw = jmax(4, (int)(effectiveLengthBars(b) * mPPBar) - 1);
        int by = rowToY(b.trackRow) + 2;
        int bh = (int)mEffectiveRowH - 4;

        float progress = (float)(mPlayheadBar - b.startBar) / (float) juce::jmax(0.001, effectiveLengthBars(b));
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
    drawRuler(g);   // ruler on top
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
    mPendingRowNames = mRowNames;
}

void ArrangementGrid::commitEdit()
{
    if (!mUndoCtx.isValid()) return;

    std::vector<ArrangementBlock> afterBlocks;
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
        afterBlocks.push_back(mPM.getBlock(i));

    auto* action = new ArrangementEditAction(
        mPendingLabel,
        { mPendingBlocks, std::vector<juce::String>(mPendingRowNames.begin(), mPendingRowNames.end()) },
        { afterBlocks,    std::vector<juce::String>(mRowNames.begin(), mRowNames.end()) },
        [this](const ArrangementEditAction::Snapshot& s) {
            std::array<juce::String, kNumRows> rn;
            for (int i = 0; i < kNumRows && i < (int)s.rowNames.size(); ++i)
                rn[i] = s.rowNames[i];
            applySnapshot(s.blocks, rn);
        });

    mUndoCtx.perform(action, mPendingLabel);
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
    if (onArrangementChanged)   onArrangementChanged();
}

void ArrangementGrid::applySnapshot(const std::vector<ArrangementBlock>& blocks,
                                    const std::array<juce::String, kNumRows>& rowNames)
{
    while (mPM.getNumBlocks() > 0) mPM.removeBlock(0);
    for (const auto& b : blocks) mPM.addBlock(b);
    mRowNames = rowNames;
    mSelection.clear();
    resized();
    repaint();
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
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
            const float bs = (float) b.startBar;
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
        int bx = barToX((float)b.startBar);
        int bw = (int)(effectiveLengthBars(b) * mPPBar);
        int by = rowToY(b.trackRow);
        if (mMarqueeRect.intersects(Rectangle<int>(bx, by, bw, (int)mEffectiveRowH)))
            mSelection.push_back(i);
    }
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Operations
// ─────────────────────────────────────────────────────────────────────────────
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
        const float bs = (float) b.startBar;
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
            const float bs = (float) b.startBar;
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
        const float bs = (float) mPM.getBlock(i).startBar;
        if (bs >= t0 - kEps && bs < t1 - kEps)
            mPM.removeBlock(i);
    }
    // 2) slide every block that starts at or after t1 left by removedBars.
    for (int i = 0; i < mPM.getNumBlocks(); ++i)
    {
        auto& b = mPM.getBlock(i);
        if ((float) b.startBar >= t1 - kEps)
            b.startBar = juce::jmax(0, b.startBar - removedBars);
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
            earliest = jmin(earliest, (float)mPM.getBlock(idx).startBar);
    for (int idx : mSelection) {
        if (idx >= mPM.getNumBlocks()) continue;
        auto b = mPM.getBlock(idx);
        b.startBar -= (int)earliest;
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
        b.startBar += pasteStart;
        mPM.addBlock(b);
        mSelection.push_back(mPM.getNumBlocks() - 1);
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
            if ((float)src.startBar >= selStart && (float)src.startBar < selEnd)
            {
                ArrangementBlock nb = src;
                nb.startBar = src.startBar + (int)selLen;
                newBlocks.push_back(nb);
            }
        }
        if (newBlocks.empty()) return;

        beginEdit("Duplicate (Timeline)");
        mSelection.clear();
        for (auto& nb : newBlocks) {
            mPM.addBlock(nb);
            mSelection.push_back(mPM.getNumBlocks() - 1);
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
            rightmost = jmax(rightmost, (float)(mPM.getBlock(idx).startBar + effectiveLengthBars(mPM.getBlock(idx))));

    float earliest = 1e9f;
    for (int i2 : mSelection)
        if (i2 < mPM.getNumBlocks())
            earliest = jmin(earliest, (float)mPM.getBlock(i2).startBar);

    std::vector<ArrangementBlock> newBlocks;
    for (int idx : mSelection) {
        if (idx >= mPM.getNumBlocks()) continue;
        auto b = mPM.getBlock(idx);
        b.startBar = (int)rightmost + (mPM.getBlock(idx).startBar - (int)earliest);
        newBlocks.push_back(b);
    }
    mSelection.clear();
    for (auto& nb : newBlocks) {
        mPM.addBlock(nb);
        mSelection.push_back(mPM.getNumBlocks() - 1);
    }
    commitEdit();
    resized(); repaint();
}

void ArrangementGrid::nudgeSelection(int dBars, int dRows)
{
    if (mSelection.empty()) return;
    beginEdit("Nudge");
    for (int idx : mSelection) {
        if (idx >= mPM.getNumBlocks()) continue;
        auto& b = mPM.getBlock(idx);
        b.startBar = jmax(0, b.startBar + dBars);
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
    const float minPP = vpW / 32.f, maxPP = vpW / 8.f;
    mPPBar  = jlimit(minPP, maxPP, vpW / lenBars);
    mBarOff = jmax(0.f, (float) b.startBar);
    resized(); repaint();
}

// 2026-04-26 (D-2): TooltipClient - show marker label / TS info when the
// mouse hovers over a flag or TS pill on the ruler.  Returns empty string
// when the cursor is outside the ruler band so we don't overwrite tooltips
// owned by other components (block names etc).
juce::String ArrangementGrid::getTooltip()
{
    const auto pt = getMouseXYRelative();
    if (pt.y < 0 || pt.y >= kRulerH) return {};

    const float bar = xToBar (pt.x);
    const int   markerIdx = mPM.findTimeMarkerNearBar (bar, 0.5f);
    if (markerIdx >= 0)
    {
        const auto& m = mPM.getTimeMarker (markerIdx);
        const juce::String name = m.label.isNotEmpty() ? m.label : juce::String("(unnamed marker)");
        return "Marker: " + name + "  -  Bar " + juce::String (m.bar + 1)
             + "\nRight-click to edit / delete";
    }

    const int snapped = juce::jmax (0, (int) std::floor (bar));
    const int tsIdx = mPM.findTimeSigChangeAtBar (snapped);
    if (tsIdx >= 0)
    {
        const auto& ts = mPM.getTimeSigChange (tsIdx);
        return "Time Signature: " + juce::String (ts.num) + "/" + juce::String (ts.den)
             + "  @ Bar " + juce::String (ts.bar + 1)
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

    const int existingMarker = mPM.findTimeMarkerNearBar(clickedBar, 0.5f);
    const int existingTS     = mPM.findTimeSigChangeAtBar(snappedBar);

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
    m.addItem (10, "Add Time Marker at Bar " + juce::String(snappedBar + 1) + "...");
    m.addItem (11, "Add Time Signature at Bar " + juce::String(snappedBar + 1) + "...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent(this),
        [this, snappedBar, existingMarker, existingTS](int result)
        {
            if (result <= 0) return;
            if (result == 1 && existingMarker >= 0) promptRenameTimeMarker(existingMarker);
            else if (result == 2 && existingMarker >= 0) { mPM.removeTimeMarker(existingMarker); repaint(); }
            else if (result == 3 && existingTS >= 0)     promptEditTimeSigChange(existingTS);
            else if (result == 4 && existingTS >= 0)     { mPM.removeTimeSigChange(existingTS); repaint(); }
            else if (result == 10) promptAddTimeMarker(snappedBar);
            else if (result == 11) promptAddTimeSigChange(snappedBar);
        });
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
            mPM.addTimeMarker (bar, aw->getTextEditorContents("label").trim());
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
            mPM.renameTimeMarker (idx, aw->getTextEditorContents("label").trim());
            repaint();
        }), true);
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
            mPM.addTimeSigChange (bar, n, d);
            repaint();
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
            mPM.addTimeSigChange (cur.bar, n, d);
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
                const float startBars = (float) blk.startBar;
                const float snapped   = std::round(startBars / unit) * unit;
                blk.startBar = jmax(0, (int) std::round(snapped));
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
        m.addItem(6, "Properties...");                 // QA-E Task 7 (FILE-02):
                                                        // renamed; now also hosts
                                                        // the Routing dropdown
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
                        mPM.setPatternTimeSig (patIdx, kTsOpts[optIdx].n, kTsOpts[optIdx].d);
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
                              std::shared_ptr<PendingRoute>& outPending)
{
    aw.addTextEditor ("pitch", juce::String (curPitch, 2), "Pitch shift (semitones):");
    aw.addTextEditor ("bpm",   juce::String (curBPM, 1),   "Original BPM:");

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
    // Per-clip dialog: offerMove == false -> the menu offers "Copy to X" only
    // (per-clip routing always forks a copy; the acted-on block becomes it).
    buildAudioPropsControls (*aw, block.pitchSemitones, block.originalBPM,
                             block.stretchMode, curRouteName, pages,
                             /*offerMove*/ false,
                             /*offerResetToMaster*/ true, routeBtn, pending);

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
            const float bpmClamped = jmax(1.f, aw->getTextEditorContents("bpm").getFloatValue());
            bool  stretch = true;
            if (auto* cb = aw->getComboBoxComponent("mode"))
                stretch = (cb->getSelectedItemIndex() == 0);

            // QA-E Task 7 (FILE-02): per-clip routing pick = COPY (Jeff
            // 2026-05-15).  Duplicate FIRST, THEN create the new page bound
            // to the DUPLICATE (so "Copy to a new Clip Page" registers only
            // the one dupe entry -- not the original too).  Then tag it
            // (dedup-safe) + THIS block BECOMES the copy.
            if (pending && pending->chosen && onDuplicateFileForCopy)
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

    // Compute bars + precise beats from file duration at the assumed original BPM (120).
    // stretchMode will handle tempo differences at playback time.
    // QA-E Task 5 (2026-05-15): set lengthBeats to the precise float beat count
    // so playback / loop end match the source file's exact duration -- mirrors
    // the recording-finalize dropWavAsClip path at StandaloneEditor.cpp:9853-9854.
    // Prior bug: lengthBeats defaulted to -1.f, falling back to lengthBars * 4
    // (rounded up to next bar boundary), so drag-from-browser blocks were
    // longer than the underlying audio.
    int lengthBars = 4;  // fallback
    float originalBPM = 120.f;
    float fileBeats = -1.f;
    if (reader && reader->sampleRate > 0 && reader->lengthInSamples > 0)
    {
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
    // Register the stored path in the persistent audio library so the Browser
    // keeps showing it even if every block referencing it gets deleted.
    // QA-E Task 4 (2026-05-12): tag ownerChannelId so re-drag from browser
    // continues to find the entry under the right category.
    mPM.addAudioToLibrary(storedPath, {}, routeChannel);
    ClipDropDiag::log ("importAudioFile library-add", "storedPath=" + storedPath + " routeChannel=" + juce::String (routeChannel) + " libCount=" + juce::String (mPM.getNumAudioLibrary()));
    ArrangementBlock b;
    b.clipType       = ClipType::Audio;
    b.trackRow       = jlimit(0, kNumRows - 1, targetRow);
    b.startBar       = (int)snapBar(targetBar);
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
        onAudioClipAdded(b.trackRow, mRowNames[b.trackRow], storedPath);
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
    if (! f.existsAsFile()) return;

    // Read file metadata for exact lengthBeats (mirrors importAudioFile).
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    int   lengthBars  = 4;
    const float originalBPM = 120.f;
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
    b.startBar      = (int) snapBar (targetBar);
    b.lengthBars    = lengthBars;
    b.setLengthBeats (fileBeats);
    b.audioFilePath = path;
    b.originalBPM   = originalBPM;
    b.stretchMode   = true;
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
    mGhostBlock.startBar      = (int)snapBar(xToBar(x));
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
    if (y < kRulerH) { mHasGhost = false; repaint(); return; }

    mGhostBlock = ArrangementBlock();
    mGhostBlock.trackRow   = jlimit(0, kNumRows - 1, yToRow(y));
    mGhostBlock.startBar   = jmax(0, (int)snapBar(xToBar(x)));
    mGhostBlock.lengthBars = 1;

    if (kind == "pattern")
    {
        mGhostBlock.clipType     = ClipType::Pattern;
        mGhostBlock.patternIndex = idx;
        if (idx >= 0 && idx < mPM.getNumPatterns())
            mGhostBlock.lengthBars = jmax(1, mPM.getPattern(idx).bars);
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
    String kind; int idx;
    if (!parseBrowserDragDescription(d.description, kind, idx))
    {
        mHasGhost = false; repaint(); return;
    }

    const int x = d.localPosition.getX();
    const int y = d.localPosition.getY();
    if (y < kRulerH) { mHasGhost = false; repaint(); return; }

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
        b.startBar     = (int)bar;
        b.lengthBars   = jmax(1, mPM.getPattern(idx).bars);
        mPM.addBlock(b);
        mSelection.clear();
        mSelection.push_back(mPM.getNumBlocks() - 1);
        commitEdit();
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
            b.startBar       = (int)bar;
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

void ArrangementGrid::mouseDown(const MouseEvent& e)
{
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
    if (e.y < kRulerH)
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
            // Bare left-click on ruler: seek playhead to clicked position
            double beat = static_cast<double>(xToBar(e.x)) * 4.0;
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

    // 2026-04-26 (D-1): Alt+LClick on a block = toggle mute, regardless of
    // active tool.  Falls through when the click misses (so Zoom tool's
    // Alt+click = zoom-out still works on empty area).
    if (e.mods.isAltDown() && ! e.mods.isCtrlDown() && ! e.mods.isShiftDown())
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
        float factor = e.mods.isAltDown() ? (1.f / 1.25f) : 1.25f;
        float vpW = 800.f;
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            vpW = (float)jmax(1, vp->getWidth());
        const float minPP = vpW / 32.f, maxPP = vpW / 8.f;
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

    // Slice tool: split clip at click position
    if (mActiveTool == AGTool::Slice)
    {
        int hit = blockAtPos(e.x, e.y);
        if (hit >= 0) {
            auto& orig = mPM.getBlock(hit);
            float cutBar = snapBar(xToBar(e.x));
            if (cutBar > orig.startBar && cutBar < orig.startBar + orig.lengthBars) {
                beginEdit("Slice");
                ArrangementBlock right = orig;
                right.startBar  = (int)cutBar;
                right.lengthBars = orig.startBar + orig.lengthBars - (int)cutBar;
                orig.lengthBars = (int)cutBar - orig.startBar;
                mPM.addBlock(right);
                commitEdit();
                resized(); repaint();
            }
        }
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
            // ── Automation clip: point editing ───────────────────────────────
            if (mPM.getBlock(hit).clipType == ClipType::Automation)
            {
                mAutomEditBlock = hit;
                int bx = barToX((float)mPM.getBlock(hit).startBar);
                int bw = jmax(4, (int)(mPM.getBlock(hit).lengthBars * mPPBar) - 1);
                int by = rowToY(mPM.getBlock(hit).trackRow) + 2;
                int bh = (int)mEffectiveRowH - 4;

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
                    mResizeOrigStart = (float)mPM.getBlock(hit).startBar;
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
                // Resize or Shift+drag = time-stretch for audio clips
                beginEdit("Resize");
                mResizing        = true;
                mStretching      = e.mods.isShiftDown() && mPM.getBlock(hit).clipType == ClipType::Audio;
                mResizeIdx       = hit;
                mResizeOrigLen   = (float)mPM.getBlock(hit).lengthBars;
                mResizeOrigStart = (float)mPM.getBlock(hit).startBar;
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
                    mMoveOrigBars.push_back((float)mPM.getBlock(idx).startBar);
                    mMoveOrigRows.push_back(mPM.getBlock(idx).trackRow);
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
        b.startBar     = (int)bar;
        b.lengthBars   = 1;
        b.layerTrack   = true;
        mPM.addBlock(b);
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
                    mMoveOrigBars.push_back((float)mPM.getBlock(idx).startBar);
                    mMoveOrigRows.push_back(mPM.getBlock(idx).trackRow);
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
                int bx = barToX((float)blk.startBar);
                int bw = jmax(4, (int)(blk.lengthBars * mPPBar) - 1);
                int by = rowToY(blk.trackRow) + 2;
                int bh = (int)mEffectiveRowH - 4;
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
        int bx = barToX((float)blk.startBar);
        int bw = jmax(4, (int)(blk.lengthBars * mPPBar) - 1);
        int by = rowToY(blk.trackRow) + 2;
        int bh = (int)mEffectiveRowH - 4;

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

        // For audio stretch: adjust originalBPM so the content duration stays proportional
        if (mStretching) {
            // Store new length; actual time-stretching applied offline via Rubber Band
            (void)mResizeOrigLen;
        }
        resized(); repaint();
        return;
    }

    if (mMoving)
    {
        float dBars = (float)(e.x - mMoveDragOrigin.x) / mPPBar;
        int   dRows = (int)std::round((float)(e.y - mMoveDragOrigin.y) / mEffectiveRowH);
        // QA-Ea Task 0c (2026-05-20 - perf): hoist maxRevealableNegativeBars
        // out of the per-block loop.  The function does an O(num-blocks) scan
        // and was previously called per selected block -- O(N*M) per drag
        // fire for N selected blocks against M total blocks.  Now O(M) per
        // drag fire regardless of selection size.
        const int negFloor = -(int) std::ceil (maxRevealableNegativeBars());
        for (int i = 0; i < (int)mMoveIndices.size(); ++i) {
            int idx = mMoveIndices[i];
            if (idx >= mPM.getNumBlocks()) continue;
            auto& b = mPM.getBlock(idx);
            // QA-Ea Task 0c (2026-05-20 - Option ii): allow moves into the
            // negative-bar zone (mirror of the user-scroll clamp lift).  Floor
            // matches the dynamic negative-bar viewport limit so users can
            // park clips in the same negative space the viewport reveals
            // after a slip-edit; auto-fit operations still keep their own
            // bar-0 floors elsewhere.
            b.startBar = jmax(negFloor,
                              (int) snapBar(mMoveOrigBars[i] + dBars));
            // QA-Ea Task 0c (2026-05-20): a user-initiated MOVE overrides any
            // prior slip-edit sub-bar startBeats.  Clearing the sentinel
            // resets the block to bar-aligned int-precision (effectiveStartBeats
            // falls back to startBar * 4) so the moved clip lands exactly on
            // its grid bar with no leftover sub-bar offset from a previous
            // slip drag.
            b.startTicks = ArrangementBlock::kStartTicksUnset;
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
        // Paint continuous blocks left-to-right, avoid overlaps
        if (bar > mPaintLastBar + 0.5f && mPaintRow >= 0)
        {
            ArrangementBlock b;
            b.trackRow     = mPaintRow;
            b.patternIndex = mBrowserSelection;
            b.startBar     = (int)mPaintLastBar + 1;
            b.lengthBars   = 1;
            b.layerTrack   = true;
            mPM.addBlock(b);
            mPaintLastBar = bar;
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
            blk.startBar            = (int) std::floor (newStartBeats / 4.0);
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
                const float bs = (float) b.startBar;
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

    if (mResizing) { commitEdit(); mResizing = false; mStretching = false; mResizeIdx = -1; updateCursor(); return; }
    if (mMoving)   { commitEdit(); mMoving = false; mMoveIndices.clear(); updateCursor(); return; }
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
            beginEdit("Draw");
            ArrangementBlock b;
            b.trackRow     = row;
            b.patternIndex = mBrowserSelection;
            b.startBar     = (int)start;
            b.lengthBars   = len;
            b.layerTrack   = true;
            mPM.addBlock(b);
            commitEdit();
            mSelection.clear();
            mSelection.push_back(mPM.getNumBlocks() - 1);
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
                const float minPP = vpW / 32.f, maxPP = vpW / 8.f;
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
    if (e.y < kRulerH) return;
    int hit = blockAtPos(e.x, e.y);
    if (hit >= 0 && mPM.getBlock(hit).clipType == ClipType::Audio)
        showAudioClipProperties(hit);
    else if (hit >= 0 && mPM.getBlock(hit).clipType == ClipType::Automation)
    {
        // Check if double-clicking a curve handle diamond → reset its tension to 0
        if (mActiveTool == AGTool::Draw)
        {
            const auto& b  = mPM.getBlock(hit);
            int bx = barToX(b.startBar);
            int bw = jmax(4, (int)(effectiveLengthBars(b) * mPPBar) - 1);
            int by = rowToY(b.trackRow) + 2;
            int bh = (int)mEffectiveRowH - 4;
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
        // mouse. Limits: full out = 32 bars in viewport, full in = 8 bars.
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        float minPP  = vpW / 32.f;
        float maxPP  = vpW / 8.f;
        const float anchorBar = xToBar(e.x);
        mPPBar = jlimit(minPP, maxPP, mPPBar * factor);
        // QA-Ea Task 0c (Option ii): allow negative-bar zoom anchor.
        mBarOff = jmax(-(float) maxRevealableNegativeBars(),
                       anchorBar - (float)e.x / jmax(1.f, mPPBar));
        resized(); repaint();
    }
    else if (e.mods.isAltDown())
    {
        // Alt+scroll = vertical zoom: full out = all kNumRows fill viewport, full in = 8 rows
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        float minRH  = vpH / (float)kNumRows;
        float maxRH  = vpH / 8.f;
        mEffectiveRowH = jlimit(minRH, maxRH, mEffectiveRowH * factor);
        resized(); repaint();
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
                if (kc == '1' + i) { mPPBar = kZoomPresets[i]; resized(); repaint(); return true; }
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
                const float minPP = vpW / 32.f, maxPP = vpW / 8.f;
                const float factor = zoomIn ? 1.25f : (1.f / 1.25f);
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
    int totalBars = totalVisibleBars();
    // Get viewport size so we never leave blank space at the edges
    float vpW = 800.f, vpH = 600.f;
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        vpW = (float)jmax(1, vp->getWidth());
        vpH = (float)jmax(1, vp->getHeight());
    }
    int w = jmax((int)vpW, (int)(totalBars * mPPBar));
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

int TrackHeaderPanel::yToRow(int y) const
{
    int gridY = y - ArrangementGrid::kRulerH + mYOffset;
    int rh    = jmax(1, (int)mGrid.getEffectiveRowH());
    return gridY / rh;
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
    // "BUILDER" label in corner
    g.setColour(VC::TextDim);
    g.setFont(Font(8.f, Font::bold));
    g.drawText("BUILDER", 4, 1, b.getWidth() - 8, ArrangementGrid::kRulerH - 2, Justification::centredLeft);

    // Track rows
    const auto& names = mGrid.getRowNames();
    int rh = jmax(1, (int)mGrid.getEffectiveRowH());
    for (int r = 0; r < ArrangementGrid::kNumRows; ++r)
    {
        int y = ArrangementGrid::kRulerH + r * rh - mYOffset;
        if (y + rh < 0 || y > b.getHeight()) continue;

        g.setColour(kHeaderBg);
        g.fillRect(0, y, b.getWidth(), rh);

        // Subtle alternating tint
        if (r % 2 == 0) {
            g.setColour(Colours::white.withAlpha(0.02f));
            g.fillRect(0, y, b.getWidth(), rh);
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

    // Right border
    g.setColour(VC::Accent.withAlpha(0.8f));
    g.drawVerticalLine(b.getWidth() - 1, (float)ArrangementGrid::kRulerH, (float)b.getHeight());
}

void TrackHeaderPanel::resized() {}

int TrackHeaderPanel::hitTestLed(int x, int y, int& kind) const
{
    kind = -1;
    if (y < ArrangementGrid::kRulerH) return -1;
    const int rh = jmax(1, (int)mGrid.getEffectiveRowH());
    const int gridY = y - ArrangementGrid::kRulerH + mYOffset;
    const int row   = gridY / rh;
    if (row < 0 || row >= ArrangementGrid::kNumRows) return -1;
    const int yInRow = gridY - row * rh;
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
                if (n.isNotEmpty()) mGrid.setRowName(row, n);
            }
        }), true);
}

void TrackHeaderPanel::showTrackContextMenu(int row)
{
    PopupMenu m;
    m.addItem(1, "Rename...");
    m.addSeparator();
    m.addItem(2, "Move Up");
    m.addItem(3, "Move Down");
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
                            if (n.isNotEmpty()) mGrid.setRowName(row, n);
                        }
                    }), true);
                break;
            }
            case 2:
            case 3:
            {
                // Swap row names + move all blocks in these rows
                int otherRow = (result == 2) ? row - 1 : row + 1;
                if (otherRow < 0 || otherRow >= ArrangementGrid::kNumRows) break;
                // Swap names
                auto names = mGrid.getRowNames();
                String tmp = names[row];
                mGrid.setRowName(row, names[otherRow]);
                mGrid.setRowName(otherRow, tmp);
                // Swap block rows
                for (int i = 0; i < mPM.getNumBlocks(); ++i) {
                    auto& b = mPM.getBlock(i);
                    if (b.trackRow == row)       b.trackRow = otherRow;
                    else if (b.trackRow == otherRow) b.trackRow = row;
                }
                repaint();
                break;
            }
            case 4:
            {
                // Delete all clips on this track row
                for (int i = mPM.getNumBlocks() - 1; i >= 0; --i)
                    if (mPM.getBlock(i).trackRow == row) mPM.removeBlock(i);
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
    mZoomInBtn ->onClick = [this] { if (onZoom) onZoom(1.25f); };
    mZoomOutBtn->onClick = [this] { if (onZoom) onZoom(1.f / 1.25f); };
    addAndMakeVisible(*mZoomInBtn);
    addAndMakeVisible(*mZoomOutBtn);

    mSnapLabel = std::make_unique<Label>("", "Snap:");
    mSnapLabel->setFont(Font(10.f));
    mSnapLabel->setColour(Label::textColourId, VC::TextDim);
    addAndMakeVisible(*mSnapLabel);

    mSnapCombo = std::make_unique<ComboBox>();
    mSnapCombo->addItem("Bar",    1);
    mSnapCombo->addItem("Beat",   2);
    mSnapCombo->addItem("Cell",   3);
    mSnapCombo->addItem("Line",   4);
    mSnapCombo->addItem("Steps",  5);
    mSnapCombo->addItem("Events", 6);
    mSnapCombo->addItem("None",   7);
    mSnapCombo->setSelectedId(1, dontSendNotification);
    mSnapCombo->onChange = [this] {
        if (!onSnapChanged) return;
        static const SnapMode kModes[] = {
            SnapMode::Bar, SnapMode::Beat, SnapMode::Cell, SnapMode::Line,
            SnapMode::Steps, SnapMode::Events, SnapMode::None
        };
        int idx = mSnapCombo->getSelectedItemIndex();
        if (idx >= 0 && idx < 7) onSnapChanged(kModes[idx]);
    };
    addAndMakeVisible(*mSnapCombo);

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
    mSnapLabel ->setBounds(b.removeFromLeft(36).reduced(1));
    mSnapCombo ->setBounds(b.removeFromLeft(72).reduced(1));
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
    mBrowser->onRenderPattern = [this](int idx) { renderPatternToWav(idx); };
    mBrowser->onImportAudio   = [this](const String& path) {
        if (mGrid) mGrid->importAudioFile(path, 0, 0.f);
    };
    mBrowser->onArrangementChanged = [this] { notifyArrangementChanged(); };
    addAndMakeVisible(*mBrowser);

    // Grid + viewport
    mGrid = std::make_unique<ArrangementGrid>(pm, mAFM, mThumbCache);
    mGrid->onToolChanged = [this](ArrangementGrid::AGTool t) {
        if (mToolbar) mToolbar->setActiveTool(t);
    };
    mGrid->onUndoRedoStateChanged = [this] { syncToolbar(); };
    mGrid->onImportAudioRequested = [this] { doImportAudio(); };
    mGrid->onRowHeightChanged     = [this] { if (mTrackHeader) mTrackHeader->repaint(); };

    mGridViewport = std::make_unique<Viewport>();
    mGridViewport->setViewedComponent(mGrid.get(), false);
    mGridViewport->setScrollBarsShown(true, true);
    mGridViewport->setScrollBarThickness(10);
    addAndMakeVisible(*mGridViewport);

    // Track header panel (Task 10 - fixed left label column)
    mTrackHeader = std::make_unique<TrackHeaderPanel>(*mGrid, pm);
    addAndMakeVisible(*mTrackHeader);

    // Toolbar
    mToolbar = std::make_unique<ArrangementToolbar>();
    mToolbar->onToolSelected = [this](ArrangementGrid::AGTool t) {
        if (mGrid) mGrid->setTool(t);
    };
    mToolbar->onSnapChanged = [this](SnapMode s) {
        if (mGrid) mGrid->setSnapMode(s);
    };
    mToolbar->onZoom = [this](float factor) {
        if (!mGrid || !mGridViewport) return;
        float vpW  = (float)jmax(1, mGridViewport->getWidth());
        float minPP = vpW / 32.f, maxPP = vpW / 8.f;
        // Anchor zoom around the bar currently at viewport center so content
        // doesn't slide off-screen on each click.
        const float centreVpX = vpW * 0.5f;
        const float centreGridX = (float)mGridViewport->getViewPositionX() + centreVpX;
        const float anchorBar = mGrid->xToBar((int)centreGridX);
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

    // Menu bar (replaces ≡ popup)
    mMenuBarModel = std::make_unique<BuilderMenuBar>(*this);
    mMenuBar = std::make_unique<juce::MenuBarComponent>(mMenuBarModel.get());
    addAndMakeVisible(*mMenuBar);

    // Initial context label
    {
        const int idx = mPM.getCurrentPatternIndex();
        const juce::String name = (idx >= 0 && idx < mPM.getNumPatterns())
            ? mPM.getPattern(idx).name : juce::String();
        if (mToolbar) mToolbar->setContextText("Playlist > " + name);
    }

    startTimerHz(30);  // higher rate for performance mode animations
}

BuilderPage::~BuilderPage()
{
    // QA-D Task 4 (QA-0a finding #8): defensive teardown of the MenuBarComponent
    // before its model is destroyed.  See PianoRollContainer::~PianoRollContainer
    // for the rationale.
    if (mMenuBar)
    {
        mMenuBar->setModel (nullptr);
        mMenuBar.reset();
    }

    stopTimer();
    if (auto* top = getTopLevelComponent())
        top->removeKeyListener(this);
}

void BuilderPage::setUndoContext(const UndoContext& ctx)
{
    if (mGrid) { mGrid->setUndoContext(ctx); syncToolbar(); }
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

    // Browser panel (left)
    int browserW = mBrowser->isCollapsed() ? 28 : 180;
    mBrowser->setBounds(b.removeFromLeft(browserW));

    // Menu bar (above toolbar)
    if (mMenuBar) mMenuBar->setBounds(b.removeFromTop(kMenuBarH));

    // Toolbar (right of browser, below menu bar)
    mToolbar->setBounds(b.removeFromTop(ArrangementToolbar::kHeight));

    // Track header (fixed left label column)
    mTrackHeader->setBounds(b.removeFromLeft(ArrangementGrid::kLabelW));

    // Grid viewport (remaining area)
    mGridViewport->setBounds(b);
    // Grid content sized by ArrangementGrid::resized() - just trigger it
    if (mGrid) mGrid->resized();
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
}

// ─────────────────────────────────────────────────────────────────────────────
// BuilderPage actions (called from ≡ toolbar menu / keyboard shortcuts)
// ─────────────────────────────────────────────────────────────────────────────
void BuilderPage::doImportAudio()
{
    auto chooser = std::make_shared<FileChooser>(
        "Import Audio File",
        File::getSpecialLocation(File::userMusicDirectory),
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
    const float minPP = vpW / 32.f, maxPP = vpW / 8.f;
    const float centreVpX = vpW * 0.5f;
    const float centreGridX = (float)mGridViewport->getViewPositionX() + centreVpX;
    const float anchorBar = mGrid->xToBar((int)centreGridX);
    mGrid->mPPBar = jlimit(minPP, maxPP, mGrid->mPPBar * factor);
    // QA-Ea Task 0c (Option ii): allow negative-bar viewport.
    mGrid->mBarOff = jmax(-(float) mGrid->maxRevealableNegativeBars(),
                          anchorBar - centreGridX / jmax(1.f, mGrid->mPPBar));
    mGrid->resized();
    resized();
}

void BuilderPage::doToggleBrowser()
{
    if (mBrowser) { mBrowser->setCollapsed(!mBrowser->isCollapsed()); resized(); }
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
                        bar = jmax(bar, b.startBar + b.lengthBars);
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
                ArrangementBlock b;
                b.clipType               = ClipType::Automation;
                b.trackRow               = row;
                b.startBar               = bar;
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
// Render to WAV (unchanged from original)
// ─────────────────────────────────────────────────────────────────────────────
void BuilderPage::renderPatternToWav(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= mPM.getNumPatterns()) return;
    auto& pat = mPM.getPattern(patternIndex);
    String defaultName = pat.name.replaceCharacter(' ', '_') + ".wav";

    auto chooser = std::make_shared<FileChooser>(
        "Render \"" + pat.name + "\" to WAV",
        File::getSpecialLocation(File::userMusicDirectory).getChildFile(defaultName),
        "*.wav");

    chooser->launchAsync(
        FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
        [this, patternIndex, chooser](const FileChooser& fc) {
            auto dest = fc.getResult();
            if (dest == File()) return;

            auto& pat2 = mPM.getPattern(patternIndex);
            constexpr double kSR  = 44100.0;
            constexpr int    kBlk = 512;
            const double tailSec  = 2.0;
            const double renderBpm = jmax (1.0, mPM.getGlobalTempo());
            double durSec         = pat2.bars * 4.0 * (60.0 / renderBpm) + tailSec;
            int totalSamples      = (int)(durSec * kSR);

            VibeSynthProcessor renderProc;
            renderProc.setPatternManager(&mPM);
            renderProc.prepareToPlay(kSR, kBlk);
            { MemoryBlock state; mProcessor.getStateInformation(state);
              renderProc.setStateInformation(state.getData(), (int)state.getSize()); }
            mPM.setCurrentPattern(patternIndex);

            struct OfflineHead : public AudioPlayHead {
                double ppq{0.0}, bpm, sr;
                OfflineHead(double b2, double s) : bpm(b2), sr(s) {}
                Optional<PositionInfo> getPosition() const override {
                    PositionInfo pi; pi.setBpm(bpm); pi.setPpqPosition(ppq);
                    pi.setIsPlaying(true); pi.setIsRecording(false);
                    pi.setTimeInSeconds(ppq * 60.0 / jmax(1.0, bpm)); return pi;
                }
                void advance(int n) { ppq += (n / sr) * (bpm / 60.0); }
            } head(renderBpm, kSR);
            renderProc.setPlayHead(&head);

            WavAudioFormat fmt;
            auto os = dest.createOutputStream();
            if (!os) return;
            std::unique_ptr<AudioFormatWriter> writer(
                fmt.createWriterFor(os.release(), kSR, 2, 24, {}, 0));
            if (!writer) return;

            AudioBuffer<float> buf(2, kBlk);
            MidiBuffer midi;
            int remaining = totalSamples;
            while (remaining > 0) {
                int chunk = jmin(kBlk, remaining);
                buf.setSize(2, chunk, false, false, true); buf.clear(); midi.clear();
                renderProc.processBlock(buf, midi);
                head.advance(chunk);
                writer->writeFromAudioSampleBuffer(buf, 0, chunk);
                remaining -= chunk;
            }
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// BuilderMenuBar
// ─────────────────────────────────────────────────────────────────────────────
juce::StringArray BuilderMenuBar::getMenuBarNames()
{
    return { "Edit", "Tools", "Clips", "View" };
}

juce::PopupMenu BuilderMenuBar::getMenuForIndex(int index, const juce::String&)
{
    using AGTool = ArrangementGrid::AGTool;
    PopupMenu m;

    if (index == 0)  // Edit
    {
        bool canUndo = mOwner.mGrid && mOwner.mGrid->canUndo();
        bool canRedo = mOwner.mGrid && mOwner.mGrid->canRedo();
        m.addItem(1, "Undo",  canUndo);
        m.addItem(2, "Redo",  canRedo);
        m.addSeparator();
        m.addItem(3, "Select All\tCtrl+A");
        m.addItem(4, "Deselect\tEsc");
        m.addSeparator();
        m.addItem(5, "Copy\tCtrl+C");
        m.addItem(6, "Paste\tCtrl+V");
        m.addItem(7, "Delete\tDel");
        m.addItem(8, "Duplicate\tCtrl+B");
    }
    else if (index == 1)  // Tools
    {
        auto cur = mOwner.mGrid ? mOwner.mGrid->getTool() : AGTool::Draw;
        m.addItem(21, "Draw\tP",          true, cur == AGTool::Draw);
        m.addItem(22, "Paint\tB",         true, cur == AGTool::Paint);
        m.addItem(23, "Select\tE",        true, cur == AGTool::Select);
        m.addItem(24, "Delete\tD",        true, cur == AGTool::Delete);
        m.addItem(25, "Mute\tT",          true, cur == AGTool::Mute);
        // QA-Ea Task 0c (2026-05-20): "Slip Edit\tS" tool menu item removed.
        // Slip is now an edge-drag mode (the toolbar Slip/Stretch dropdown),
        // not a tool selection.  Item id 26 is intentionally retired.
        m.addItem(27, "Slice\tC",         true, cur == AGTool::Slice);
        m.addItem(28, "Zoom\tZ",          true, cur == AGTool::Zoom);
        m.addItem(29, "Play Selected\tY", true, cur == AGTool::PlaySelected);
    }
    else if (index == 2)  // Clips
    {
        m.addItem(41, "Import Audio...");
        m.addSeparator();
        m.addItem(42, "Rename Pattern\tF2");
        m.addItem(43, "Find Next Empty\tF4");
        m.addItem(44, "New Automation Clip...");
        m.addSeparator();
        m.addItem(45, "Render Pattern to WAV...");
    }
    else if (index == 3)  // View
    {
        m.addItem(51, "Zoom In\t+");
        m.addItem(52, "Zoom Out\t-");
        m.addSeparator();
        m.addItem(53, "Toggle Browser");
        m.addItem(54, "Performance Mode\tCtrl+P", true, mOwner.mPerfMode);
    }

    return m;
}

void BuilderMenuBar::menuItemSelected(int itemId, int /*topLevelIndex*/)
{
    using AGTool = ArrangementGrid::AGTool;
    auto& o = mOwner;

    switch (itemId)
    {
        // Edit
        case 1: if (o.mGrid) o.mGrid->undo();                           break;
        case 2: if (o.mGrid) o.mGrid->redo();                           break;
        case 3: if (o.mGrid) o.mGrid->keyPressed(KeyPress('a', ModifierKeys::ctrlModifier, 0)); break;
        case 4: if (o.mGrid) o.mGrid->keyPressed(KeyPress(KeyPress::escapeKey)); break;
        case 5: if (o.mGrid) o.mGrid->keyPressed(KeyPress('c', ModifierKeys::ctrlModifier, 0)); break;
        case 6: if (o.mGrid) o.mGrid->keyPressed(KeyPress('v', ModifierKeys::ctrlModifier, 0)); break;
        case 7: if (o.mGrid) o.mGrid->keyPressed(KeyPress(KeyPress::deleteKey)); break;
        case 8: if (o.mGrid) o.mGrid->keyPressed(KeyPress('b', ModifierKeys::ctrlModifier, 0)); break;

        // Tools
        case 21: if (o.mGrid) o.mGrid->setTool(AGTool::Draw);         break;
        case 22: if (o.mGrid) o.mGrid->setTool(AGTool::Paint);        break;
        case 23: if (o.mGrid) o.mGrid->setTool(AGTool::Select);       break;
        case 24: if (o.mGrid) o.mGrid->setTool(AGTool::Delete);       break;
        case 25: if (o.mGrid) o.mGrid->setTool(AGTool::Mute);         break;
        // QA-Ea Task 0c (2026-05-20): case 26 (Slip Edit tool) removed.
        case 27: if (o.mGrid) o.mGrid->setTool(AGTool::Slice);        break;
        case 28: if (o.mGrid) o.mGrid->setTool(AGTool::Zoom);         break;
        case 29: if (o.mGrid) o.mGrid->setTool(AGTool::PlaySelected); break;

        // Clips
        case 41: o.doImportAudio();           break;
        case 42: o.doRenamePattern();         break;
        case 43: o.doFindNextEmptyPattern();  break;
        case 44: o.doNewAutomationClip();     break;
        case 45: if (o.mGrid && o.mPM.getNumPatterns() > 0)
                     o.renderPatternToWav(o.mPM.getCurrentPatternIndex()); break;

        // View
        case 51: o.doZoom(1.25f);            break;
        case 52: o.doZoom(1.f / 1.25f);      break;
        case 53: o.doToggleBrowser();        break;
        case 54: o.doPerformanceModeToggle(); break;

        default: break;
    }
}
