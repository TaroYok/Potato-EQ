#include "PluginEditor.h"
#include "Presets.h"
#include <map>

using namespace juce;

PotatoEQEditor::PotatoEQEditor (PotatoEQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), bandPanel (p), loudness (p), autoPanel (p)
{
    theme::applySchemeFromState (proc.apvts.state);
    ++theme::openEditors();
    theme::setUiFont (ana::get (proc.apvts.state, ana::uiFont, 4));
    lnf.refreshColours();
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    fillCombo (phase, s, params::id::phaseMode);   aPhase = std::make_unique<APVTS::ComboBoxAttachment> (s, params::id::phaseMode, phase);
    fillCombo (analyzer, s, params::id::analyzer); aAnalyzer = std::make_unique<APVTS::ComboBoxAttachment> (s, params::id::analyzer, analyzer);
    fillCombo (range, s, params::id::range);       aRange = std::make_unique<APVTS::ComboBoxAttachment> (s, params::id::range, range);
    phase.setTooltip ("Zero Latency: classic minimum-phase EQ.  Linear Phase: no phase shift (good for mastering), adds latency.");
    analyzer.setTooltip ("Spectrum analyzer: before and/or after the EQ.");
    range.setTooltip ("Vertical zoom of the EQ display.");

    for (auto* sl : { &scale, &out })
    {
        sl->setSliderStyle (Slider::LinearHorizontal);
        sl->setTextBoxStyle (Slider::TextBoxRight, false, 52, 18);
        sl->setColour (Slider::trackColourId, theme::accent);
        addAndMakeVisible (*sl);
    }
    aScale = std::make_unique<APVTS::SliderAttachment> (s, params::id::gainScale, scale);
    aOut = std::make_unique<APVTS::SliderAttachment> (s, params::id::outGain, out);
    scale.setTooltip ("Scales the gain of every band (100% = as drawn).");
    scaleL.setText ("Scale", dontSendNotification); outL.setText ("Out", dontSendNotification);
    for (auto* l : { &scaleL, &outL }) { l->setColour (Label::textColourId, theme::textDim); l->setJustificationType (Justification::centredRight); addAndMakeVisible (*l); }
    aAutoGain = std::make_unique<APVTS::ButtonAttachment> (s, params::id::autoGain, autoGain);
    aExperimental = std::make_unique<APVTS::ButtonAttachment> (s, params::id::experimental, experimental);
    experimental.setTooltip ("Shows tools that are still being tuned, like the Auto EQ. While this is off they're hidden and do nothing.");
    experimental.onStateChange = [this] { updateExperimental(); };
    addAndMakeVisible (experimental);
    autoGain.setTooltip ("Keeps the perceived level steady while you EQ, so louder doesn't fool you into thinking better.");

    // analyzer extras: freeze is momentary (not saved), the rest is saved with the project
    proc.apvts.state.setProperty (ana::freeze, 0, nullptr);
    freeze.setTooltip ("Freeze the spectrum analyzer.");
    freeze.onClick = [this] { proc.apvts.state.setProperty (ana::freeze, freeze.getToggleState() ? 1 : 0, nullptr); };
    anaMenu.setTooltip ("Analyzer resolution, speed, tilt, range and peak hold.");
    anaMenu.onClick = [this] { showAnalyzerMenu(); };
    presetBtn.setTooltip ("Factory presets: character EQs, instruments, vocals, mastering and filter sweeps.");
    presetBtn.onClick = [this] { showPresetMenu(); };
    if (proc.getCurrentProgram() > 0) presetBtn.setButtonText (presets::all()[(size_t) proc.getCurrentProgram()].name);

    for (auto* c : { (Component*) &phase, (Component*) &analyzer, (Component*) &range, (Component*) &autoGain,
                     (Component*) &freeze, (Component*) &anaMenu, (Component*) &presetBtn,
                     (Component*) &display, (Component*) &loudness, (Component*) &autoPanel })
        addAndMakeVisible (*c);
    addAndMakeVisible (bandPanel);   // fixed box under the display
    addAndMakeVisible (meters);

    display.onSelectionChanged = [this] (int b) { bandPanel.setBand (b); };
    bandPanel.onDelete = [this] (int b)
    {
        proc.setParam (params::bandId (b, "used"), 0.0f);
        proc.setParam (params::bandId (b, "gain"), 0.0f);
        display.setSelected (-1);
    };

    setResizable (true, true);
    setResizeLimits (1340, 800, 2000, 1300);
    autoPanel.setVisible (experimental.getToggleState());
    setSize (1360, 860);
}

