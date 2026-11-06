#include "PluginEditor.h"

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

    for (auto* c : { (Component*) &phase, (Component*) &analyzer, (Component*) &range, (Component*) &autoGain,
                     (Component*) &freeze, (Component*) &anaMenu,
                     (Component*) &display, (Component*) &bandPanel, (Component*) &loudness, (Component*) &autoPanel })
        addAndMakeVisible (*c);

    display.onSelectionChanged = [this] (int b) { bandPanel.setBand (b); };
    bandPanel.onDelete = [this] (int b)
    {
        proc.setParam (params::bandId (b, "used"), 0.0f);
        proc.setParam (params::bandId (b, "gain"), 0.0f);
        display.setSelected (-1);
    };

    setResizable (true, true);
    setResizeLimits (1100, 640, 2000, 1300);
    setSize (1240, 760);
}

PotatoEQEditor::~PotatoEQEditor() { setLookAndFeel (nullptr); }

void PotatoEQEditor::showAnalyzerMenu()
{
    auto& st = proc.apvts.state;
    PopupMenu m, res, spd, tlt, rng;
    auto fill = [&] (PopupMenu& sub, const StringArray& names, const Identifier& id, int def, int base)
    {
        const int cur = ana::get (st, id, def);
        for (int i = 0; i < names.size(); ++i) sub.addItem (base + i, names[i], true, cur == i);
    };
    fill (res, ana::resolutionNames, ana::resolution, 2, 100);
    fill (spd, ana::speedNames, ana::speed, 1, 200);
    fill (tlt, ana::tiltNames, ana::tilt, 2, 300);
    fill (rng, ana::rangeNames, ana::range, 1, 400);
    m.addSubMenu ("Resolution", res);
    m.addSubMenu ("Speed", spd);
    m.addSubMenu ("Tilt", tlt);
    m.addSubMenu ("Range", rng);
    m.addSeparator();
    m.addItem (1, "Peak hold", true, ana::get (st, ana::peakHold, 0) != 0);
    m.addItem (2, "Clear peak hold");
    SafePointer<PotatoEQEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&anaMenu), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& s = safe->proc.apvts.state;
        if (r == 1) { s.setProperty (ana::peakHold, ana::get (s, ana::peakHold, 0) ? 0 : 1, nullptr); safe->display.clearPeakHold(); }
        else if (r == 2) safe->display.clearPeakHold();
        else if (r >= 400) s.setProperty (ana::range, r - 400, nullptr);
        else if (r >= 300) s.setProperty (ana::tilt, r - 300, nullptr);
        else if (r >= 200) s.setProperty (ana::speed, r - 200, nullptr);
        else if (r >= 100) s.setProperty (ana::resolution, r - 100, nullptr);
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
    top.removeFromLeft (140);
    phase.setBounds (top.removeFromLeft (130)); top.removeFromLeft (8);
    analyzer.setBounds (top.removeFromLeft (110)); top.removeFromLeft (6);
    anaMenu.setBounds (top.removeFromLeft (86)); top.removeFromLeft (6);
    freeze.setBounds (top.removeFromLeft (72)); top.removeFromLeft (8);
    range.setBounds (top.removeFromLeft (76)); top.removeFromLeft (10);
    autoGain.setBounds (top.removeFromLeft (96));
    out.setBounds (top.removeFromRight (150)); outL.setBounds (top.removeFromRight (50));
    top.removeFromRight (6);
    scale.setBounds (top.removeFromRight (150)); scaleL.setBounds (top.removeFromRight (70));

    r.reduce (10, 0); r.removeFromBottom (10);
    auto side = r.removeFromRight (290);
    r.removeFromRight (10);
    loudness.setBounds (side.removeFromTop (370));
    side.removeFromTop (10);
    autoPanel.setBounds (side);
    bandPanel.setBounds (r.removeFromBottom (122));
    r.removeFromBottom (10);
    display.setBounds (r);
}
