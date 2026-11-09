#include "Panels.h"

using namespace juce;

// ---------------------------------------------------------------- look and feel (Frutiger Aero, modernised)
PotatoLookAndFeel::PotatoLookAndFeel() { refreshColours(); }

void PotatoLookAndFeel::refreshColours()
{
    setColour (ResizableWindow::backgroundColourId, theme::bg);
    setColour (ComboBox::backgroundColourId, theme::panelHi);
    setColour (ComboBox::outlineColourId, theme::grid);
    setColour (ComboBox::textColourId, theme::text);
    setColour (ComboBox::arrowColourId, theme::accent);
    setColour (PopupMenu::backgroundColourId, theme::panel.darker (0.2f));
    setColour (PopupMenu::textColourId, theme::text);
    setColour (PopupMenu::headerTextColourId, theme::accent);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.3f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (Label::textColourId, theme::text);
    setColour (Slider::textBoxTextColourId, theme::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::black.withAlpha (0.18f));
    setColour (TextButton::buttonColourId, theme::panelHi);
    setColour (TextButton::buttonOnColourId, theme::accent.darker (0.3f));
    setColour (TextButton::textColourOffId, theme::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (ToggleButton::textColourId, theme::text);
    setColour (ToggleButton::tickColourId, theme::accent);
    setColour (TooltipWindow::backgroundColourId, theme::panel);
    setColour (TooltipWindow::textColourId, theme::text);
    setColour (TooltipWindow::outlineColourId, theme::accent.withAlpha (0.4f));
}

// glossy pill used by buttons and combo boxes
static void drawPill (Graphics& g, Rectangle<float> r, Colour base, bool over, bool down)
{
    const float rad = jmin (r.getHeight() * 0.5f, 10.0f);
    auto c = down ? base.brighter (0.25f) : over ? base.brighter (0.12f) : base;
    g.setGradientFill (ColourGradient (c.brighter (0.25f), 0, r.getY(), c.darker (0.3f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    auto sheen = r.reduced (2.0f, 1.5f).removeFromTop (r.getHeight() * 0.48f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.28f), 0, sheen.getY(), Colours::white.withAlpha (0.04f), 0, sheen.getBottom(), false));
    g.fillRoundedRectangle (sheen, rad - 1.5f);
    g.setColour (Colours::white.withAlpha (over ? 0.32f : 0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

void PotatoLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (3);
    const float r = jmin (b.getWidth(), b.getHeight()) / 2.0f;
    const auto c = b.getCentre();
    const float ang = start + pos * (end - start);
    const auto col = s.isEnabled() ? s.findColour (Slider::rotarySliderFillColourId) : theme::textDim.withAlpha (0.5f);

    Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, start, end, true);
    g.setColour (Colours::black.withAlpha (0.35f)); g.strokePath (track, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (start + end) / 2 : start;
    Path val; val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, jmin (from, ang), jmax (from, ang), true);
    g.setColour (col.withAlpha (0.35f)); g.strokePath (val, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded));   // glow
    g.setColour (col); g.strokePath (val, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    theme::drawOrb (g, c, r - 7, theme::chrome.darker (0.15f));   // chrome knob cap
    g.setColour (Colours::black.withAlpha (s.isEnabled() ? 0.75f : 0.3f));
    g.drawLine (Line<float> (c.getPointOnCircumference ((r - 7) * 0.3f, ang), c.getPointOnCircumference (r - 10, ang)), 2.2f);
}

void PotatoLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    const auto b = Rectangle<int> (x, y, w, h).toFloat();
    const auto track = b.withSizeKeepingCentre (b.getWidth(), 6.0f);
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (track, 3);
    const auto col = s.findColour (Slider::trackColourId);
    g.setGradientFill (ColourGradient (col.brighter (0.3f), 0, track.getY(), col.darker (0.2f), 0, track.getBottom(), false));
    g.fillRoundedRectangle (track.withRight (pos), 3);
    theme::drawOrb (g, { pos, b.getCentreY() }, 7.5f, Colour (0xffdff3ff));
}

void PotatoLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& base, bool over, bool down)
{
    drawPill (g, b.getLocalBounds().toFloat().reduced (0.5f), b.getToggleState() ? theme::accent.darker (0.25f) : base, over, down);
}

void PotatoLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool down, int, int, int, int, ComboBox& box)
{
    drawPill (g, Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f), theme::panelHi, box.isMouseOver (true), down);
    Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4, ay - 2); arrow.lineTo (ax, ay + 2); arrow.lineTo (ax + 4, ay - 2);
    g.setColour (box.isEnabled() ? theme::accent : theme::textDim);
    g.strokePath (arrow, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

void PotatoLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void PotatoLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (theme::panel.darker (0.35f));
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRect (0, 0, w, h, 1);
}

void PotatoLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (3.5f);
    const bool on = b.getToggleState();
    if (on)
    {   // lit aqua bubble with glow
        g.setGradientFill (ColourGradient (theme::accent.withAlpha (0.5f), box.getCentreX(), box.getCentreY(), theme::accent.withAlpha (0.0f), box.getRight() + 5, box.getCentreY(), true));
        g.fillEllipse (box.expanded (5));
    }
    theme::drawOrb (g, box.getCentre(), box.getWidth() * 0.5f, on ? theme::accent : (over ? theme::panelHi.brighter (0.3f) : theme::panelHi), on);
    g.setColour (b.isEnabled() ? theme::text : theme::textDim);
    g.setFont (theme::font (12.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (5), Justification::centredLeft);
}

void PotatoLookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    theme::drawGlassPanel (g, Rectangle<float> (0, 0, (float) w, (float) h), 6.0f, theme::panel.darker (0.2f));
    g.setColour (theme::text); g.setFont (theme::font (12.5f));
    g.drawFittedText (text, Rectangle<int> (0, 0, w, h).reduced (8, 4), Justification::centredLeft, 4);
}

// ---------------------------------------------------------------- knob
Knob::Knob (const String& caption)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 16);
    slider.setColour (Slider::rotarySliderFillColourId, theme::accent);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    label.setText (caption, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (theme::font (10.5f, true));
    label.setColour (Label::textColourId, theme::textDim);
    addAndMakeVisible (slider); addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

static void setupSmallSlider (Component& parent, Slider& sl, Label& l, const String& text, Colour c)
{
    sl.setSliderStyle (Slider::LinearHorizontal);
    sl.setTextBoxStyle (Slider::TextBoxRight, false, 74, 16);
    sl.setColour (Slider::trackColourId, c);
    l.setText (text, dontSendNotification);
    l.setFont (theme::font (11.0f));
    l.setColour (Label::textColourId, theme::textDim);
    parent.addAndMakeVisible (sl); parent.addAndMakeVisible (l);
}

void fillCombo (ComboBox& c, APVTS& s, const String& paramId)
{
    c.clear (dontSendNotification);
    if (auto* p = dynamic_cast<AudioParameterChoice*> (s.getParameter (paramId)))
        c.addItemList (p->choices, 1);
}

// ---------------------------------------------------------------- band panel
BandPanel::BandPanel (PotatoEQProcessor& p) : proc (p)
{
    for (auto* l : { &typeL, &slopeL, &placeL })
    {
        l->setFont (theme::font (10.5f, true)); l->setColour (Label::textColourId, theme::textDim);
        l->setJustificationType (Justification::centredLeft);   // compact floating panel: combos speak for themselves
    }
    type.setTooltip ("Band shape"); slope.setTooltip ("Slope (cuts and shelves)"); place.setTooltip ("Stereo placement");

    typeL.setText ("SHAPE", dontSendNotification); slopeL.setText ("SLOPE", dontSendNotification); placeL.setText ("STEREO", dontSendNotification);
    for (auto* c : { &type, &slope, &place }) addAndMakeVisible (*c);
    for (auto* k : { &freq, &gain, &q, &thr, &range }) addAndMakeVisible (*k);
    for (auto* b : { (Component*) &dyn, (Component*) &sc, (Component*) &bypass, (Component*) &del }) addAndMakeVisible (*b);
    sc.setTooltip ("Dynamic detection listens to the plugin's sidechain input instead of this track "
                   "(e.g. duck the bass at 60 Hz only when the kick hits). Route a track to the sidechain in your DAW.");
    thr.slider.setColour (Slider::rotarySliderFillColourId, theme::autoCol);
    range.slider.setColour (Slider::rotarySliderFillColourId, theme::autoCol);
    del.onClick = [this] { if (band >= 0 && onDelete) onDelete (band); };
    dyn.setTooltip ("Dynamic EQ: the band only moves when its frequency range gets louder than the threshold.");
    setBand (-1);
    startTimerHz (10);
}

BandPanel::~BandPanel() { stopTimer(); }

void BandPanel::setBand (int b)
{
    aType.reset(); aSlope.reset(); aPlace.reset(); aFreq.reset(); aGain.reset(); aQ.reset(); aThr.reset(); aRange.reset(); aDyn.reset(); aBypass.reset(); aSc.reset();
    band = b;
    for (int i = 0; i < getNumChildComponents(); ++i) getChildComponent (i)->setVisible (b >= 0);
    if (b >= 0)
    {
        auto& s = proc.apvts;
        fillCombo (type, s, params::bandId (b, "type")); fillCombo (slope, s, params::bandId (b, "slope")); fillCombo (place, s, params::bandId (b, "place"));
        aType = std::make_unique<APVTS::ComboBoxAttachment> (s, params::bandId (b, "type"), type);
        aSlope = std::make_unique<APVTS::ComboBoxAttachment> (s, params::bandId (b, "slope"), slope);
        aPlace = std::make_unique<APVTS::ComboBoxAttachment> (s, params::bandId (b, "place"), place);
        aFreq = std::make_unique<APVTS::SliderAttachment> (s, params::bandId (b, "freq"), freq.slider);
        aGain = std::make_unique<APVTS::SliderAttachment> (s, params::bandId (b, "gain"), gain.slider);
        aQ = std::make_unique<APVTS::SliderAttachment> (s, params::bandId (b, "q"), q.slider);
        aThr = std::make_unique<APVTS::SliderAttachment> (s, params::bandId (b, "thr"), thr.slider);
        aRange = std::make_unique<APVTS::SliderAttachment> (s, params::bandId (b, "range"), range.slider);
        aDyn = std::make_unique<APVTS::ButtonAttachment> (s, params::bandId (b, "dyn"), dyn);
        aBypass = std::make_unique<APVTS::ButtonAttachment> (s, params::bandId (b, "bypass"), bypass);
        aSc = std::make_unique<APVTS::ButtonAttachment> (s, params::bandId (b, "sc"), sc);
        const auto c = theme::bandColour (b);
        for (auto* k : { &freq, &gain, &q }) k->slider.setColour (Slider::rotarySliderFillColourId, c);
        timerCallback();
    }
    repaint();
}

void BandPanel::timerCallback()
{
    if (band < 0) return;
    const auto s = proc.bandSettings (band);
    if (! s.used) { setBand (-1); return; }
    gain.setEnabled (arc::typeHasGain (s.type));
    slope.setEnabled (arc::typeHasSlope (s.type));
    dyn.setEnabled (arc::typeSupportsDynamic (s.type));
    const bool dynOn = s.dynamic && arc::typeSupportsDynamic (s.type);
    thr.setEnabled (dynOn); range.setEnabled (dynOn); sc.setEnabled (dynOn);
    repaint();
}

void BandPanel::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    theme::drawGlassPanel (g, bounds);
    if (band < 0)
    {
        g.setColour (theme::textDim); g.setFont (theme::font (13.0f));
        g.drawText ("Click a band to edit it here.  Double-click the display to add one, or drag a peak in the analyzer.",
                    getLocalBounds(), Justification::centred);
        return;
    }
    const auto c = theme::bandColour (band);
    g.setColour (c.withAlpha (0.5f)); g.drawRoundedRectangle (bounds.reduced (1.0f), theme::radius - 1, 1.2f);
    auto r = getLocalBounds().reduced (12, 8);
    g.setColour (c); g.fillEllipse (Rectangle<float> ((float) r.getX(), (float) r.getY() + 3, 10, 10));
    g.setColour (theme::text); g.setFont (theme::font (13.0f, true));
    g.drawText ("Band " + String (band + 1), r.getX() + 16, r.getY(), 70, 16, Justification::centredLeft);
    const auto s = proc.bandSettings (band);
    g.setColour (theme::textDim); g.setFont (theme::font (11.0f));
    String info = theme::noteName (s.freq);
    const float d = proc.dynamicDelta (band);
    if (std::abs (d) > 0.05f) info << "   dyn " << String (d, 1) << " dB";
    g.drawText (info, r.getX() + 84, r.getY(), 120, 16, Justification::centredLeft);
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    auto col = r.removeFromLeft (150);
    col.removeFromTop (20);
    for (auto* c : { &type, &slope, &place }) { c->setBounds (col.removeFromTop (22)); col.removeFromTop (3); }
    r.removeFromLeft (8);
    for (auto* k : { &freq, &gain, &q }) k->setBounds (r.removeFromLeft (72));
    r.removeFromLeft (8);
    auto dcol = r.removeFromLeft (100).withTrimmedTop (2);
    dyn.setBounds (dcol.removeFromTop (22)); sc.setBounds (dcol.removeFromTop (22)); bypass.setBounds (dcol.removeFromTop (22));
    dcol.removeFromTop (3); del.setBounds (dcol.removeFromTop (20));
    r.removeFromLeft (4);
    for (auto* k : { &thr, &range }) k->setBounds (r.removeFromLeft (72));
}

