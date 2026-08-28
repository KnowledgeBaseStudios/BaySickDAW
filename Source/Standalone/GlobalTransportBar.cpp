#include "GlobalTransportBar.h"
#include "ShotMenuHook.h"
#include "../TsMapRead.h"   // QA-G Task 6: marker-map readout in song mode
#include "../DSP/AudioClipStreamer.h"   // CL-282: streaming telemetry readout
#include "SharedUI.h"
#if JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
 #pragma comment(lib, "psapi.lib")
#endif

// ─────────────────────────────────────────────────────────────────────────────
// PlayButton (R5a, 2026-04-23)
// White play-triangle glyph on a recessed dark body.  When `playing` is true
// the body fills green; the white triangle stays in place.  Pure path render -
// no font dependency, no color-button tinting (which inverts on hover and
// looked grimy at small sizes).
// ─────────────────────────────────────────────────────────────────────────────
class PlayButton : public juce::TextButton
{
public:
    PlayButton() : juce::TextButton("")
    {
        setMouseClickGrabsKeyboardFocus(false);
        // LAF reads buttonColourId for off + buttonOnColourId when toggled on.
        // Green-on when playing; off state inherits the LAF default so it
        // matches every other transport TextButton.
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1ea848));
        setClickingTogglesState(false);   // we drive the state manually
    }
    void setPlaying(bool p)
    {
        if (p == getToggleState()) return;
        setToggleState(p, juce::dontSendNotification);
        repaint();
    }

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
    {
        if (getToggleState())
            paintActiveTintedSlab(g, getLocalBounds().toFloat(),
                                   juce::Colour(0xff1ea848), isOver);    // green
        else
            getLookAndFeel().drawButtonBackground(g, *this,
                findColour(juce::TextButton::buttonColourId), isOver, isDown);

        // White play triangle on top
        auto b = getLocalBounds().toFloat();
        const float side = juce::jmin(b.getWidth(), b.getHeight()) * 0.50f;
        const float cx   = b.getCentreX();
        const float cy   = b.getCentreY();
        juce::Path tri;
        tri.addTriangle(cx - side * 0.40f, cy - side * 0.55f,
                        cx - side * 0.40f, cy + side * 0.55f,
                        cx + side * 0.55f, cy);
        g.setColour(juce::Colours::white);
        g.fillPath(tri);
    }

    // Mirrors BaySickLAF's LRX-12 "action button" raised-slab shape, but with a
    // tinted backdrop instead of VC::Chrome.  Used by the active states of
    // both PlayButton (green) and RecordButton (red) so they match the rest
    // of the transport row in every respect except colour.
    static void paintActiveTintedSlab(juce::Graphics& g,
                                       juce::Rectangle<float> bounds,
                                       juce::Colour tint,
                                       bool isOver)
    {
        const float radius = 3.f;
        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(bounds.translated(0.f, 1.5f), radius);
        // Body gradient (tint brighter on top, darker on bottom)
        juce::Colour topCol = isOver ? tint.brighter(0.15f) : tint.brighter(0.05f);
        juce::Colour botCol = tint.darker(0.20f);
        g.setGradientFill(juce::ColourGradient(topCol, 0.f, bounds.getY(),
                                                botCol, 0.f, bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, radius);
        // Side highlight
        juce::ColourGradient sideHL(juce::Colours::white.withAlpha(0.22f), bounds.getX(), 0.f,
                                     juce::Colours::transparentWhite,       bounds.getX() + 3.f, 0.f, false);
        g.setGradientFill(sideHL);
        g.fillRoundedRectangle(bounds, radius);
        // Top specular
        g.setColour(juce::Colour(0x44ffffff));
        g.drawLine(bounds.getX() + radius, bounds.getY() + 1.f,
                   bounds.getRight() - radius, bounds.getY() + 1.f, 1.f);
        // Border
        g.setColour(tint.darker(0.4f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.f);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// RecordButton (R5a, 2026-04-23)
// White dot glyph on a recessed dark body.  When `armed` is true the body
// fills red; the white dot stays in the centre.  A small chevron on the right
// edge acts as the dropdown trigger - clicks within `kArrowZoneW` from the
// right edge fire onArrowClicked, anything else fires onDotClicked.
// ─────────────────────────────────────────────────────────────────────────────
class RecordButton : public juce::TextButton
{
public:
    static constexpr int kArrowZoneW = 14;

    RecordButton() : juce::TextButton("")
    {
        setMouseClickGrabsKeyboardFocus(false);
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd02020));
        setClickingTogglesState(false);
    }

    void setArmed(bool a)
    {
        if (a == getToggleState()) return;
        setToggleState(a, juce::dontSendNotification);
        repaint();
    }
    bool isArmed() const noexcept { return getToggleState(); }

    std::function<void()> onDotClicked;
    std::function<void()> onArrowClicked;

    void clicked(const juce::ModifierKeys&) override
    {
        if (mLastClickInArrow) { if (onArrowClicked) onArrowClicked(); }
        else                   { if (onDotClicked)   onDotClicked();   }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        mLastClickInArrow = (e.x >= getWidth() - kArrowZoneW);
        juce::TextButton::mouseDown(e);
    }

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
    {
        if (getToggleState())
            PlayButton::paintActiveTintedSlab(
                g, getLocalBounds().toFloat(),
                juce::Colour(0xffd02020), isOver);              // red when armed
        else
            getLookAndFeel().drawButtonBackground(g, *this,
                findColour(juce::TextButton::buttonColourId), isOver, isDown);

        auto b = getLocalBounds().toFloat();

        // White dot - centred in the dot zone (left of the arrow strip)
        auto dotZone = b.withTrimmedRight((float) kArrowZoneW);
        const float dotR = juce::jmin(dotZone.getWidth(), dotZone.getHeight()) * 0.30f;
        const float cx   = dotZone.getCentreX();
        const float cy   = dotZone.getCentreY();
        g.setColour(juce::Colours::white);
        g.fillEllipse(cx - dotR, cy - dotR, dotR * 2.f, dotR * 2.f);

        // Divider line + chevron on the right
        const float ax = b.getRight() - kArrowZoneW;
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawLine(ax, b.getY() + 3.f, ax, b.getBottom() - 3.f, 1.f);

        const float acx = ax + (kArrowZoneW * 0.5f);
        const float acy = b.getCentreY();
        juce::Path chev;
        chev.addTriangle(acx - 3.5f, acy - 1.5f,
                         acx + 3.5f, acy - 1.5f,
                         acx,        acy + 3.0f);
        g.setColour(juce::Colours::white);
        g.fillPath(chev);
    }

private:
    bool mLastClickInArrow{ false };
};

// ─────────────────────────────────────────────────────────────────────────────
// MetronomeButton (D-5 polish, 2026-04-26)
// Custom-painted icon - classic mechanical metronome shape (trapezoidal body
// + vertical pendulum + small weight).  Replaces the prior "METRO" text
// label.  Editor drives toggle state manually (matches Play/Record pattern).
// ─────────────────────────────────────────────────────────────────────────────
class MetronomeButton : public juce::TextButton
{
public:
    MetronomeButton() : juce::TextButton("")
    {
        setMouseClickGrabsKeyboardFocus(false);
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd8a520));   // amber
        setClickingTogglesState(false);   // editor drives state
    }

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
    {
        // Same raised-slab styling Play/Record use when active.
        if (getToggleState())
            PlayButton::paintActiveTintedSlab(g, getLocalBounds().toFloat(),
                                               juce::Colour(0xffd8a520), isOver);
        else
            getLookAndFeel().drawButtonBackground(g, *this,
                findColour(juce::TextButton::buttonColourId), isOver, isDown);

        // 2026-04-26: smaller absolute-sized symbol so the button can sit
        // tight around it (~4 px padding on each side) and leave room for
        // the keyboard-MIDI button (D-4) to slot in next to the metronome.
        auto b = getLocalBounds().toFloat();
        const float cx       = b.getCentreX();
        const float cy       = b.getCentreY();
        const float bodyTopW = 12.0f;   // narrower top
        const float bodyBotW = 22.0f;   // narrower base (was ~33)
        const float bodyH    = juce::jmin(b.getHeight() - 4.f, 16.0f);
        const float topY     = cy - bodyH * 0.5f;
        const float botY     = cy + bodyH * 0.5f;

        juce::Path body;
        body.startNewSubPath(cx - bodyTopW * 0.5f, topY);
        body.lineTo         (cx + bodyTopW * 0.5f, topY);
        body.lineTo         (cx + bodyBotW * 0.5f, botY);
        body.lineTo         (cx - bodyBotW * 0.5f, botY);
        body.closeSubPath();

        const juce::Colour fg = getToggleState() ? juce::Colours::white : VC::Text;
        g.setColour(fg);
        g.fillPath(body);

        // Pendulum + weight (drawn inset over the body).
        const juce::Colour inner = getToggleState() ? juce::Colour(0xff222222) : VC::Bg;
        g.setColour(inner);
        g.drawLine(cx, topY + 1.5f, cx, botY - 2.f, 1.3f);
        g.fillEllipse(cx - 1.5f, topY + bodyH * 0.22f, 3.f, 3.f);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MetroArrowButton (D-5 polish, 2026-04-26)
// Custom-painted downward chevron, matching the ▾ glyph used on ribbon tabs.
// Was a TextButton with the UTF-8 ▾ as its label, but the rendered glyph was
// invisible at the available font size - this paints the path explicitly.
// ─────────────────────────────────────────────────────────────────────────────
class MetroArrowButton : public juce::TextButton
{
public:
    MetroArrowButton() : juce::TextButton("")
    {
        setMouseClickGrabsKeyboardFocus(false);
        setClickingTogglesState(false);
    }

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
    {
        getLookAndFeel().drawButtonBackground(g, *this,
            findColour(juce::TextButton::buttonColourId), isOver, isDown);

        // Filled triangle pointing down - same convention as the ribbon tabs.
        auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX();
        const float cy = b.getCentreY();
        const float w  = 5.f;
        const float h  = 4.f;
        juce::Path tri;
        tri.addTriangle(cx - w, cy - h * 0.5f,
                        cx + w, cy - h * 0.5f,
                        cx,     cy + h * 0.5f);
        g.setColour(VC::Text.withAlpha(0.85f));
        g.fillPath(tri);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// LoopModeButton - custom-painted toggle for play-through (→|) vs loop (↻).
// Default font lacks the unicode glyphs we'd want, so we paint icons directly.
// ─────────────────────────────────────────────────────────────────────────────
class LoopModeButton : public juce::TextButton
{
public:
    LoopModeButton()
    {
        setClickingTogglesState(true);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void paintButton(juce::Graphics& g, bool /*hover*/, bool /*down*/) override
    {
        auto b   = getLocalBounds().toFloat().reduced(1.f);
        bool on  = getToggleState();
        // Background - match other transport buttons' look
        g.setColour(on ? VC::Purple.withAlpha(0.5f) : VC::Accent);
        g.fillRoundedRectangle(b, 3.f);
        g.setColour(juce::Colour(0xff333333));
        g.drawRoundedRectangle(b, 3.f, 1.f);

        auto inner = b.reduced(4.f);
        g.setColour(on ? juce::Colours::white : VC::TextDim);

        if (on)
        {
            // Loop icon: 3/4 circular arc with arrowhead
            const float cx = inner.getCentreX();
            const float cy = inner.getCentreY();
            const float r  = juce::jmin(inner.getWidth(), inner.getHeight()) * 0.40f;

            juce::Path arc;
            // Arc from ~30° to ~330° (270° sweep) - leaves a gap for the arrow
            arc.addCentredArc(cx, cy, r, r, 0.f,
                              juce::degreesToRadians(30.f),
                              juce::degreesToRadians(330.f),
                              true);
            g.strokePath(arc, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            // Arrowhead at the arc end (top-right)
            const float endA = juce::degreesToRadians(330.f);
            const float ex = cx + r * std::sin(endA);
            const float ey = cy - r * std::cos(endA);
            juce::Path head;
            head.addTriangle(ex,             ey,
                             ex + 4.5f,      ey - 1.f,
                             ex + 1.5f,      ey - 5.f);
            g.fillPath(head);
        }
        else
        {
            // Play-through-then-stop icon: arrow → end-bar (→|)
            const float midY  = inner.getCentreY();
            const float left  = inner.getX() + 1.f;
            const float right = inner.getRight() - 3.f;
            const float headW = 4.f;
            const float barX  = right;
            // Shaft
            g.drawLine(left, midY, barX - headW, midY, 1.6f);
            // Arrowhead
            juce::Path head;
            head.addTriangle(barX - headW, midY - 3.f,
                             barX - headW, midY + 3.f,
                             barX,         midY);
            g.fillPath(head);
            // End bar (the "stop" line after the arrow)
            g.drawLine(barX + 1.f, midY - 4.5f,
                       barX + 1.f, midY + 4.5f, 1.6f);
        }
    }
};

// D-4 (QA-TransportDisplay 2026-07-08): typing-keyboard MIDI toggle - a row of
// piano keys, amber-lit while the mode is on.  Occupies the bar slot that was
// reserved next to the metronome.  setClickingTogglesState stays OFF because
// StandaloneEditor owns the mode state (Ctrl+T is the other entry point) and
// pushes the visual back via GlobalTransportBar::setTypingKeyboardOn.
class KeyboardMidiButton : public juce::Button
{
public:
    KeyboardMidiButton() : juce::Button ("KeybMidi")
    {
        setMouseClickGrabsKeyboardFocus (false);
        setClickingTogglesState (false);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool /*down*/) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();

        g.setColour (on ? juce::Colour (0xff3A2F18)
                        : (highlighted ? VC::Accent.brighter (0.15f) : VC::Accent));
        g.fillRoundedRectangle (b, 4.0f);

        auto keys = b.reduced (5.0f, 6.0f);
        const float kw = keys.getWidth() / 4.0f;
        g.setColour (on ? juce::Colour (0xffFFB030) : juce::Colours::white.withAlpha (0.75f));
        for (int i = 0; i < 4; ++i)
            g.fillRect (juce::Rectangle<float> (keys.getX() + i * kw, keys.getY(),
                                                kw - 1.0f, keys.getHeight()));
        g.setColour (juce::Colour (0xff141414));
        for (int i = 1; i < 4; ++i)
            g.fillRect (juce::Rectangle<float> (keys.getX() + i * kw - kw * 0.30f, keys.getY(),
                                                kw * 0.55f, keys.getHeight() * 0.60f));
    }
};

GlobalTransportBar::GlobalTransportBar(StandalonePlayHead& ph)
    : mPlayHead(ph)
{
    auto makeBtn = [this](const juce::String& text, bool isToggle = false)
    {
        auto b = std::make_unique<juce::TextButton>(text);
        b->setClickingTogglesState(isToggle);
        b->setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(*b);
        return b;
    };

    // R5a: custom-painted Play (white triangle, green when playing).
    {
        auto p = std::make_unique<PlayButton>();
        p->setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(*p);
        mPlayBtn.reset(p.release());
    }
    mPauseBtn    = makeBtn(juce::CharPointer_UTF8("\xe2\x80\x96")); // ‖
    mStopBtn     = makeBtn(juce::CharPointer_UTF8("\xe2\x96\xa0")); // ■
    // 2026-04-26 (D-5 polish): icon-painted metronome button replaces "METRO" text.
    {
        auto m = std::make_unique<MetronomeButton>();
        addAndMakeVisible(*m);
        mMetronomeBtn.reset(m.release());
    }

    // 2026-04-26 (D-5 polish): custom path-painted chevron - the previous
    // text-button rendered the UTF-8 ▾ at an invisible size.
    {
        auto a = std::make_unique<MetroArrowButton>();
        addAndMakeVisible(*a);
        mMetroArrowBtn.reset(a.release());
    }
    mMetroArrowBtn->setTooltip("Metronome settings");
    mMetroArrowBtn->onClick = [this] { if (onMetroSettings) onMetroSettings(); };

    mSongModeBtn = makeBtn("PATTERN", false);

    // Song-loop-mode toggle: custom-painted icons (→| for play-through, ↻ for loop).
    // Default font lacks the unicode arrows, so we draw with juce::Path.
    mSongLoopBtn = std::make_unique<LoopModeButton>();
    mSongLoopBtn->setMouseClickGrabsKeyboardFocus(false);
    mSongLoopBtn->setTooltip("Song playback: arrow + bar = play to end and stop / circle = loop");
    // 2026-04-26: default to loop (🔁) - was play-through (→).  Pairs with
    // the matching default in `BaySickDAWProcessor::mSongLoopMode`.
    mSongLoopBtn->setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(*mSongLoopBtn);

    // D-4: typing-keyboard MIDI toggle in the reserved slot next to the metronome.
    {
        auto k = std::make_unique<KeyboardMidiButton>();
        k->setTooltip ("Play notes with your computer keyboard (Ctrl+T)");
        k->onClick = [this] { if (onTypingKeyboardToggle) onTypingKeyboardToggle(); };
        addAndMakeVisible (*k);
        mKeybMidiBtn.reset (k.release());
    }

    // QA-G3Smoke SW-1: global Swing knob (between the typing-keyboard toggle
    // and the pattern dropdown).  0-100%, default 0, double-click resets to 0;
    // per-player Swing Mix knobs scale this on each title bar (SW-3).
    {
        auto s = std::make_unique<BaySickSlider>();
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s->setRange (0.0, 1.0, 0.0);
        s->setDoubleClickReturnValue (true, 0.0);
        s->setTooltip ("Swing - pushes every second 16th step late.  Per-player Swing Mix knobs scale it.");
        // Smoke #59/60: value popup on hover/drag (mixer pan/width convention).
        s->setPopupDisplayEnabled (true, true, nullptr);
        s->textFromValueFunction = [] (double v)
        { return "Swing " + juce::String ((int) std::lround (v * 100.0)) + "%"; };
        s->onValueChange = [this]
        {
            if (onSetSwing) onSetSwing ((float) mSwingKnob->getValue());
        };
        addAndMakeVisible (*s);
        mSwingKnob = std::move (s);
    }

    mPlayBtn->setTooltip("Play  (Space)");
    mPauseBtn->setTooltip("Pause  (Space while playing)");
    mStopBtn->setTooltip("Stop  (Shift+Space)");
    mMetronomeBtn->setTooltip("Toggle metronome");
    mSongModeBtn->setTooltip("Toggle Pattern / Song playback mode");

    // R5a: custom-painted Record button (white dot, red body when armed, +
    // dropdown chevron on the right for the upcoming ASIO / MIDI mode menu).
    {
        auto r = std::make_unique<RecordButton>();
        r->setMouseClickGrabsKeyboardFocus(false);
        r->setTooltip("Arm recording (R) - press Play to start; click chevron for ASIO / MIDI mode");
        r->onDotClicked = [this] {
            mIsRecording = !mIsRecording;
            if (auto* rb = dynamic_cast<RecordButton*>(mRecBtn.get())) rb->setArmed(mIsRecording);
            if (onRecord) onRecord(mIsRecording);
        };
        r->onArrowClicked = [this] {
            // R5c (2026-04-24): ASIO / MIDI mode picker.  Label on MIDI spells
            // out "(piano roll tabs only)" so beginners know the mode is
            // strictly for capturing notes into whatever piano-roll page is
            // open - not a general MIDI input router.
            juce::PopupMenu m;
            const auto cur = mRecordMode;
            m.addItem (1, "ASIO",                       true, cur == RecordMode::Audio);
            m.addItem (2, "MIDI (piano roll tabs only)", true, cur == RecordMode::Midi);

            // QA-Ea Task 0c (FL pre-roll record): "Global Record-Quantize"
            // submenu (added 2026-05-19).  Snap captured MIDI startBeats to
            // the chosen grid divisor at commit time (after the FL Early-
            // Strike clamp).  Off = no snap, raw timing kept.  Owner of the
            // underlying APVTS param lives in StandaloneEditor; this menu
            // reads + writes via the onGetRecordQuantizeDiv /
            // onRecordQuantizeDivChanged callbacks.
            // QA-Ee Stage 4: 11-label unified snap scheme (ids 100..110), built
            // from the shared kUnifiedSnapLabels so this picker stays identical
            // to the Builder + Piano Roll snap menus.  "Line" appears for parity
            // but is a no-snap no-op here (no zoom grid at record-commit time).
            const int curQ = onGetRecordQuantizeDiv ? onGetRecordQuantizeDiv() : 0;
            juce::PopupMenu qSub;
            for (int i = 0; i < kNumUnifiedSnapDivs; ++i)
                qSub.addItem (100 + i, kUnifiedSnapLabels[i], true, curQ == i);
            m.addSeparator();
            m.addSubMenu ("Global Record-Quantize", qSub);

            if (shots::maybeCapture (m)) return;
            m.showMenuAsync (
                juce::PopupMenu::Options().withTargetComponent (mRecBtn.get()),
                [this](int r) {
                    if      (r == 1) setRecordMode (RecordMode::Audio);
                    else if (r == 2) setRecordMode (RecordMode::Midi);
                    else if (r >= 100 && r < 100 + kNumUnifiedSnapDivs && onRecordQuantizeDivChanged)
                        onRecordQuantizeDivChanged (r - 100);
                });
        };
        addAndMakeVisible(*r);
        mRecBtn.reset(r.release());
    }

    mPlayBtn->onClick = [this] { if (onPlay) onPlay(); };
    mPauseBtn->onClick = [this] { if (onPause) onPause(); };
    mStopBtn->onClick  = [this] {
        mIsRecording = false;
        if (auto* rb = dynamic_cast<RecordButton*>(mRecBtn.get())) rb->setArmed(false);
        if (onRecord) onRecord(false);
        if (onStop) onStop();
    };

    mMetronomeBtn->onClick = [this] {
        bool on = !mMetronomeBtn->getToggleState();
        mMetronomeBtn->setToggleState(on, juce::dontSendNotification);
        if (onMetronomeToggle) onMetronomeToggle(on);
    };
    mSongModeBtn->onClick = [this] {
        bool songMode = !mSongModeBtn->getToggleState();
        mSongModeBtn->setToggleState(songMode, juce::dontSendNotification);
        mSongModeBtn->setButtonText(songMode ? "SONG" : "PATTERN");
        if (onSongModeChanged) onSongModeChanged(songMode);
    };

    mSongLoopBtn->onClick = [this] {
        // setClickingTogglesState(true) in LoopModeButton already flipped the
        // state by the time onClick fires - just read the new state and
        // notify. Don't flip again or the two flips will cancel out and the
        // button will appear inert.
        const bool loop = mSongLoopBtn->getToggleState();
        mSongLoopBtn->repaint();
        if (onSongLoopModeChanged) onSongLoopModeChanged(loop);
    };

    // BPM
    mBpmLabel = std::make_unique<juce::Label>();
    mBpmLabel->setText("BPM", juce::dontSendNotification);
    mBpmLabel->setFont(juce::Font(9.0f));
    mBpmLabel->setColour(juce::Label::textColourId, VC::TextDim);
    mBpmLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*mBpmLabel);

    mBpmField = std::make_unique<juce::TextEditor>();
    mBpmField->setText("120", false);
    mBpmField->setInputRestrictions(6, "0123456789.");
    mBpmField->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
    mBpmField->setJustification(juce::Justification::centred);
    mBpmField->setTooltip("Tempo (BPM)");
    // LRX-14: LCD amber digits on dark recessed background
    mBpmField->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0A0A08));
    mBpmField->setColour(juce::TextEditor::textColourId,       juce::Colour(0xffFFB030));
    mBpmField->setColour(juce::TextEditor::outlineColourId,    juce::Colour(0xff2A2A28));
    mBpmField->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff604020));
    mBpmField->onReturnKey = [this] {
        if (onTempoChanged) onTempoChanged(getBPM());
        mBpmField->giveAwayKeyboardFocus();
    };
    mBpmField->onFocusLost = [this] {
        if (onTempoChanged) onTempoChanged(getBPM());
    };
    // 2026-04-24: right-click the BPM field -> "Automate tempo".  Override
    // the built-in copy/paste popup and show our own menu via a persistent
    // MouseListener member.
    mBpmField->setPopupMenuEnabled (false);
    mBpmField->addMouseListener (&mBpmMouseWatcher, true);
    addAndMakeVisible(*mBpmField);

    // Tap Tempo button
    mTapBtn = makeBtn("TAP");
    mTapBtn->setTooltip("Tap to set tempo - tap repeatedly then it locks in");
    mTapBtn->onClick = [this] { doTap(); };

    mPerfReadout = std::make_unique<TransportPerfReadout>();
    mPerfReadout->update ("SYS --%", "DSP --%", "MEM --  LAT --", "UND --  PF --",
                          VC::TextDim, VC::TextDim);
    // Tooltip is owned by the readout now (live values + the legend), rebuilt
    // whenever a value changes -- see TransportPerfReadout::refreshTooltip.
    addAndMakeVisible(*mPerfReadout);

    startTimerHz(10);
}