void PotatoEQEditor::updateExperimental()
{
    const bool on = experimental.getToggleState();
    if (autoPanel.isVisible() != on) { autoPanel.setVisible (on); resized(); }
}

PotatoEQEditor::~PotatoEQEditor()
{
    setLookAndFeel (nullptr);
    // release the embedded fonts with the last window, before JUCE's own font cache goes away at unload
    if (--theme::openEditors() == 0) { theme::uiRegular() = nullptr; theme::uiBold() = nullptr; }
}


void PotatoEQEditor::showAnalyzerMenu()
{
    auto& st = proc.apvts.state;
    PopupMenu m, res, spd, tlt, rng, smo, vw, fl, col, sch;
    auto fill = [&] (PopupMenu& sub, const StringArray& names, const Identifier& id, int def, int base)
    {
        const int cur = ana::get (st, id, def);
        for (int i = 0; i < names.size(); ++i) sub.addItem (base + i, names[i], true, cur == i);
    };
    fill (res, ana::resolutionNames, ana::resolution, 2, 100);
    fill (spd, ana::speedNames, ana::speed, 1, 200);
    fill (tlt, ana::tiltNames, ana::tilt, 2, 300);
    fill (rng, ana::rangeNames, ana::range, 1, 400);
    fill (smo, ana::smoothingNames, ana::smoothing, 2, 500);
    fill (vw, ana::viewNames, ana::view, 0, 600);
    fill (fl, ana::fillNames, ana::fill, 1, 700);
    {
        const int cur = ana::get (st, ana::palette, 0);
        for (int i = 0; i < (int) theme::palettePresets().size(); ++i) col.addItem (800 + i, theme::palettePresets()[(size_t) i].name, true, cur == i);
        col.addSeparator();
        col.addItem (899, "Custom colours...", true, cur >= (int) theme::palettePresets().size());
    }
    {
        const int cur = ana::get (st, ana::scheme, 0);
        for (int i = 0; i < (int) theme::schemes().size(); ++i) sch.addItem (950 + i, theme::schemes()[(size_t) i].name, true, cur == i);
        sch.addSeparator();
        sch.addItem (990, "Custom colours...", true, cur >= (int) theme::schemes().size());
    }
    m.addSectionHeader ("Look");
    m.addSubMenu ("Colour scheme", sch);
    {
        PopupMenu fm;
        const int cur = ana::get (st, ana::uiFont, 4);
        for (int i = 0; i < theme::fontNames.size(); ++i) fm.addItem (1100 + i, theme::fontNames[i], true, cur == i);
        m.addSubMenu ("Font", fm);
    }
    {
        PopupMenu un;
        const int cur = ana::get (st, ana::units, 0);
        for (int i = 0; i < ana::unitNames.size(); ++i) un.addItem (1000 + i, ana::unitNames[i], true, cur == i);
        m.addSubMenu ("Meter units", un);
    }
    m.addSubMenu ("View", vw);
    m.addSubMenu ("Fill style", fl);
    m.addSubMenu ("Colours", col);
    m.addSectionHeader ("Analyzer");
    m.addSubMenu ("Resolution", res);
    m.addSubMenu ("Speed", spd);
    m.addSubMenu ("Tilt", tlt);
    m.addSubMenu ("Range", rng);
    m.addSubMenu ("Smoothing", smo);
    m.addSeparator();
    m.addItem (1, "Peak hold", true, ana::get (st, ana::peakHold, 0) != 0);
    m.addItem (2, "Clear peak hold");
    m.addItem (6, "Note grid (C1, C2 ...)", true, ana::get (st, ana::noteGrid, 1) != 0);
    m.addSeparator();
    m.addItem (3, "Show sidechain spectrum", true, ana::get (st, ana::showSc, 1) != 0);
    m.addItem (5, "Highlight collisions with sidechain", true, ana::get (st, ana::collide, 1) != 0);
    SafePointer<PotatoEQEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&anaMenu), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& s = safe->proc.apvts.state;
        if (r >= 1100) { safe->setUiFont (r - 1100); return; }
        if (r >= 1000) { s.setProperty (ana::units, r - 1000, nullptr); return; }
        if (r == 990) { safe->showSchemeDialog(); return; }
        if (r >= 950) { safe->setScheme (r - 950); return; }
        if (r == 899) { safe->showColourDialog(); return; }
        if (r >= 800) { s.setProperty (ana::palette, r - 800, nullptr); return; }
        if (r >= 700) { s.setProperty (ana::fill, r - 700, nullptr); return; }
        if (r >= 600) { s.setProperty (ana::view, r - 600, nullptr); return; }
        if (r == 1) { s.setProperty (ana::peakHold, ana::get (s, ana::peakHold, 0) ? 0 : 1, nullptr); safe->display.clearPeakHold(); }
        else if (r == 2) safe->display.clearPeakHold();
        else if (r == 6) s.setProperty (ana::noteGrid, ana::get (s, ana::noteGrid, 1) ? 0 : 1, nullptr);
        else if (r == 3) s.setProperty (ana::showSc, ana::get (s, ana::showSc, 1) ? 0 : 1, nullptr);
        else if (r == 5) s.setProperty (ana::collide, ana::get (s, ana::collide, 1) ? 0 : 1, nullptr);
        else if (r >= 500) s.setProperty (ana::smoothing, r - 500, nullptr);
        else if (r >= 400) s.setProperty (ana::range, r - 400, nullptr);
        else if (r >= 300) s.setProperty (ana::tilt, r - 300, nullptr);
        else if (r >= 200) s.setProperty (ana::speed, r - 200, nullptr);
        else if (r >= 100) s.setProperty (ana::resolution, r - 100, nullptr);
    });
}

