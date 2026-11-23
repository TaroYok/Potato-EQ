#pragma once
#include "PluginProcessor.h"
#include "ui/EqDisplay.h"
#include "ui/Panels.h"

class PotatoEQEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PotatoEQEditor (PotatoEQProcessor&);
    ~PotatoEQEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void setScheme (int);   // whole-interface colour scheme
    void setUiFont (int);   // interface font
    void refreshLook();     // re-apply colours after a change
    void setBackdrop (int);  // 0 = sky and nature, 1 = colour void
    void openModule (int);   // ModuleDock::Module, or -1 to close the open sheet
    void showSettings (bool);
private:
    void timerCallback() override;
    void syncModules();
    void layoutOverlays();
    void showModuleMenu();
    bool loudnessAdded() const;
    bool autoEqAdded() const;
    int openSheet = -1, dockCount = -1, dockOpen = -2;
    void paintBackdrop (juce::Graphics&);   // glass backdrop + title bar, cached in backdrop
    juce::Image backdrop;
    bool backdropDirty = true;
    juce::String backdropKey;
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
    juce::ToggleButton autoGain { "Auto gain" }, freeze { "Freeze" };
    juce::Label phaseL, analyzerL, rangeL;
    GlassIconButton cogBtn { "Settings", GlassIconButton::cog }, addBtn { "Add module", GlassIconButton::plus };
    GlassSheet settings;
    ModuleDock dock;
    juce::TextButton anaMenu { "Display..." }, presetBtn { "Presets" };
    void showPresetMenu();
    void showAnalyzerMenu();
    void showColourDialog();
    void showSchemeDialog();
    std::unique_ptr<APVTS::ComboBoxAttachment> aPhase, aAnalyzer, aRange;
    std::unique_ptr<APVTS::SliderAttachment> aScale, aOut;
    std::unique_ptr<APVTS::ButtonAttachment> aAutoGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoEQEditor)
};
