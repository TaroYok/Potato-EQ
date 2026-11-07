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
    void setScheme (int);   // whole-interface colour scheme
private:
    PotatoEQProcessor& proc;
    PotatoLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    EqDisplay display;
    BandPanel bandPanel;
    LoudnessPanel loudness;
    LevelMeters meters { proc };
    AutoPanel autoPanel;

    juce::ComboBox phase, analyzer, range;
    juce::Slider scale, out;
    juce::Label scaleL, outL;
    juce::ToggleButton autoGain { "Auto gain" }, freeze { "Freeze" }, experimental { "Experimental tools" };
    void updateExperimental();
    juce::TextButton anaMenu { "Display..." }, presetBtn { "Presets" };
    void showPresetMenu();
    void showAnalyzerMenu();
    void showColourDialog();
    std::unique_ptr<APVTS::ComboBoxAttachment> aPhase, aAnalyzer, aRange;
    std::unique_ptr<APVTS::SliderAttachment> aScale, aOut;
    std::unique_ptr<APVTS::ButtonAttachment> aAutoGain, aExperimental;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoEQEditor)
};