void GlobalTransportBar::BpmMouseWatcher::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown()) return;
    juce::PopupMenu m;
    m.addItem ("Automate tempo", [this] {
        if (owner.onAutomateTempo) owner.onAutomateTempo();
    });
    m.showMenuAsync (juce::PopupMenu::Options()
                       .withTargetComponent (owner.mBpmField.get()));
}

GlobalTransportBar::~GlobalTransportBar()
{
    stopTimer();
}

double GlobalTransportBar::getBPM() const
{
    // 2026-04-30: clamp to range instead of snapping to 120.  Old behaviour
    // silently dropped a typed value of 19 or 301 to 120 with no feedback -
    // user couldn't tell whether their input was accepted.  Clamp keeps the
    // intent (limit value to musical range) but preserves the closest legal
    // value rather than reverting to default.
    const double v = mBpmField->getText().getDoubleValue();
    return juce::jlimit (20.0, 300.0, v);
}

void GlobalTransportBar::setPlayState(bool playing, bool paused)
{
    mIsPlaying = playing;
    mIsPaused  = paused;
}

void GlobalTransportBar::showRecordMenuForShot()
{
    if (auto* rb = dynamic_cast<RecordButton*> (mRecBtn.get()))
        if (rb->onArrowClicked) rb->onArrowClicked();
}

