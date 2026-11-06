#pragma once
#include "PluginProcessor.h"
#include "ui/EqDisplay.h"
#include "ui/Panels.h"

class PotatoEQEditor : public juce::AudioProcessorEditor
{
public:
    explicit PotatoEQEditor (PotatoEQProcessor&);
    ~PotatoEQEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PotatoEQProcessor& proc;
    PotatoLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    EqDisplay display;
    BandPanel bandPanel;
    LoudnessPanel loudness;
    AutoPanel autoPanel;

    juce::ComboBox phase, analyzer, range;
    juce::Slider scale, out;
    juce::Label scaleL, outL;
    juce::ToggleButton autoGain { "Auto gain" }, freeze { "Freeze" };
    juce::TextButton anaMenu { "Analyzer..." }, presetBtn { "Presets" };
    void showPresetMenu();
    void showAnalyzerMenu();
    std::unique_ptr<APVTS::ComboBoxAttachment> aPhase, aAnalyzer, aRange;
    std::unique_ptr<APVTS::SliderAttachment> aScale, aOut;
    std::unique_ptr<APVTS::ButtonAttachment> aAutoGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoEQEditor)
};