void PotatoEQEditor::showPresetMenu()
{
    PopupMenu m;
    std::map<String, PopupMenu> cats;
    StringArray order;
    const auto& all = presets::all();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const String c = all[(size_t) i].category;
        if (! order.contains (c)) order.add (c);
        cats[c].addItem (i + 1, all[(size_t) i].name, true, proc.getCurrentProgram() == i);
    }
    for (auto& c : order) m.addSubMenu (c, cats[c]);
    SafePointer<PotatoEQEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        const auto& p = presets::all()[(size_t) (r - 1)];
        safe->proc.applyPreset (r - 1);
        safe->presetBtn.setButtonText (p.name);
        safe->display.setSelected (p.selectBand);
        if (String (p.hint).isNotEmpty()) safe->display.showMessage (String (p.name) + ":  " + p.hint);
    });
}

// Pick the lightest (loud) and darkest (quiet) colour of the analyzer gradient.
class ColourDialog : public Component, private ChangeListener
{
public:
    explicit ColourDialog (ValueTree st) : state (st)
    {
        const auto pal = theme::paletteFrom (state);
        for (auto* cs : { &light, &dark })
        {
            cs->setColour (ColourSelector::backgroundColourId, Colours::transparentBlack);
            cs->addChangeListener (this);
            addAndMakeVisible (*cs);
        }
        light.setCurrentColour (pal.light, dontSendNotification);
        dark.setCurrentColour (pal.dark, dontSendNotification);
        setSize (520, 330);
    }
    void paint (Graphics& g) override
    {
        g.setColour (theme::text); g.setFont (theme::font (13.0f, true));
        g.drawText ("Lightest  (loud)", 10, 6, 240, 18, Justification::centredLeft);
        g.drawText ("Darkest  (quiet)", 270, 6, 240, 18, Justification::centredLeft);
        const auto pal = theme::paletteFrom (state);
        auto bar = Rectangle<float> (10, (float) getHeight() - 28, (float) getWidth() - 20, 16);
        ColourGradient grad (pal.light, bar.getX(), 0, pal.dark, bar.getRight(), 0, false);
        grad.addColour (0.5, pal.mid);
        g.setGradientFill (grad); g.fillRoundedRectangle (bar, 8);
    }
    void resized() override
    {
        light.setBounds (10, 26, 240, getHeight() - 64);
        dark.setBounds (270, 26, 240, getHeight() - 64);
    }

private:
    void changeListenerCallback (ChangeBroadcaster*) override
    {
        state.setProperty (ana::customLight, (int64) light.getCurrentColour().getARGB(), nullptr);
        state.setProperty (ana::customDark, (int64) dark.getCurrentColour().getARGB(), nullptr);
        state.setProperty (ana::palette, (int) theme::palettePresets().size(), nullptr);   // = custom
        repaint();
    }
    ValueTree state;
    ColourSelector light { ColourSelector::showColourspace | ColourSelector::showColourAtTop },
                   dark  { ColourSelector::showColourspace | ColourSelector::showColourAtTop };
};