void GlobalTransportBar::setRecordArmed(bool armed)
{
    mIsRecording = armed;
    if (auto* rb = dynamic_cast<RecordButton*>(mRecBtn.get())) rb->setArmed(armed);
}

void GlobalTransportBar::setRecordMode(RecordMode m)
{
    if (m == mRecordMode) return;
    mRecordMode = m;
    if (mRecBtn)
        mRecBtn->setTooltip (juce::String ("Arm recording (R) - mode: ")
                              + (m == RecordMode::Audio
                                 ? "ASIO"
                                 : "MIDI (piano roll tabs only)"));
}

void GlobalTransportBar::togglePlayPause()
{
    if (mIsPlaying && ! mIsPaused)
    { if (onPause) onPause(); }
    else
    { if (onPlay)  onPlay();  }
}

void GlobalTransportBar::stopAndDisarm()
{
    mIsRecording = false;
    if (auto* rb = dynamic_cast<RecordButton*>(mRecBtn.get())) rb->setArmed(false);
    if (onRecord) onRecord(false);
    if (onStop)   onStop();
}

void GlobalTransportBar::toggleRecord()
{
    mIsRecording = ! mIsRecording;
    if (auto* rb = dynamic_cast<RecordButton*>(mRecBtn.get())) rb->setArmed(mIsRecording);
    if (onRecord) onRecord(mIsRecording);
}

