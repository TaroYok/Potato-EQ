#include "PluginEditor.h"
#include "Presets.h"
#include <map>

using namespace juce;

PotatoEQEditor::PotatoEQEditor (PotatoEQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), bandPanel (p), loudness (p), autoPanel (p)
{
    theme::applySchemeFromState (proc.apvts.state);
    ++theme::openEditors();
    theme::setUiFont (ana::get (proc.apvts.state, ana::uiFont, theme::kDefaultFont));
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
    }
    aScale = std::make_unique<APVTS::SliderAttachment> (s, params::id::gainScale, scale);
    aOut = std::make_unique<APVTS::SliderAttachment> (s, params::id::outGain, out);
    scale.setTooltip ("Scales the gain of every band (100% = as drawn).");
    scaleL.setText ("Scale", dontSendNotification); outL.setText ("Output", dontSendNotification);
    aAutoGain = std::make_unique<APVTS::ButtonAttachment> (s, params::id::autoGain, autoGain);
    autoGain.setTooltip ("Keeps the perceived level steady while you EQ, so louder doesn't fool you into thinking better.");

    // analyzer extras: freeze is momentary (not saved), the rest is saved with the project
    proc.apvts.state.setProperty (ana::freeze, 0, nullptr);
    freeze.setTooltip ("Freeze the spectrum analyzer.");
    freeze.onClick = [this] { proc.apvts.state.setProperty (ana::freeze, freeze.getToggleState() ? 1 : 0, nullptr); };
    anaMenu.setTooltip ("Analyzer resolution, speed, tilt, range, peak hold, colours, font and background.");
    anaMenu.onClick = [this] { showAnalyzerMenu(); };
    presetBtn.setTooltip ("Factory presets: character EQs, instruments, vocals, mastering and filter sweeps.");
    presetBtn.onClick = [this] { showPresetMenu(); };
    if (proc.getCurrentProgram() > 0) presetBtn.setButtonText (presets::all()[(size_t) proc.getCurrentProgram()].name);

    // settings live behind the cog, in a glass sheet under it
    phaseL.setText ("Phase", dontSendNotification); analyzerL.setText ("Analyzer", dontSendNotification); rangeL.setText ("Range", dontSendNotification);
    for (auto* l : { &phaseL, &analyzerL, &rangeL, &scaleL, &outL })
    {
        l->setColour (Label::textColourId, theme::textDim);
        l->setJustificationType (Justification::centredLeft);
        settings.addAndMakeVisible (*l);
    }
    for (auto* c : { (Component*) &phase, (Component*) &analyzer, (Component*) &range, (Component*) &autoGain,
                     (Component*) &freeze, (Component*) &anaMenu, (Component*) &scale, (Component*) &out })
        settings.addAndMakeVisible (*c);
    cogBtn.setTooltip ("Settings: phase mode, analyzer, range, auto gain, output and the Display menu.");
    cogBtn.setClickingTogglesState (false);
    cogBtn.onClick = [this] { showSettings (! settings.isVisible()); };
    addBtn.setTooltip ("Add a module: the loudness meter or the Auto EQ.");
    addBtn.onClick = [this] { showModuleMenu(); };

    addAndMakeVisible (presetBtn);
    addAndMakeVisible (display);
    addAndMakeVisible (meters);
    addAndMakeVisible (dock);
    addChildComponent (bandPanel);   // floats over the display while a band is selected
    addChildComponent (loudness);
    addChildComponent (autoPanel);
    addChildComponent (settings);
    addAndMakeVisible (cogBtn);
    addAndMakeVisible (addBtn);

    dock.onClick = [this] (int m) { openModule (openSheet == m ? -1 : m); };
    display.onSelectionChanged = [this] (int b) { bandPanel.setBand (b); layoutOverlays(); };
    bandPanel.onDelete = [this] (int b)
    {
        proc.setParam (params::bandId (b, "used"), 0.0f);
        proc.setParam (params::bandId (b, "gain"), 0.0f);
        display.setSelected (-1);
    };

    setResizable (true, true);
    setResizeLimits (1100, 700, 2000, 1300);
    setSize (1360, 860);
    syncModules();
    startTimerHz (8);
}