// ---------------------------------------------------------------- loudness panel
LoudnessPanel::LoudnessPanel (PotatoEQProcessor& p) : proc (p)
{
    fillCombo (target, proc.apvts, params::id::lufsTarget);
    aTarget = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, params::id::lufsTarget, target);
    addAndMakeVisible (target); addAndMakeVisible (match); addAndMakeVisible (reset);
    match.setTooltip ("Sets the output gain so the integrated loudness hits the target, without pushing the true peak above -1 dBTP. "
                      "Reaching loud targets needs a limiter after this EQ.");
    reset.onClick = [this] { proc.resetMeter(); history.clear(); status = {}; };
    for (auto* b : { &unitLufs, &unitDb })
    {
        b->setClickingTogglesState (false);
        b->setConnectedEdges (b == &unitLufs ? Button::ConnectedOnRight : Button::ConnectedOnLeft);
        addAndMakeVisible (*b);
    }
    unitLufs.setTooltip ("Show loudness in LUFS (what streaming services measure)");
    unitDb.setTooltip ("Show plain levels in dBFS: RMS, peak and crest factor");
    unitLufs.onClick = [this] { proc.apvts.state.setProperty (ana::units, 0, nullptr); };
    unitDb.onClick = [this] { proc.apvts.state.setProperty (ana::units, 1, nullptr); };
    aAutoLevel = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, params::id::autoLevel, autoLevel);
    fillCombo (levelMode, proc.apvts, params::id::levelMode);
    aLevelMode = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, params::id::levelMode, levelMode);
    levelMode.setTooltip ("Ride continuously: keeps adjusting through the song.  Learn, then lock: listens for ~15 s of music, "
                          "settles on one gain and keeps it (saved with the project).");
    relearnLevel.setTooltip ("Forget the locked gain and learn again from what plays next.");
    relearnLevel.onClick = [this] { proc.relearnLevel(); };
    addAndMakeVisible (levelMode); addAndMakeVisible (relearnLevel);
    aLimiter = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, params::id::limiter, limiterOn);
    autoLevel.setTooltip ("Continuously sets the gain so the track lands on the loudness target. Turn the limiter on for loud targets.");
    limiterOn.setTooltip ("Brickwall look-ahead limiter at the end of the chain. Catches peaks so louder targets don't clip.");
    addAndMakeVisible (autoLevel); addAndMakeVisible (limiterOn);
    setupSmallSlider (*this, ceiling, ceilingL, "Ceiling", theme::bad.withAlpha (0.8f));
    setupSmallSlider (*this, custom, customL, "Custom target", theme::accent);
    aCeiling = std::make_unique<APVTS::SliderAttachment> (proc.apvts, params::id::ceiling, ceiling);
    aCustom = std::make_unique<APVTS::SliderAttachment> (proc.apvts, params::id::lufsCustom, custom);
    match.onClick = [this]
    {
        const auto m = proc.matchLoudnessToTarget();
        if (! m.ok) status = "Play some audio first.";
        else if (m.limitedByPeak) status = "Applied " + String (m.appliedDb, 1) + " dB of " + String (m.wantedDb, 1) + " dB.  Add a limiter to go louder.";
        else status = "Output gain changed by " + String (m.appliedDb, 1) + " dB.";
        history.clear();
    };
    startTimerHz (20);
}

