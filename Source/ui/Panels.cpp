#include "Panels.h"

using namespace juce;

// ---------------------------------------------------------------- look and feel
PotatoLookAndFeel::PotatoLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, theme::bg);
    setColour (ComboBox::backgroundColourId, theme::panelHi);
    setColour (ComboBox::outlineColourId, theme::grid);
    setColour (ComboBox::textColourId, theme::text);
    setColour (ComboBox::arrowColourId, theme::textDim);
    setColour (PopupMenu::backgroundColourId, theme::panel);
    setColour (PopupMenu::textColourId, theme::text);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (Label::textColourId, theme::text);
    setColour (Slider::textBoxTextColourId, theme::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (TextButton::buttonColourId, theme::panelHi);
    setColour (TextButton::textColourOffId, theme::text);
    setColour (ToggleButton::textColourId, theme::text);
    setColour (ToggleButton::tickColourId, theme::accent);
    setColour (TooltipWindow::backgroundColourId, theme::panel);
}

void PotatoLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (3);
    const float r = jmin (b.getWidth(), b.getHeight()) / 2.0f;
    const auto c = b.getCentre();
    const float ang = start + pos * (end - start);
    const auto col = s.findColour (Slider::rotarySliderFillColourId);

    Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, start, end, true);
    g.setColour (theme::grid); g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (start + end) / 2 : start;
    Path val; val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, jmin (from, ang), jmax (from, ang), true);
    g.setColour (s.isEnabled() ? col : theme::textDim); g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (theme::panelHi); g.fillEllipse (Rectangle<float> (2 * (r - 6), 2 * (r - 6)).withCentre (c));
    g.setColour (theme::text);
    g.drawLine (Line<float> (c, c.getPointOnCircumference (r - 8, ang)), 2.0f);
}

void PotatoLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    const auto b = Rectangle<int> (x, y, w, h).toFloat();
    const auto track = b.withSizeKeepingCentre (b.getWidth(), 4.0f);
    g.setColour (theme::grid); g.fillRoundedRectangle (track, 2);
    g.setColour (s.findColour (Slider::trackColourId)); g.fillRoundedRectangle (track.withRight (pos), 2);
    g.setColour (theme::text); g.fillEllipse (Rectangle<float> (12, 12).withCentre ({ pos, b.getCentreY() }));
}

void PotatoLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? theme::accent.withAlpha (0.35f) : base;
    if (down) c = c.brighter (0.2f); else if (over) c = c.brighter (0.08f);
    g.setColour (c); g.fillRoundedRectangle (r, 4);
    g.setColour (theme::grid.brighter (0.2f)); g.drawRoundedRectangle (r, 4, 1);
}

void PotatoLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (4);
    g.setColour (b.getToggleState() ? theme::accent : (over ? theme::panelHi.brighter (0.2f) : theme::panelHi));
    g.fillRoundedRectangle (box, 3);
    g.setColour (theme::grid.brighter (0.3f)); g.drawRoundedRectangle (box, 3, 1);
    g.setColour (b.isEnabled() ? theme::text : theme::textDim);
    g.setFont (theme::font (12.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4), Justification::centredLeft);
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
    g.setColour (theme::panel); g.fillRoundedRectangle (bounds, 6);
    if (band < 0)
    {
        g.setColour (theme::textDim); g.setFont (theme::font (13.0f));
        g.drawText ("Click a band to edit it here.  Double-click the display to add one, or drag a peak in the analyzer.",
                    getLocalBounds(), Justification::centred);
        return;
    }
    const auto c = theme::bandColour (band);
    g.setColour (c.withAlpha (0.45f)); g.drawRoundedRectangle (bounds, 6, 1.0f);
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
    aAutoLevel = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, params::id::autoLevel, autoLevel);
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
    const bool isCustom = params::lufsTargets()[(size_t) jlimit (0, (int) params::lufsTargets().size() - 1, target.getSelectedId() - 1)].lufs > 0;
    custom.setEnabled (isCustom); customL.setEnabled (isCustom);
    ceiling.setEnabled (limiterOn.getToggleState());
    if (++tick % 2 == 0)
    {
        history.push_back (r.momentary);
        if (history.size() > 300) history.erase (history.begin());
    }
    repaint();
}