bool PotatoEQEditor::loudnessAdded() const { return ana::get (proc.apvts.state, ana::modLoudness, 1) != 0; }
bool PotatoEQEditor::autoEqAdded() const
{
    if (auto* p = proc.apvts.getRawParameterValue (params::id::experimental)) return p->load() > 0.5f;
    return false;
}

void PotatoEQEditor::timerCallback() { syncModules(); }

// keep the dock (and any open sheet) in step with which modules are added, including after automation or a preset
void PotatoEQEditor::syncModules()
{
    std::vector<ModuleDock::Item> items;
    if (loudnessAdded()) items.push_back ({ ModuleDock::loudnessModule, "Loudness" });
    if (autoEqAdded()) items.push_back ({ ModuleDock::autoEqModule, "Auto EQ" });
    if ((openSheet == ModuleDock::loudnessModule && ! loudnessAdded()) || (openSheet == ModuleDock::autoEqModule && ! autoEqAdded()))
        openSheet = -1;
    const bool changed = items.size() != (size_t) dockCount || openSheet != dockOpen;
    dockCount = (int) items.size(); dockOpen = openSheet;
    dock.setItems (std::move (items), openSheet);
    if (changed) layoutOverlays();
}

void PotatoEQEditor::openModule (int m)
{
    openSheet = m;
    syncModules();
    layoutOverlays();
}

void PotatoEQEditor::showSettings (bool on)
{
    settings.setVisible (on);
    cogBtn.setToggleState (on, dontSendNotification);
    if (on) settings.toFront (false);
}

void PotatoEQEditor::showModuleMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Modules");
    m.addItem (1, "Loudness meter (LUFS, auto level, limiter)", true, loudnessAdded());
    m.addItem (2, "Auto EQ (experimental)", true, autoEqAdded());
    SafePointer<PotatoEQEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&addBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r == 1)
        {
            const bool add = ! safe->loudnessAdded();
            pr.apvts.state.setProperty (ana::modLoudness, add ? 1 : 0, nullptr);
            safe->openModule (add ? (int) ModuleDock::loudnessModule : -1);
        }
        else if (r == 2)
        {
            const bool add = ! safe->autoEqAdded();
            pr.setParam (params::id::experimental, add ? 1.0f : 0.0f);
            safe->openModule (add ? (int) ModuleDock::autoEqModule : -1);
        }
    });
}

void PotatoEQEditor::setBackdrop (int i)
{
    proc.apvts.state.setProperty (ana::backdrop, i, nullptr);
    refreshLook();
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
        const int cur = ana::get (st, ana::palette, ana::kDefaultPalette);
        for (int i = 0; i < (int) theme::palettePresets().size(); ++i) col.addItem (800 + i, theme::palettePresets()[(size_t) i].name, true, cur == i);
        col.addSeparator();
        col.addItem (899, "Custom colours...", true, cur >= (int) theme::palettePresets().size());
    }
    {
        const int cur = ana::get (st, ana::scheme, theme::kDefaultScheme);
        sch.addSectionHeader ("Themes (colours + scene)");
        for (int i = theme::kFirstTheme; i < theme::numSchemeSlots(); ++i) sch.addItem (950 + i, theme::schemeAt (i)->name, true, cur == i);
        sch.addSectionHeader ("Colour schemes");
        for (int i = 0; i < (int) theme::schemes().size(); ++i) sch.addItem (950 + i, theme::schemes()[(size_t) i].name, true, cur == i);
        sch.addSeparator();
        sch.addItem (990, "Custom colours...", true, cur == theme::kCustomScheme);
    }
    m.addSectionHeader ("Look");
    m.addSubMenu ("Theme", sch);
    {
        PopupMenu fm;
        const int cur = ana::get (st, ana::uiFont, theme::kDefaultFont);
        for (int i = 0; i < theme::fontNames.size(); ++i) fm.addItem (1100 + i, theme::fontNames[i], true, cur == i);
        m.addSubMenu ("Font", fm);
    }
    {
        PopupMenu bd;
        const int cur = ana::get (st, ana::backdrop, 0);
        for (int i = 0; i < ana::backdropNames.size(); ++i) bd.addItem (1200 + i, ana::backdropNames[i], true, cur == i);
        m.addSubMenu ("Background", bd);
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
        if (r >= 1200) { safe->setBackdrop (r - 1200); return; }
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
    if (const auto* sc = theme::schemeAt (i))
    {
        st.setProperty (ana::palette, sc->palette, nullptr);   // matching analyzer colours (still changeable)
        if (sc->backdrop >= 0) { st.setProperty (ana::backdrop, sc->backdrop, nullptr); theme::backdropMode() = sc->backdrop; }
    }
    theme::applyScheme (i);
    lnf.refreshColours();
    backdropDirty = true;
    sendLookAndFeelChange();
    repaint();
}