void PotatoEQEditor::setScheme (int i)
{
    auto& st = proc.apvts.state;
    st.setProperty (ana::scheme, i, nullptr);
    st.setProperty (ana::palette, theme::schemes()[(size_t) i].palette, nullptr);   // matching analyzer colours (still changeable)
    theme::applyScheme (i);
    lnf.refreshColours();
    sendLookAndFeelChange();
    repaint();
}

void PotatoEQEditor::refreshLook()
{
    theme::applySchemeFromState (proc.apvts.state);
    lnf.refreshColours();
    sendLookAndFeelChange();
    repaint();
}

void PotatoEQEditor::setUiFont (int i)
{
    proc.apvts.state.setProperty (ana::uiFont, i, nullptr);
    theme::setUiFont (i);
    sendLookAndFeelChange();
    resized();
    repaint();
}

// Custom interface colours: pick a part of the interface on the left, its colour on the right.
class SchemeDialog : public Component, private ChangeListener
{
public:
    SchemeDialog (ValueTree st, std::function<void()> changed) : state (st), onChanged (std::move (changed))
    {
        // start from whatever is on screen right now
        const int cur = ana::get (state, ana::scheme, 0);
        if (cur < (int) theme::schemes().size())
        {
            const auto k = theme::slotsFromScheme (cur);
            for (int i = 0; i < theme::kCustomSlots; ++i) state.setProperty (ana::customSlot (i), (int64) k[(size_t) i].getARGB(), nullptr);
            state.setProperty (ana::scheme, (int) theme::schemes().size(), nullptr);
            onChanged();
        }
        for (int i = 0; i < theme::kCustomSlots; ++i)
        {
            auto* b = slots.add (new TextButton (theme::customSlotNames[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (77);
            b->onClick = [this, i] { select (i); };
            addAndMakeVisible (b);
        }
        picker.setColour (ColourSelector::backgroundColourId, Colours::transparentBlack);
        picker.addChangeListener (this);
        addAndMakeVisible (picker);
        setSize (470, 300);
        slots[0]->setToggleState (true, dontSendNotification);
        select (0);
    }
    void paint (Graphics& g) override
    {
        for (int i = 0; i < slots.size(); ++i)
        {
            auto sw = slots[i]->getBounds().toFloat().removeFromRight (18.0f).reduced (3.0f, 5.0f);
            g.setColour (colourOf (i)); g.fillRoundedRectangle (sw, 3.0f);
            g.setColour (Colours::white.withAlpha (0.4f)); g.drawRoundedRectangle (sw, 3.0f, 1.0f);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto left = r.removeFromLeft (180);
        for (auto* b : slots) { b->setBounds (left.removeFromTop (28).withTrimmedRight (20)); left.removeFromTop (3); }
        picker.setBounds (r.withTrimmedLeft (8));
    }

private:
    Colour colourOf (int i) const { return Colour ((uint32) (int64) state.getProperty (ana::customSlot (i), (int64) 0xff808080)); }
    void select (int i) { current = i; picker.setCurrentColour (colourOf (i), dontSendNotification); repaint(); }
    void changeListenerCallback (ChangeBroadcaster*) override
    {
        state.setProperty (ana::customSlot (current), (int64) picker.getCurrentColour().getARGB(), nullptr);
        onChanged();
        repaint();
    }
    ValueTree state;
    std::function<void()> onChanged;
    OwnedArray<TextButton> slots;
    ColourSelector picker { ColourSelector::showColourspace | ColourSelector::showColourAtTop | ColourSelector::showSliders };
    int current = 0;
};

void PotatoEQEditor::showSchemeDialog()
{
    SafePointer<PotatoEQEditor> safe (this);
    CallOutBox::launchAsynchronously (std::make_unique<SchemeDialog> (proc.apvts.state, [safe] { if (safe != nullptr) safe->refreshLook(); }),
                                      anaMenu.getScreenBounds(), nullptr);
}

void PotatoEQEditor::showColourDialog()
{
    CallOutBox::launchAsynchronously (std::make_unique<ColourDialog> (proc.apvts.state), anaMenu.getScreenBounds(), nullptr);
}

void PotatoEQEditor::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // glossy black glass with a diagonal reflection, like the face of an iPod touch
    g.setGradientFill (ColourGradient (theme::bgTop, 0, 0, theme::bgBottom, 0, b.getBottom(), false));
    g.fillAll();
    Path refl;
    refl.startNewSubPath (0, 0); refl.lineTo (b.getWidth() * 0.62f, 0); refl.lineTo (b.getWidth() * 0.30f, b.getHeight()); refl.lineTo (0, b.getHeight()); refl.closeSubPath();
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.045f), 0, 0, Colours::white.withAlpha (0.0f), b.getWidth() * 0.45f, b.getHeight() * 0.6f, false));
    g.fillPath (refl);