void LoudnessPanel::paint (Graphics& g)
{
    g.setColour (theme::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6);
    auto r0 = getLocalBounds().reduced (12, 10);
    g.setColour (theme::text); g.setFont (theme::font (13.0f, true));
    g.drawText ("LOUDNESS  (output)", r0.removeFromTop (18), Justification::centredLeft);

    const float tgt = proc.targetLufs();
    auto fmt = [] (float v) { return v <= -69.9f ? String ("--") : String (v, 1); };

    auto big = r0.removeFromTop (54);
    g.setColour (theme::textDim); g.setFont (theme::font (10.5f, true));
    g.drawText ("INTEGRATED LUFS", big.removeFromTop (14), Justification::centredLeft);
    const float I = r.integratedValid ? r.integrated : -100.0f;
    const float diff = I - tgt;
    g.setColour (I < -69 ? theme::textDim : std::abs (diff) <= 1.0f ? theme::good : std::abs (diff) <= 3.0f ? theme::warn : theme::text);
    g.setFont (theme::font (34.0f, true));
    g.drawText (fmt (I), big.removeFromLeft (120), Justification::centredLeft);
    if (I > -69)
    {
        g.setFont (theme::font (12.0f)); g.setColour (theme::textDim);
        g.drawText (std::abs (diff) < 0.05f ? String ("on target") : (diff > 0 ? "+" : "") + String (diff, 1) + " LU vs target", big, Justification::centredLeft);
    }

    auto row = [&] (const String& name, const String& val, Colour c)
    {
        auto rr = r0.removeFromTop (18);
        g.setColour (theme::textDim); g.setFont (theme::font (12.0f));
        g.drawText (name, rr, Justification::centredLeft);
        g.setColour (c); g.setFont (theme::font (12.5f, true));
        g.drawText (val, rr, Justification::centredRight);
    };
    row ("Short-term (3 s)", fmt (r.shortTerm) + " LUFS", theme::text);
    row ("Momentary (400 ms)", fmt (r.momentary) + " LUFS", theme::text);
    row ("Loudness range", String (r.lra, 1) + " LU", theme::text);
    row ("True peak", fmt (r.truePeakDb) + " dBTP", r.truePeakDb > -1.0f ? theme::bad : theme::text);
    {
        String extra;
        if (std::abs (proc.autoLevelDb()) > 0.05f) extra << "Auto level " << (proc.autoLevelDb() > 0 ? "+" : "") << String (proc.autoLevelDb(), 1) << " dB";
        if (limiterOn.getToggleState()) extra << (extra.isEmpty() ? "" : "   ") << "Limiting " << String (grShown, 1) << " dB";
        row (extra.isEmpty() ? String() : String ("Gain"), extra, grShown > 3.0f ? theme::warn : theme::accent);
    }

    // history graph with the target line
    r0.removeFromTop (6);
    auto graph = r0.removeFromTop (60).toFloat();
    g.setColour (theme::bg); g.fillRoundedRectangle (graph, 4);
    auto yFor = [&] (float l) { return graph.getBottom() - graph.getHeight() * jlimit (0.0f, 1.0f, (l + 40.0f) / 40.0f); };
    g.setColour (theme::accent.withAlpha (0.5f)); g.drawHorizontalLine ((int) yFor (tgt), graph.getX(), graph.getRight());
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
    g.drawText ("last 30 s", graph.reduced (4, 2), Justification::topLeft);

    if (status.isNotEmpty())
    {
        g.setColour (theme::textDim); g.setFont (theme::font (11.0f));
        g.drawFittedText (status, getLocalBounds().reduced (12, 8).removeFromBottom (28), Justification::centredLeft, 2);
    }
}

void LoudnessPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
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
    g.setColour (theme::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6);
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