void PotatoEQEditor::refreshLook()
{
    theme::applySchemeFromState (proc.apvts.state);
    lnf.refreshColours();
    backdropDirty = true;
    sendLookAndFeelChange();
    repaint();
}

void PotatoEQEditor::setUiFont (int i)
{
    proc.apvts.state.setProperty (ana::uiFont, i, nullptr);
    theme::setUiFont (i);
    backdropDirty = true;   // the logo is part of the cached backdrop
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
        const int cur = ana::get (state, ana::scheme, theme::kDefaultScheme);
        if (theme::schemeAt (cur) != nullptr)
        {
            const auto k = theme::slotsFromScheme (cur);
            for (int i = 0; i < theme::kCustomSlots; ++i) state.setProperty (ana::customSlot (i), (int64) k[(size_t) i].getARGB(), nullptr);
            state.setProperty (ana::scheme, theme::kCustomScheme, nullptr);
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
    // The backdrop is costly (gradients, bubbles) and shows through the see-through panels, so the
    // analyzer's frequent repaints would redraw it every frame: render it once into an image instead.
    const float sc = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int w = roundToInt ((float) getWidth() * sc), h = roundToInt ((float) getHeight() * sc);
    // rebuild when the size or anything the backdrop uses changes (scheme, custom colours, font)
    const auto key = String::toHexString ((int64) theme::bgTop.getARGB()) + String::toHexString ((int64) theme::bgBottom.getARGB())
                   + String::toHexString ((int64) theme::navTop.getARGB()) + String::toHexString ((int64) theme::navBottom.getARGB())
                   + String::toHexString ((int64) theme::accent.getARGB()) + String::toHexString ((int64) theme::accent2.getARGB())
                   + String::toHexString ((int64) theme::navText.getARGB()) + String (theme::uiFontScale()) + String (theme::backdropMode());
    if (backdropDirty || key != backdropKey || backdrop.getWidth() != w || backdrop.getHeight() != h)
    {
        backdropKey = key;
        backdrop = Image (Image::ARGB, jmax (1, w), jmax (1, h), true);
        Graphics ig (backdrop);
        ig.addTransform (AffineTransform::scale (sc));
        paintBackdrop (ig);
        backdropDirty = false;
    }
    g.drawImage (backdrop, getLocalBounds().toFloat());
}

void PotatoEQEditor::paintBackdrop (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // the scene behind all the glass: sky and nature, or the colour void (Display > Background)
    theme::paintScene (g, b, theme::backdropMode());

    // glossy glass title bar
    auto head = b.removeFromTop (44.0f).reduced (6.0f, 4.0f);
    g.setGradientFill (ColourGradient (theme::navTop.withAlpha (0.55f), 0, head.getY(), theme::navBottom.withAlpha (0.72f), 0, head.getBottom(), false));
    g.fillRoundedRectangle (head, theme::radius);
    auto under = head.reduced (2.0f).removeFromBottom (head.getHeight() * 0.4f);
    g.setGradientFill (ColourGradient (theme::accent.withAlpha (0.0f), 0, under.getY(), theme::accent.withAlpha (0.45f), 0, under.getBottom(), false));
    g.fillRoundedRectangle (under, theme::radius - 2.0f);
    theme::drawAeroSheen (g, head.reduced (1.0f), theme::radius - 1.0f, 1.25f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.7f), 0, head.getY(), theme::accent.withAlpha (0.6f), 0, head.getBottom(), false));
    g.drawRoundedRectangle (head.reduced (0.5f), theme::radius, 1.2f);

    // thin glass rim around the whole window
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRect (getLocalBounds().toFloat(), 1.0f);

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
    const auto eqCol = theme::accent.brighter (0.5f);
    g.setColour (eqCol.withAlpha (0.35f));   // soft glow behind "EQ"
    for (auto d : { Point<int> (-1, 0), Point<int> (1, 0), Point<int> (0, -1), Point<int> (0, 1) })
        g.drawText ("EQ", logo.withTrimmedLeft (84).translated (d.x, d.y), Justification::centredLeft);
    g.setColour (eqCol);
    g.drawText ("EQ", logo.withTrimmedLeft (84), Justification::centredLeft);
}

void PotatoEQEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (44).reduced (10, 8);
    top.removeFromLeft (128);
    presetBtn.setBounds (top.removeFromLeft (220));
    addBtn.setBounds (top.removeFromRight (34));
    top.removeFromRight (8);
    cogBtn.setBounds (top.removeFromRight (34));

    // settings sheet, hanging under the cog
    {
        const int w = 340, rowH = 26, gap = 8;
        settings.setBounds (cogBtn.getRight() - w + 6, 46, w, 12 + 6 * (rowH + gap) + 4);
        auto s = settings.getLocalBounds().reduced (14, 12);
        auto row = [&] { auto x = s.removeFromTop (rowH); s.removeFromTop (gap); return x; };
        auto r1 = row(); phaseL.setBounds (r1.removeFromLeft (78)); phase.setBounds (r1);
        auto r2 = row(); analyzerL.setBounds (r2.removeFromLeft (78)); anaMenu.setBounds (r2.removeFromRight (100)); r2.removeFromRight (6); analyzer.setBounds (r2);
        auto r3 = row(); rangeL.setBounds (r3.removeFromLeft (78)); freeze.setBounds (r3.removeFromRight (100)); r3.removeFromRight (6); range.setBounds (r3);
        auto r4 = row(); r4.removeFromLeft (78); autoGain.setBounds (r4);
        auto r5 = row(); scaleL.setBounds (r5.removeFromLeft (78)); scale.setBounds (r5);
        auto r6 = row(); outL.setBounds (r6.removeFromLeft (78)); out.setBounds (r6);
    }

    r.reduce (10, 0);
    auto bottom = r.removeFromBottom (84);
    const int dw = dock.preferredWidth();
    dock.setBounds (bottom.withSizeKeepingCentre (dw, 80));
    meters.setBounds (r.removeFromRight (78).withTrimmedBottom (4));
    r.removeFromRight (8);
    display.setBounds (r.withTrimmedBottom (4));
    layoutOverlays();
}

// Things that float over the analyzer: the band editor near the selected band, and the open module sheet.
void PotatoEQEditor::layoutOverlays()
{
    dock.setBounds (dock.getBounds().withSizeKeepingCentre (dock.preferredWidth(), dock.getHeight()));
    const auto d = display.getBounds();
    const int b = display.getSelected();
    if (b >= 0)
    {
        const int w = jmin (996, d.getWidth() - 24), h = BandPanel::kHeight;
        const int nodeX = d.getX() + roundToInt (display.bandNodePosition (b).x);
        const int x = jlimit (d.getX() + 12, d.getRight() - 12 - w, nodeX - w / 2);
        bandPanel.setBounds (x, d.getBottom() - h - 30, w, h);
    }
    bandPanel.setVisible (b >= 0);
    const int sw = 300, sx = d.getRight() - sw - 34, sy = d.getY() + 40;
    loudness.setBounds (sx, sy, sw, jmin (440, d.getHeight() - 60));
    autoPanel.setBounds (sx, sy, sw, jmin (400, d.getHeight() - 60));
    loudness.setVisible (openSheet == ModuleDock::loudnessModule);
    autoPanel.setVisible (openSheet == ModuleDock::autoEqModule);
    if (settings.isVisible()) settings.toFront (false);
}