void GlobalTransportBar::toggleMetronome()
{
    if (mMetronomeBtn == nullptr) return;
    const bool on = ! mMetronomeBtn->getToggleState();
    // dontSendNotification - the button's own onClick also toggles + fires
    // onMetronomeToggle, so sendNotification would double-toggle and leave the
    // visual state out of sync with the listener payload.
    mMetronomeBtn->setToggleState(on, juce::dontSendNotification);
    if (onMetronomeToggle) onMetronomeToggle(on);
}

void GlobalTransportBar::toggleSongMode()
{
    setSongMode(! isSongMode());   // setSongMode already fires onSongModeChanged
}

void GlobalTransportBar::timerCallback()
{
    // Keep playhead loop wrap point in sync with the current pattern content
    if (onGetLoopBeats)
        mPlayHead.setLoopBeats(onGetLoopBeats());

    // C.5: keep playhead time-signature in sync with current beat's TS lookup.
    if (onGetTimeSig)
    {
        int n = 4, d = 4;
        onGetTimeSig (n, d);
        mPlayHead.setTimeSignature (n, d);
    }

    bool playing = mPlayHead.isPlaying();

    // R5a: Play + Record paint themselves from internal state flags.  No more
    // per-tick colour fights with the LookAndFeel.
    if (auto* pb = dynamic_cast<PlayButton*>(mPlayBtn.get())) pb->setPlaying(playing);
    if (auto* rb = dynamic_cast<RecordButton*>(mRecBtn.get())) rb->setArmed(mIsRecording);

    // Pause button colour
    mPauseBtn->setColour(juce::TextButton::buttonColourId,
        mIsPaused ? VC::Yellow.withAlpha(0.45f) : VC::Accent);

    // Stop button
    mStopBtn->setColour(juce::TextButton::buttonColourId,
        playing ? VC::Highlight.withAlpha(0.45f) : VC::Accent);

    // 2026-04-30: re-sync visual toggle state from processor truth (after
    // project load, atomics may have changed without the buttons knowing).
    if (onGetSongLoopMode && mSongLoopBtn)
    {
        const bool want = onGetSongLoopMode();
        if (mSongLoopBtn->getToggleState() != want)
            mSongLoopBtn->setToggleState (want, juce::dontSendNotification);
    }
    if (onGetMetronomeEnabled && mMetronomeBtn)
    {
        const bool want = onGetMetronomeEnabled();
        if (mMetronomeBtn->getToggleState() != want)
            mMetronomeBtn->setToggleState (want, juce::dontSendNotification);
    }

    // Metronome button + arrow
    mMetronomeBtn->setColour(juce::TextButton::buttonOnColourId,  VC::Blue.withAlpha(0.5f));
    mMetronomeBtn->setColour(juce::TextButton::buttonColourId,    VC::Accent);
    if (mMetroArrowBtn)
    {
        mMetroArrowBtn->setColour(juce::TextButton::buttonColourId, VC::Accent);
        mMetroArrowBtn->setColour(juce::TextButton::textColourOffId, VC::TextDim);
    }

    // Song/Pattern mode button
    mSongModeBtn->setColour(juce::TextButton::buttonOnColourId,   VC::Purple.withAlpha(0.5f));
    mSongModeBtn->setColour(juce::TextButton::buttonColourId,     VC::Accent);

    // CPU + DSP load readout
    {
       #if JUCE_WINDOWS
        float sysCpu = 0.f;
        int   memMb  = 0;
        {
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
                memMb = (int)(pmc.WorkingSetSize / (1024 * 1024));
        }
        {
            static ULONGLONG lastIdle = 0, lastKernel = 0, lastUser = 0;
            FILETIME idleT, kernelT, userT;
            if (GetSystemTimes(&idleT, &kernelT, &userT))
            {
                auto toU = [](FILETIME ft) -> ULONGLONG {
                    return (ULONGLONG(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
                };
                ULONGLONG idle    = toU(idleT);
                ULONGLONG kernel  = toU(kernelT);
                ULONGLONG user    = toU(userT);
                ULONGLONG dKernel = kernel - lastKernel;
                ULONGLONG dUser   = user   - lastUser;
                ULONGLONG dIdle   = idle   - lastIdle;
                ULONGLONG total   = dKernel + dUser;
                if (total > 0)
                    sysCpu = 100.f * float(total - dIdle) / float(total);
                lastIdle = idle; lastKernel = kernel; lastUser = user;
            }
        }
       #else
        float sysCpu = 0.f;
        int   memMb  = 0;
       #endif

        // 1M: read audio DSP load from processor (via callbacks set by StandaloneEditor)
        const float dspLoad   = onGetDspLoad ? onGetDspLoad() : 0.f;
        const bool  over95    = onGetDsp95   ? onGetDsp95()   : false;
        const int   dspPct    = juce::jlimit(0, 999, (int)(dspLoad * 100.f));

        // 12f: PDC latency readout. Total samples reported by host PDC, plus
        // ms-converted form when sample rate is known. "--" when callback empty.
        juce::String latStr ("--");
        if (onGetLatencySamples)
            latStr = juce::String(onGetLatencySamples());

        // Unit suffixes dropped (covered by tooltip) to keep the column widths
        // balanced - otherwise "MEM 234MB  LAT 16sp/0.3ms" overflows the row
        // bounds left of the right-justified anchor and clips the M of MEM.
        //
        // CL-282: streaming telemetry.  UND = clip-stream reads that returned
        // silence during CONTINUOUS playback (seeks never count); PF = worst
        // disk-fill this session in ms.  Zero / small is healthy.
        const auto und   = AudioClipStreamer::sUnderrunCount.load (std::memory_order_relaxed);
        const auto pfMs  = AudioClipStreamer::sPeakPrefillMs.load (std::memory_order_relaxed);
        juce::String row2, row3;
        row2 << "MEM " << memMb << "  LAT " << latStr;
        row3 << "UND " << (int) und << "  PF " << juce::String (pfMs, 1);

        // L20 (QA-Layout): SYS colours only itself, off whole-machine CPU;
        // DSP load/overload colours the DSP token and the remaining rows, so
        // background apps can't tint an idle project's engine readout.
        juce::Colour sysCol;
        if      (sysCpu > 80.f) sysCol = VC::Yellow;
        else if (sysCpu > 50.f) sysCol = VC::Yellow.withAlpha(0.7f);
        else                    sysCol = VC::Green;

        juce::Colour dspCol;
        if (over95)
        {
            // Flash between bright red and dark red each tick (10 Hz = visible flicker)
            static bool sFlash = false;
            sFlash = !sFlash;
            dspCol = sFlash ? juce::Colour(0xffff2222) : juce::Colour(0xff991111);
        }
        else if (dspLoad > 0.85f)
            dspCol = VC::Yellow;
        else if (dspLoad > 0.5f)
            dspCol = VC::Yellow.withAlpha(0.7f);
        else
            dspCol = VC::Green;

        mPerfReadout->update ("SYS " + juce::String ((int) sysCpu) + "%",
                              "DSP " + juce::String (dspPct) + "%",
                              row2, row3, sysCol, dspCol);
    }

    // Keep BPM field synced to playhead tempo if it's not focused
    if (!mBpmField->hasKeyboardFocus(false))
    {
        double ph_bpm = mPlayHead.getBPM();
        double ui_bpm = getBPM();
        if (std::abs(ph_bpm - ui_bpm) > 0.01)
            mBpmField->setText(juce::String(ph_bpm, 1), false);
    }
}

void GlobalTransportBar::doTap()
{
    double now = juce::Time::getMillisecondCounterHiRes();

    // If gap > 2 s, treat as a fresh sequence
    if (!mTapTimes.isEmpty() && (now - mTapTimes.getLast()) > 2000.0)
        mTapTimes.clear();

    mTapTimes.add(now);

    // Need at least 2 taps to compute a period
    if (mTapTimes.size() < 2)
        return;

    // Keep only the last 8 taps
    while (mTapTimes.size() > 8)
        mTapTimes.remove(0);

    // Average inter-tap interval
    double totalMs = mTapTimes.getLast() - mTapTimes.getFirst();
    double avgMs   = totalMs / (double)(mTapTimes.size() - 1);
    double bpm     = juce::jlimit(20.0, 300.0, 60000.0 / avgMs);

    // Round to nearest whole number
    bpm = std::round(bpm);

    mBpmField->setText(juce::String((int)bpm), false);
    if (onTempoChanged) onTempoChanged(bpm);
}

void GlobalTransportBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // LRX-14: Brushed aluminum base - dark silver with subtle top-lit gradient
    juce::ColourGradient baseGrad(
        juce::Colour(0xff22252A), 0.f, b.getY(),
        juce::Colour(0xff1A1D21), 0.f, b.getBottom(), false);
    g.setGradientFill(baseGrad);
    g.fillRect(b);

    // LRX-14: 1D horizontal grain (brushed aluminum)
    {
        juce::Random rng(17);
        for (int y = 0; y < getHeight(); ++y)
        {
            float alpha = rng.nextFloat() * 0.025f + 0.005f;
            bool bright = rng.nextBool();
            g.setColour((bright ? juce::Colours::white : juce::Colours::black).withAlpha(alpha));
            g.drawHorizontalLine(y, 0.f, (float)getWidth());
        }
    }

    // Top highlight edge (light catching the rim)
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.fillRect(0.f, 0.f, (float)getWidth(), 1.f);

    // Bottom separator line
    g.setColour(VC::Accent);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

void GlobalTransportBar::resized()
{
    auto b = getLocalBounds().reduced(4, 3);

    // Left side: transport buttons
    int btnW  = 34;
    int space = 3;

    mPlayBtn->setBounds (b.removeFromLeft(btnW).reduced(1, 1));
    b.removeFromLeft(space);
    mPauseBtn->setBounds(b.removeFromLeft(btnW).reduced(1, 1));
    b.removeFromLeft(space);
    mStopBtn->setBounds (b.removeFromLeft(btnW).reduced(1, 1));
    b.removeFromLeft(space);
    // R5a: record button is wider to fit the dropdown chevron without crowding the dot.
    mRecBtn->setBounds  (b.removeFromLeft(btnW + 10).reduced(1, 1));
    b.removeFromLeft(12);

    // BPM: label + field + tap button
    mBpmLabel->setBounds(b.removeFromLeft(28).reduced(0, 2));
    mBpmField->setBounds(b.removeFromLeft(54).reduced(1, 1));
    b.removeFromLeft(4);
    mTapBtn->setBounds(b.removeFromLeft(46).reduced(1, 1));
    b.removeFromLeft(8);

    // Right side: mode toggles
    mSongModeBtn->setBounds (b.removeFromLeft(72).reduced(1, 1));
    b.removeFromLeft(2);
    // Loop toggle (→ / 🔁) sits immediately next to SONG button
    if (mSongLoopBtn) mSongLoopBtn->setBounds(b.removeFromLeft(28).reduced(1, 1));
    b.removeFromLeft(space);
    // 2026-04-26 (D-5 polish): button widths tightened around the icon glyphs
    // (metronome 30, arrow 18).  kControlsWidth stays at 520 - the once-
    // reserved ~40 px now holds the D-4 typing-keyboard toggle (2026-07-08).
    mMetronomeBtn->setBounds(b.removeFromLeft(30).reduced(1, 1));
    if (mMetroArrowBtn) mMetroArrowBtn->setBounds(b.removeFromLeft(18).reduced(1, 1));
    b.removeFromLeft(4);
    if (mKeybMidiBtn) mKeybMidiBtn->setBounds(b.removeFromLeft(32).reduced(1, 1));
    b.removeFromLeft(2);
    // SW-1; smoke #59: 24 px + shifted left so it clears the pattern box.
    if (mSwingKnob) mSwingKnob->setBounds(b.removeFromLeft(24).reduced(1, 1));
    b.removeFromLeft(12);

    // Three-row perf readout (far right).  kWidth is shared with the ribbon
    // reserve in StandaloneEditor::resized() -- see TransportPerfReadout.
    mPerfReadout->setBounds(b.removeFromRight(TransportPerfReadout::kWidth).reduced(2, 0));
}

void GlobalTransportBar::refreshSwingKnob()
{
    if (mSwingKnob && onGetSwing)
        mSwingKnob->setValue ((double) onGetSwing(), juce::dontSendNotification);
}

void GlobalTransportBar::setSongMode(bool song)
{
    if (!mSongModeBtn) return;
    if (mSongModeBtn->getToggleState() == song) return;   // no-op, avoid callback churn
    mSongModeBtn->setToggleState(song, juce::dontSendNotification);
    mSongModeBtn->setButtonText(song ? "SONG" : "PATTERN");
    if (onSongModeChanged) onSongModeChanged(song);
}

void GlobalTransportBar::setTypingKeyboardOn (bool on)
{
    if (mKeybMidiBtn && mKeybMidiBtn->getToggleState() != on)
    {
        mKeybMidiBtn->setToggleState (on, juce::dontSendNotification);
        mKeybMidiBtn->repaint();
    }
}

// ── TransportPositionReadout ─────────────────────────────────────────────────
TransportPositionReadout::TransportPositionReadout()
{
    setTooltip ("Playback position - click to switch between bars:beats:ticks and time");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void TransportPositionReadout::setShowTime (bool showTime)
{
    if (mShowTime == showTime) return;
    mShowTime = showTime;
    mText = buildText();
    repaint();
}

void TransportPositionReadout::mouseDown (const juce::MouseEvent&)
{
    mShowTime = ! mShowTime;
    mText = buildText();
    repaint();
    if (onDisplayModeChanged) onDisplayModeChanged (mShowTime);
}

void TransportPositionReadout::timerCallback()
{
    auto s = buildText();
    if (s != mText) { mText = s; repaint(); }
}

juce::String TransportPositionReadout::buildText() const
{
    if (mShowTime)
    {
        const double secs = onGetTimeSeconds ? juce::jmax (0.0, onGetTimeSeconds()) : 0.0;
        int mins = (int) (secs / 60.0);
        int ms   = (int) juce::roundToInt ((secs - mins * 60.0) * 1000.0);
        int s    = ms / 1000;
        ms %= 1000;
        if (s >= 60) { s -= 60; ++mins; }
        return juce::String (mins) + ":" + juce::String (s).paddedLeft ('0', 2)
             + "." + juce::String (ms).paddedLeft ('0', 3);
    }

    const double beat = onGetBeat ? juce::jmax (0.0, onGetBeat()) : 0.0;
    // QA-G Task 6: song mode reads the marker map (bars resize at TS
    // markers); pattern mode uses the pattern's effective signature.  Beats
    // display in DENOMINATOR units (7/8 counts 1..7), matching the
    // metronome; ticks are 96-PPQ within the beat unit.  1-based display.
    // Quantize to ticks BEFORE splitting so 95.99 ticks can't render as 96.
    const juce::int64 totalTicks = (juce::int64) std::llround (beat * 96.0);
    int num = 4, den = 4;
    juce::int64 barNum = 0, inBar = 0;
    const bool songMode = onGetSongMode && onGetSongMode();
    if (songMode && TsMap::isActive())
    {
        const double qBeat = (double) totalTicks / 96.0;
        const auto bb = TsMap::barBeatAt (qBeat + 1.0e-9);
        num = juce::jmax (1, bb.num);
        den = juce::jmax (1, bb.den);
        barNum = bb.bar;
        inBar  = juce::jmax ((juce::int64) 0,
                    totalTicks - (juce::int64) std::llround (bb.barStartBeat * 96.0));
    }
    else
    {
        if (! songMode && onGetPatternTs) onGetPatternTs (num, den);
        num = juce::jmax (1, num);
        den = juce::jmax (1, den);
        const juce::int64 ticksPerBar = juce::jmax ((juce::int64) 1,
            (juce::int64) std::llround ((double) num * 4.0 / (double) den * 96.0));
        barNum = totalTicks / ticksPerBar;
        inBar  = totalTicks - barNum * ticksPerBar;
    }
    const juce::int64 unit  = juce::jmax ((juce::int64) 1,
        (juce::int64) std::llround (96.0 * 4.0 / (double) den));
    const juce::int64 beatN = inBar / unit;
    const juce::int64 tick  = inBar % unit;
    return juce::String (barNum + 1) + ":" + juce::String ((int) beatN + 1)
         + ":" + juce::String ((int) tick).paddedLeft ('0', 2);
}

void TransportPositionReadout::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // Same LCD treatment as the BPM field (LRX-14 palette).
    g.setColour (juce::Colour (0xff0A0A08));
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (juce::Colour (0xff2A2A28));
    g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (juce::Colour (0xffFFB030));
    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold));
    g.drawText (mText, getLocalBounds().reduced (4, 0), juce::Justification::centred, false);
}