    // iPhone OS style navigation bar: slate-blue gloss, chrome edge
    auto head = b.removeFromTop (44.0f).reduced (6.0f, 4.0f);
    g.setGradientFill (ColourGradient (theme::navTop, 0, head.getY(), theme::navBottom, 0, head.getBottom(), false));
    g.fillRoundedRectangle (head, theme::radius);
    auto gloss = head.withHeight (head.getHeight() * 0.5f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.30f), 0, gloss.getY(), Colours::white.withAlpha (0.08f), 0, gloss.getBottom(), false));
    g.fillRoundedRectangle (gloss, theme::radius);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (head, theme::radius, 1.0f);

    // engraved logo
    auto top = getLocalBounds().removeFromTop (44).reduced (16, 0);
    auto logo = top.removeFromLeft (124);
    g.setFont (theme::font (21.0f, true));
    const bool lightBar = theme::navText.getBrightness() < 0.5f;
    g.setColour (lightBar ? Colours::white.withAlpha (0.6f) : Colours::black.withAlpha (0.45f));   // engraving
    g.drawText ("POTATO", logo.translated (0, lightBar ? 1 : -1).withWidth (84), Justification::centredLeft);
    g.setColour (theme::navText);
    g.drawText ("POTATO", logo.withWidth (84), Justification::centredLeft);
    g.setFont (theme::font (21.0f));
    g.setColour (theme::navText.withAlpha (0.85f));
    g.drawText ("EQ", logo.withTrimmedLeft (84), Justification::centredLeft);
}

void PotatoEQEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (44).reduced (10, 9);
    top.removeFromLeft (128);
    presetBtn.setBounds (top.removeFromLeft (130)); top.removeFromLeft (8);
    phase.setBounds (top.removeFromLeft (118)); top.removeFromLeft (8);
    analyzer.setBounds (top.removeFromLeft (100)); top.removeFromLeft (6);
    anaMenu.setBounds (top.removeFromLeft (86)); top.removeFromLeft (6);
    freeze.setBounds (top.removeFromLeft (72)); top.removeFromLeft (8);
    range.setBounds (top.removeFromLeft (70)); top.removeFromLeft (10);
    autoGain.setBounds (top.removeFromLeft (96));
    experimental.setBounds (top.removeFromRight (140)); top.removeFromRight (10);
    out.setBounds (top.removeFromRight (128)); outL.setBounds (top.removeFromRight (32));
    top.removeFromRight (6);
    scale.setBounds (top.removeFromRight (128)); scaleL.setBounds (top.removeFromRight (40));

    r.reduce (10, 0); r.removeFromBottom (10);
    auto side = r.removeFromRight (290);
    r.removeFromRight (10);
    loudness.setBounds (autoPanel.isVisible() ? side.removeFromTop (440) : side);
    side.removeFromTop (10);
    autoPanel.setBounds (side);
    bandPanel.setBounds (r.removeFromBottom (BandPanel::kHeight + 8));
    r.removeFromBottom (8);
    meters.setBounds (r.removeFromRight (92));
    r.removeFromRight (8);
    display.setBounds (r);
}
