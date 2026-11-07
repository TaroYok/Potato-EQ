#include "PluginEditor.h"
#include "Presets.h"
#include <map>

using namespace juce;

PotatoEQEditor::PotatoEQEditor (PotatoEQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), bandPanel (p), loudness (p), autoPanel (p)
{
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
        sl->setTextBoxStyle (Slider::TextBoxRight, false, 64, 18);
        sl->setColour (Slider::trackColourId, theme::accent);
        addAndMakeVisible (*sl);
    }
    aScale = std::make_unique<APVTS::SliderAttachment> (s, params::id::gainScale, scale);
    aOut = std::make_unique<APVTS::SliderAttachment> (s, params::id::outGain, out);
    scale.setTooltip ("Scales the gain of every band (100% = as drawn).");
    scaleL.setText ("Gain scale", dontSendNotification); outL.setText ("Output", dontSendNotification);
    for (auto* l : { &scaleL, &outL }) { l->setColour (Label::textColourId, theme::textDim); l->setJustificationType (Justification::centredRight); addAndMakeVisible (*l); }
    aAutoGain = std::make_unique<APVTS::ButtonAttachment> (s, params::id::autoGain, autoGain);
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
    addChildComponent (bandPanel);   // floats over the display next to the selected band
    startTimerHz (30);

    display.onSelectionChanged = [this] (int b) { bandPanel.setBand (b); };
    bandPanel.onDelete = [this] (int b)
    {
        proc.setParam (params::bandId (b, "used"), 0.0f);
        proc.setParam (params::bandId (b, "gain"), 0.0f);
        display.setSelected (-1);
    };

    setResizable (true, true);
    setResizeLimits (1240, 800, 2000, 1300);
    setSize (1360, 860);
}

PotatoEQEditor::~PotatoEQEditor() { stopTimer(); setLookAndFeel (nullptr); }

void PotatoEQEditor::timerCallback()
{
    const int b = display.getSelected();
    const bool show = b >= 0 && proc.bandSettings (b).used && ! display.isDragging();
    if (! show) { if (bandPanel.isVisible()) bandPanel.setVisible (false); return; }
    const auto d = display.getBounds();
    const auto node = display.bandNodePosition (b) + d.getPosition().toFloat();
    const int w = jmin (BandPanel::kWidth, d.getWidth() - 8), h = BandPanel::kHeight;
    int x = jlimit (d.getX() + 4, d.getRight() - w - 4, (int) node.x - w / 2);
    int y = node.y < d.getCentreY() ? (int) node.y + 22 : (int) node.y - 22 - h;   // below the node in the top half, above it in the bottom half
    y = jlimit (d.getY() + 4, d.getBottom() - h - 18, y);
    const Rectangle<int> r (x, y, w, h);
    if (bandPanel.getBounds() != r) bandPanel.setBounds (r);
    if (! bandPanel.isVisible()) { bandPanel.setVisible (true); bandPanel.toFront (false); }
}

void PotatoEQEditor::showAnalyzerMenu()
{
    auto& st = proc.apvts.state;
    PopupMenu m, res, spd, tlt, rng, smo;
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

void PotatoEQEditor::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    auto top = getLocalBounds().removeFromTop (44).reduced (14, 0);
    g.setColour (theme::accent); g.setFont (theme::font (20.0f, true));
    g.drawText ("POTATO", top.removeFromLeft (84), Justification::centredLeft);
    g.setColour (theme::text); g.setFont (theme::font (20.0f));
    g.drawText ("EQ", top.removeFromLeft (40), Justification::centredLeft);
}

void PotatoEQEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (44).reduced (10, 9);
    top.removeFromLeft (128);
    presetBtn.setBounds (top.removeFromLeft (150)); top.removeFromLeft (8);
    phase.setBounds (top.removeFromLeft (118)); top.removeFromLeft (8);
    analyzer.setBounds (top.removeFromLeft (100)); top.removeFromLeft (6);
    anaMenu.setBounds (top.removeFromLeft (86)); top.removeFromLeft (6);
    freeze.setBounds (top.removeFromLeft (72)); top.removeFromLeft (8);
    range.setBounds (top.removeFromLeft (70)); top.removeFromLeft (10);
    autoGain.setBounds (top.removeFromLeft (96));
    out.setBounds (top.removeFromRight (150)); outL.setBounds (top.removeFromRight (50));
    top.removeFromRight (6);
    scale.setBounds (top.removeFromRight (150)); scaleL.setBounds (top.removeFromRight (70));

    r.reduce (10, 0); r.removeFromBottom (10);
    auto side = r.removeFromRight (290);
    r.removeFromRight (10);
    loudness.setBounds (side.removeFromTop (440));
    side.removeFromTop (10);
    autoPanel.setBounds (side);
    display.setBounds (r);
}