LoudnessPanel::~LoudnessPanel() { stopTimer(); }

void LoudnessPanel::timerCallback()
{
    r = proc.meter.get();
    grShown = jmax (proc.limiterReductionDb(), grShown * 0.85f);
    const int units = ana::get (proc.apvts.state, ana::units, 0);
    if (units != lastUnits) { lastUnits = units; history.clear(); }
    unitLufs.setToggleState (units != 1, dontSendNotification);
    unitDb.setToggleState (units == 1, dontSendNotification);
    const bool isCustom = params::lufsTargets()[(size_t) jlimit (0, (int) params::lufsTargets().size() - 1, target.getSelectedId() - 1)].lufs > 0;
    custom.setEnabled (isCustom); customL.setEnabled (isCustom);
    ceiling.setEnabled (limiterOn.getToggleState());
    levelMode.setEnabled (autoLevel.getToggleState());
    relearnLevel.setEnabled (autoLevel.getToggleState() && levelMode.getSelectedItemIndex() == 1);
    if (++tick % 2 == 0)
    {
        history.push_back (lastUnits == 1 ? r.rmsDb : r.momentary);
        if (history.size() > 300) history.erase (history.begin());
    }
    repaint();
}

void LoudnessPanel::paint (Graphics& g)
{
    theme::drawGlassPanel (g, getLocalBounds().toFloat());
    auto r0 = getLocalBounds().reduced (12, 10);
    g.setColour (theme::text); g.setFont (theme::font (13.0f, true));
    const int units = jmax (0, lastUnits);
    const bool db = units == 1;
    g.drawText (db ? "LEVEL  (output)" : "LOUDNESS  (output)", r0.removeFromTop (18), Justification::centredLeft);

    const float tgt = proc.targetLufs();
    auto fmt = [] (float v) { return v <= -69.9f ? String ("--") : String (v, 1); };
    auto fmtDb = [] (float v) { return v <= -99.0f ? String ("-inf") : String (v, 1); };

    auto big = r0.removeFromTop (54);
    g.setColour (theme::textDim); g.setFont (theme::font (10.5f, true));
    g.drawText (db ? "RMS dBFS  (3 s)" : "INTEGRATED LUFS", big.removeFromTop (14), Justification::centredLeft);
    if (db)
    {
        g.setColour (r.rmsSlowDb < -99 ? theme::textDim : theme::text);
        g.setFont (theme::font (34.0f, true));
        g.drawText (fmtDb (r.rmsSlowDb), big.removeFromLeft (120), Justification::centredLeft);
        g.setFont (theme::font (12.0f)); g.setColour (r.clipped ? theme::bad : theme::textDim);
        g.drawText (r.clipped ? String ("CLIPPED  (Reset to clear)") : "headroom " + fmtDb (-r.samplePeakDb) + " dB", big, Justification::centredLeft);
    }
    else
    {
        const float I = r.integratedValid ? r.integrated : -100.0f;
        const float diff = I - tgt;
        g.setColour (I < -69 ? theme::textDim : std::abs (diff) <= 1.0f ? theme::good : std::abs (diff) <= 3.0f ? theme::warn : theme::text);
        g.setFont (theme::font (34.0f, true));
        g.drawText (fmt (I), big.removeFromLeft (120), Justification::centredLeft);
        auto side = big;
        if (I > -69)
        {
            g.setFont (theme::font (12.0f)); g.setColour (theme::textDim);
            g.drawText (std::abs (diff) < 0.05f ? String ("on target") : (diff > 0 ? "+" : "") + String (diff, 1) + " LU vs target",
                        units == 2 ? side.removeFromTop (side.getHeight() / 2) : side, Justification::centredLeft);
        }
        if (units == 2)
        {
            g.setFont (theme::font (11.5f)); g.setColour (theme::textDim);
            g.drawText ("RMS " + fmtDb (r.rmsSlowDb) + "  pk " + fmtDb (r.peakHoldDb) + " dB", side.withTrimmedTop (side.getHeight() / 2), Justification::centredLeft);
        }
    }

    auto row = [&] (const String& name, const String& val, Colour c)
    {
        auto rr = r0.removeFromTop (18);
        g.setColour (theme::textDim); g.setFont (theme::font (12.0f));
        g.drawText (name, rr, Justification::centredLeft);
        g.setColour (c); g.setFont (theme::font (12.5f, true));
        g.drawText (val, rr, Justification::centredRight);
    };
    if (db)
    {
        row ("RMS (300 ms)", fmtDb (r.rmsDb) + " dBFS", theme::text);
        row ("Peak (held 3 s)", fmtDb (r.peakHoldDb) + " dBFS", r.peakHoldDb > -0.1f ? theme::bad : theme::text);
        row ("True peak (max)", fmt (r.truePeakDb) + " dBTP", r.truePeakDb > -1.0f ? theme::bad : theme::text);
        const float crest = r.samplePeakDb - r.rmsSlowDb;
        row ("Crest factor (peak - RMS)", r.rmsSlowDb < -99 ? String ("--") : String (crest, 1) + " dB", crest < 6.0f && r.rmsSlowDb > -99 ? theme::warn : theme::text);
    }
    else
    {
        row ("Short-term (3 s)", fmt (r.shortTerm) + " LUFS", theme::text);
        row ("Momentary (400 ms)", fmt (r.momentary) + " LUFS", theme::text);
        row ("Loudness range", String (r.lra, 1) + " LU", theme::text);
        row ("True peak", fmt (r.truePeakDb) + " dBTP", r.truePeakDb > -1.0f ? theme::bad : theme::text);
    }
    {
        String extra;
        const auto ls = proc.levelState();
        const bool levelOn = autoLevel.getToggleState();
        auto sgn = [] (float v) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; };
        if (levelOn && ls.lockMode && ls.locked) extra << "Locked " << sgn (proc.autoLevelDb());
        else if (levelOn && ls.lockMode) extra << "Learning " << (int) (ls.progress * 100) << "%  " << sgn (proc.autoLevelDb());
        else if (std::abs (proc.autoLevelDb()) > 0.05f) extra << "Auto level " << sgn (proc.autoLevelDb());
        if (limiterOn.getToggleState()) extra << (extra.isEmpty() ? "" : "   ") << (grShown < 0.05f ? String ("Limiter ready") : "Limiting " + String (grShown, 1) + " dB");
        row (extra.isEmpty() ? String() : String ("Gain"), extra, grShown > 3.0f ? theme::warn : theme::accent);
    }

    // history graph with the target line
    r0.removeFromTop (6);
    auto graph = r0.removeFromTop (60).toFloat();
    g.setColour (theme::bg); g.fillRoundedRectangle (graph, 4);
    const float span = db ? 48.0f : 40.0f;
    auto yFor = [&] (float l) { return graph.getBottom() - graph.getHeight() * jlimit (0.0f, 1.0f, (l + span) / span); };
    if (! db) { g.setColour (theme::accent.withAlpha (0.5f)); g.drawHorizontalLine ((int) yFor (tgt), graph.getX(), graph.getRight()); }
    if (history.size() > 1)
    {
        Path p;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const float x = graph.getRight() - graph.getWidth() * (float) (history.size() - 1 - i) / 300.0f;
            if (i == 0) p.startNewSubPath (x, yFor (history[i])); else p.lineTo (x, yFor (history[i]));
        }
        g.setColour (theme::postSpec); g.strokePath (p, PathStrokeType (1.4f));
    }
    g.setColour (theme::gridText); g.setFont (theme::font (9.5f));
    g.drawText (db ? "RMS, last 30 s" : "momentary, last 30 s", graph.reduced (4, 2), Justification::topLeft);

    if (status.isNotEmpty())
    {
        g.setColour (theme::textDim); g.setFont (theme::font (11.0f));
        g.drawFittedText (status, getLocalBounds().reduced (12, 8).removeFromBottom (28), Justification::centredLeft, 2);
    }
}

void LoudnessPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto sw = r.withHeight (20).removeFromRight (92);
    unitDb.setBounds (sw.removeFromRight (46)); unitLufs.setBounds (sw);
    r.removeFromTop (18 + 54 + 18 * 5 + 6 + 60 + 8);
    target.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    auto cr = r.removeFromTop (20); customL.setBounds (cr.removeFromLeft (90)); custom.setBounds (cr);
    r.removeFromTop (6);
    auto row = r.removeFromTop (26);
    reset.setBounds (row.removeFromRight (64));
    row.removeFromRight (6);
    match.setBounds (row);
    r.removeFromTop (6);
    auto t2 = r.removeFromTop (24);
    limiterOn.setBounds (t2.removeFromRight (90)); autoLevel.setBounds (t2);
    r.removeFromTop (3);
    auto t3 = r.removeFromTop (22);
    relearnLevel.setBounds (t3.removeFromRight (80)); t3.removeFromRight (6); levelMode.setBounds (t3);
    r.removeFromTop (4);
    auto cl = r.removeFromTop (20); ceilingL.setBounds (cl.removeFromLeft (90)); ceiling.setBounds (cl);
}

// ---------------------------------------------------------------- auto EQ panel
AutoPanel::AutoPanel (PotatoEQProcessor& p) : proc (p)
{
    fillCombo (profile, proc.apvts, params::id::autoProfile);
    fillCombo (mode, proc.apvts, params::id::autoMode);
    aProfile = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, params::id::autoProfile, profile);
    aMode = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, params::id::autoMode, mode);
    amount.setSliderStyle (Slider::LinearHorizontal);
    amount.setTextBoxStyle (Slider::TextBoxRight, false, 52, 18);
    amount.setColour (Slider::trackColourId, theme::autoCol);
    amount.setTextValueSuffix (" %");
    aAmount = std::make_unique<APVTS::SliderAttachment> (proc.apvts, params::id::autoAmount, amount);
    for (auto* c : { (Component*) &profile, (Component*) &mode, (Component*) &amount, (Component*) &relearn, (Component*) &commit }) addAndMakeVisible (*c);

    setupSmallSlider (*this, tilt, tiltL, "Brightness", theme::autoCol);
    setupSmallSlider (*this, bass, bassL, "Low end", theme::autoCol);
    setupSmallSlider (*this, boost, boostL, "Max boost", theme::autoCol);
    setupSmallSlider (*this, cut, cutL, "Max cut", theme::autoCol);
    aTilt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, params::id::autoTilt, tilt);
    aBass = std::make_unique<APVTS::SliderAttachment> (proc.apvts, params::id::autoBass, bass);
    aBoost = std::make_unique<APVTS::SliderAttachment> (proc.apvts, params::id::autoMaxBoost, boost);
    aCut = std::make_unique<APVTS::SliderAttachment> (proc.apvts, params::id::autoMaxCut, cut);
    fillCombo (smooth, proc.apvts, params::id::autoSmooth);
    aSmooth = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, params::id::autoSmooth, smooth);
    addAndMakeVisible (smooth);
    tilt.setTooltip ("Tilts the target curve: negative = darker/warmer, positive = brighter.");
    bass.setTooltip ("Asks for more (or less) low end than the profile.");
    smooth.setTooltip ("Broad follows only the overall tonal balance; Fine also corrects narrower bumps.");
    mode.setTooltip ("Learn: listens for ~12 s of audio, then holds the EQ.  Live: keeps listening and adapts as the song changes.");
    commit.setTooltip ("Copies the auto EQ into regular bands you can tweak, then switches the auto EQ off.");
    relearn.onClick = [this] { proc.startLearning(); status = "Listening..."; };
    addChildComponent (loadRef);
    loadRef.setTooltip ("Pick a finished song you like (WAV, AIFF, FLAC, OGG or MP3). The Auto EQ steers your track toward its tonal balance.");
    loadRef.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Choose a reference track", File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile()) { proc.loadReference (f); status = {}; }
        });
    };
    commit.onClick = [this]
    {
        const int n = proc.commitAutoToBands();
        status = n > 0 ? "Added " + String (n) + " band" + (n == 1 ? "" : "s") + ". Auto EQ is now off." : "Nothing to convert yet.";
    };
    startTimerHz (10);
}