// ── TransportPerfReadout ─────────────────────────────────────────────────────
TransportPerfReadout::TransportPerfReadout()
{
    refreshTooltip();
}

void TransportPerfReadout::refreshTooltip()
{
    setTooltip (mSysText + "   " + mDspText + "\n"
                + mRow2 + "\n"
                + mRow3 + "\n\n"
                + "SYS: system-wide CPU % (the whole computer, not this app)  |  "
                  "DSP: audio engine load (% of buffer window)  |  "
                  "MEM: process memory in MB  |  "
                  "LAT: total reported plugin latency in samples (sum of every effect "
                  "that adds PDC)  |  "
                  "UND: audio clip stream underruns during continuous playback "
                  "(should stay 0)  |  "
                  "PF: slowest disk read this session in ms");
}

void TransportPerfReadout::update (const juce::String& sysText, const juce::String& dspText,
                                   const juce::String& row2, const juce::String& row3,
                                   juce::Colour sysCol, juce::Colour dspCol)
{
    if (sysText == mSysText && dspText == mDspText && row2 == mRow2 && row3 == mRow3
        && sysCol == mSysCol && dspCol == mDspCol)
        return;

    mSysText = sysText;  mDspText = dspText;
    mRow2    = row2;     mRow3    = row3;
    mSysCol  = sysCol;   mDspCol  = dspCol;
    refreshTooltip();
    repaint();
}

void TransportPerfReadout::paint (juce::Graphics& g)
{
    const juce::Font f (juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain);
    g.setFont (f);

    const auto area = getLocalBounds().withTrimmedRight (2);
    const int  rowH = area.getHeight() / 3;

    // Row 1 draws as two right-anchored segments so SYS and DSP can carry
    // their own colours (L20 per-token rule).
    const int dspW = f.getStringWidth (mDspText);
    const int gapW = f.getStringWidth ("  ");
    const int sysW = f.getStringWidth (mSysText);

    g.setColour (mSysCol);
    g.drawText (mSysText, area.getRight() - dspW - gapW - sysW, area.getY(), sysW, rowH,
                juce::Justification::centredLeft);
    g.setColour (mDspCol);
    g.drawText (mDspText, area.getRight() - dspW, area.getY(), dspW, rowH,
                juce::Justification::centredLeft);

    g.drawText (mRow2, area.getX(), area.getY() + rowH, area.getWidth(), rowH,
                juce::Justification::centredRight);
    g.drawText (mRow3, area.getX(), area.getY() + rowH * 2, area.getWidth(),
                area.getHeight() - rowH * 2, juce::Justification::centredRight);
}