AutoPanel::~AutoPanel() { stopTimer(); }

void AutoPanel::timerCallback()
{
    snap = proc.autoSnapshot();
    if (proc.usingReference() != refShown) { refShown = proc.usingReference(); loadRef.setVisible (refShown); resized(); }
    if (refShown) status = proc.referenceStatus();
    const int m = mode.getSelectedId() - 1;
    relearn.setEnabled (m != params::autoOff);
    commit.setEnabled (m != params::autoOff && snap.result.valid);
    repaint();
}

void AutoPanel::paint (Graphics& g)
{
    theme::drawGlassPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().reduced (12, 10);
    g.setColour (theme::text); g.setFont (theme::font (13.0f, true));
    g.drawText ("AUTO EQ", r.removeFromTop (18), Justification::centredLeft);
    r.removeFromTop (24 + 6 + 24 + 6 + 22 + 6 + 5 * 22 + 6);

    auto graph = r.removeFromTop (r.getHeight() - 40).toFloat();
    g.setColour (theme::bg); g.fillRoundedRectangle (graph, 4);
    const int m = mode.getSelectedId() - 1;

    auto xFor = [&] (int i) { return graph.getX() + graph.getWidth() * (float) i / (arc::kSpecBins - 1); };
    if (m == params::autoOff)
    {
        g.setColour (theme::textDim); g.setFont (theme::font (12.0f));
        g.drawFittedText ("Pick what you're EQing, then set the mode to Learn (or Live) and play the track.", graph.reduced (10).toNearestInt(), Justification::centred, 3);
    }
    else if (! snap.result.valid)
    {
        g.setColour (theme::textDim); g.setFont (theme::font (12.0f));
        g.drawText ("Listening...  play your audio", graph.withTrimmedBottom (graph.getHeight() / 2), Justification::centredBottom);
        auto bar = graph.withSizeKeepingCentre (graph.getWidth() - 40, 6).translated (0, 10);
        g.setColour (theme::grid); g.fillRoundedRectangle (bar, 3);
        g.setColour (theme::autoCol); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * snap.progress), 3);
    }
    else
    {
        // measured vs target (normalised), and the correction
        double lo = 1e9, hi = -1e9;
        for (int i = 0; i < arc::kSpecBins; ++i)
        {
            lo = jmin (lo, jmin (snap.result.measuredDb[(size_t) i], snap.result.targetDb[(size_t) i]));
            hi = jmax (hi, jmax (snap.result.measuredDb[(size_t) i], snap.result.targetDb[(size_t) i]));
        }
        lo = jmax (lo, hi - 48.0);
        auto yFor = [&] (double v) { return graph.getBottom() - 6 - (graph.getHeight() - 12) * (float) jlimit (0.0, 1.0, (v - lo) / jmax (1.0, hi - lo)); };
        auto line = [&] (const std::array<double, arc::kSpecBins>& a, Colour c, float w)
        {
            Path p;
            for (int i = 0; i < arc::kSpecBins; ++i) { const float y = yFor (a[(size_t) i]); if (i == 0) p.startNewSubPath (xFor (i), y); else p.lineTo (xFor (i), y); }
            g.setColour (c); g.strokePath (p, PathStrokeType (w));
        };
        line (snap.result.targetDb, theme::textDim, 1.5f);
        line (snap.result.measuredDb, theme::postSpec, 1.5f);
        const float zero = graph.getCentreY();
        Path c;
        for (int i = 0; i < arc::kSpecBins; ++i)
        {
            const float y = zero - (float) snap.result.correctionDb[(size_t) i] * graph.getHeight() / 30.0f;
            if (i == 0) c.startNewSubPath (xFor (i), y); else c.lineTo (xFor (i), y);
        }
        g.setColour (theme::autoCol); g.strokePath (c, PathStrokeType (1.8f));
        g.setFont (theme::font (10.0f));
        auto legend = graph.reduced (6, 3).removeFromBottom (12);
        g.setColour (theme::postSpec); g.drawText ("your audio", legend.removeFromLeft (64), Justification::left);
        g.setColour (theme::textDim);  g.drawText ("target", legend.removeFromLeft (44), Justification::left);
        g.setColour (theme::autoCol);  g.drawText ("correction", legend.removeFromLeft (64), Justification::left);
        if (snap.learning || (m == params::autoLearn && snap.progress < 1.0f))
        {
            g.setColour (theme::textDim);
            g.drawText ("learning " + String ((int) (snap.progress * 100)) + "%", graph.reduced (6, 3), Justification::bottomRight);
        }
        else if (m == params::autoLive)
        {
            g.setColour (theme::autoCol.withAlpha (0.8f));
            g.drawText ("live", graph.reduced (6, 3), Justification::bottomRight);
        }
    }

    g.setColour (theme::textDim); g.setFont (theme::font (11.0f));
    g.drawText ("Detail", smooth.getBounds().withX (getLocalBounds().getX() + 15).withWidth (70), Justification::centredLeft);
    if (status.isNotEmpty())
        g.drawText (status, getLocalBounds().reduced (12, 6).removeFromBottom (14), Justification::centredLeft);
}

void AutoPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromTop (18);
    auto prow = r.removeFromTop (24);
    if (refShown) { loadRef.setBounds (prow.removeFromRight (104)); prow.removeFromRight (6); }
    profile.setBounds (prow); r.removeFromTop (6);
    auto row = r.removeFromTop (24);
    mode.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); relearn.setBounds (row);
    r.removeFromTop (6);
    amount.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    for (auto pr : { std::pair<Label*, Slider*> { &tiltL, &tilt }, { &bassL, &bass }, { &boostL, &boost }, { &cutL, &cut } })
    {
        auto row2 = r.removeFromTop (22);
        pr.first->setBounds (row2.removeFromLeft (80)); pr.second->setBounds (row2);
    }
    auto srow = r.removeFromTop (22).reduced (0, 1);
    smooth.setBounds (srow.withTrimmedLeft (80));
    auto bottom = getLocalBounds().reduced (12, 8).withTrimmedBottom (16).removeFromBottom (24);
    commit.setBounds (bottom);
}

// ---------------------------------------------------------------- level meters
// Clean slim capsules. dB mode: the bar follows the sample peak with instant attack and a smooth
// 24 dB/s fall (same feel as the analyzer); a thin tick marks the held peak.
// LUFS mode: the bar follows momentary loudness refreshed every 10 ms; the tick is short-term.
LevelMeters::LevelMeters (PotatoEQProcessor& p) : proc (p)
{
    setTooltip ("Input / output level (and limiter gain reduction). Units follow the LUFS / dB switch. Click to reset peaks and clip lights.");
    startTimerHz (60);
}

LevelMeters::~LevelMeters() { stopTimer(); }

void LevelMeters::timerCallback()
{
    in = proc.inMeter.get();
    out = proc.meter.get();
    const bool db = ana::get (proc.apvts.state, ana::units, 0) == 1;
    const float fall = 24.0f / 60.0f;   // dB per frame
    auto follow = [&] (float& shown, float target)
    {
        if (target >= shown) shown = target;                       // instant rise
        else shown = jmax (target, shown - fall);                  // smooth fall
        shown = jmax (shown, -100.0f);
    };
    const float inPk = proc.inMeter.takeRecentPeakDb(), outPk = proc.meter.takeRecentPeakDb();
    follow (inShown, db ? inPk : in.momentaryFast);
    follow (outShown, db ? outPk : out.momentaryFast);
    gr = jmax (proc.limiterReductionDb(), gr - fall);
    repaint();
}

void LevelMeters::mouseDown (const MouseEvent&) { proc.resetMeter(); }

void LevelMeters::paint (Graphics& g)
{
    theme::drawGlassPanel (g, getLocalBounds().toFloat());
    const bool db = ana::get (proc.apvts.state, ana::units, 0) == 1;
    const bool lim = proc.apvts.getRawParameterValue (params::id::limiter)->load() > 0.5f;
    auto area = getLocalBounds().toFloat().reduced (8.0f, 14.0f);
    auto labels = area.removeFromBottom (30.0f);
    const float lo = db ? -60.0f : -40.0f;
    auto yFor = [&] (float v) { return area.getBottom() - area.getHeight() * jlimit (0.0f, 1.0f, (v - lo) / -lo); };

    // scale: a few quiet labels on the left
    auto scale = area.removeFromLeft (20.0f);
    g.setFont (theme::font (9.5f));
    const std::vector<float> marks = db ? std::vector<float> { 0, -12, -24, -48 } : std::vector<float> { 0, -10, -20, -30 };
    for (float v : marks)
    {
        g.setColour (theme::gridText.withAlpha (0.8f));
        g.drawText (String ((int) v), Rectangle<float> (scale.getX() - 4, yFor (v) - 5, 20, 10), Justification::centredRight);
    }

    const int nBars = lim ? 3 : 2;
    const float slot = area.getWidth() / nBars, w = jmin (9.0f, slot - 6.0f);
    auto bar = [&] (int i) { return Rectangle<float> (area.getX() + slot * i + (slot - w) * 0.5f, area.getY(), w, area.getHeight()); };
    auto caption = [&] (int i, const String& name, const String& value, Colour vc)
    {
        const auto b = bar (i);
        auto r = Rectangle<float> (b.getCentreX() - slot * 0.5f, labels.getY() + 4, slot, 12);
        g.setColour (theme::textDim); g.setFont (theme::font (9.5f, true));
        g.drawText (name, r, Justification::centred);
        g.setColour (vc); g.setFont (theme::font (9.5f));
        g.drawText (value, r.translated (0, 12), Justification::centred);
    };
    auto track = [&] (Rectangle<float> b)
    {
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (b, w * 0.5f);
    };
    // colour by level: accent body, warming up near the top of the scale
    const float warnAt = db ? -6.0f : -9.0f;
    ColourGradient grad (theme::bad, 0, yFor (0), theme::accent.darker (0.35f), 0, area.getBottom(), false);
    grad.addColour (jlimit (0.0, 1.0, (double) ((yFor (warnAt) - yFor (0)) / area.getHeight())), theme::warn);
    grad.addColour (jlimit (0.0, 1.0, (double) ((yFor (-18.0f) - yFor (0)) / area.getHeight())), theme::accent);

    auto levelBar = [&] (int i, float level, float tick, bool clipped)
    {
        const auto b = bar (i);
        track (b);
        const float y = yFor (level);
        if (y < b.getBottom() - 0.5f)
        {
            const auto fillR = b.withTop (jmin (y, b.getBottom() - w));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillR, w * 0.5f);
            g.setColour (Colours::white.withAlpha (0.35f));   // soft glow at the top of the bar
            g.fillEllipse (Rectangle<float> (w, w * 0.6f).withCentre ({ b.getCentreX(), fillR.getY() + w * 0.35f }));
        }
        if (tick > lo) { g.setColour (theme::text.withAlpha (0.85f)); g.fillRoundedRectangle (Rectangle<float> (w + 4, 2.0f).withCentre ({ b.getCentreX(), yFor (tick) }), 1.0f); }
        if (clipped) { g.setColour (theme::bad); g.fillEllipse (Rectangle<float> (5, 5).withCentre ({ b.getCentreX(), b.getY() - 6 })); }
    };
    auto valueText = [] (float v, bool dbMode) { return v <= (dbMode ? -99.0f : -69.0f) ? String ("-") : String (v, 1); };

    levelBar (0, inShown, db ? in.peakHoldDb : in.shortTerm, in.clipped);
    levelBar (1, outShown, db ? out.peakHoldDb : out.shortTerm, out.clipped);
    caption (0, "IN", valueText (db ? in.peakHoldDb : in.shortTerm, db), in.clipped ? theme::bad : theme::textDim);
    caption (1, "OUT", valueText (db ? out.peakHoldDb : out.shortTerm, db), out.clipped ? theme::bad : theme::textDim);
    if (! db)
    {   // loudness target across the bars
        const float yt = yFor (proc.targetLufs());
        g.setColour (theme::accent2.withAlpha (0.8f));
        g.fillRoundedRectangle (Rectangle<float> (bar (0).getX() - 3, yt - 0.75f, bar (1).getRight() - bar (0).getX() + 6, 1.5f), 0.75f);
    }
    if (lim)
    {   // gain reduction grows down from the top, 0..-12 dB
        const auto b = bar (2);
        track (b);
        const float h = b.getHeight() * jlimit (0.0f, 1.0f, gr / 12.0f);
        if (h > 0.5f)
        {
            g.setColour (theme::autoCol);
            g.fillRoundedRectangle (b.withHeight (jmax (h, w)), w * 0.5f);
        }
        caption (2, "GR", gr < 0.05f ? String ("0.0") : String (-gr, 1), theme::autoCol);
    }
}

// ---------------------------------------------------------------- interface fonts
#include "BinaryData.h"
void theme::setUiFont (int index)
{
    struct Pair { const char* r; int rn; const char* b; int bn; };
    const Pair fonts[] = {
        { nullptr, 0, nullptr, 0 },
        { BinaryData::NunitoRegular_ttf, BinaryData::NunitoRegular_ttfSize, BinaryData::NunitoBold_ttf, BinaryData::NunitoBold_ttfSize },
        { BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize, BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize },
        { BinaryData::DMSansRegular_ttf, BinaryData::DMSansRegular_ttfSize, BinaryData::DMSansBold_ttf, BinaryData::DMSansBold_ttfSize },
        { BinaryData::ManropeRegular_ttf, BinaryData::ManropeRegular_ttfSize, BinaryData::ManropeBold_ttf, BinaryData::ManropeBold_ttfSize },
        { BinaryData::QuicksandRegular_ttf, BinaryData::QuicksandRegular_ttfSize, BinaryData::QuicksandBold_ttf, BinaryData::QuicksandBold_ttfSize } };
    const auto& f = fonts[jlimit (0, (int) std::size (fonts) - 1, index)];
    uiRegular() = f.r != nullptr ? Typeface::createSystemTypefaceFor (f.r, (size_t) f.rn) : nullptr;
    uiBold() = f.b != nullptr ? Typeface::createSystemTypefaceFor (f.b, (size_t) f.bn) : nullptr;
    const float scales[] = { 1.0f, 1.16f, 1.06f, 1.14f, 1.10f, 1.16f };   // match the classic font's visual size
    uiFontScale() = scales[jlimit (0, (int) std::size (scales) - 1, index)];
}
