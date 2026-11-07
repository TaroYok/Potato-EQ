#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"

using APVTS = juce::AudioProcessorValueTreeState;

class PotatoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PotatoLookAndFeel();
    void refreshColours();   // after a colour-scheme change
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (12.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::font (12.5f, true); }
    juce::Font getLabelFont (juce::Label&) override { return theme::font (12.0f); }
};

// Rotary knob with a caption above and a value box below.
class Knob : public juce::Component
{
public:
    explicit Knob (const juce::String& caption);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
};

// Fills a ComboBox with the choices of a choice parameter (ids start at 1).
void fillCombo (juce::ComboBox&, APVTS&, const juce::String& paramId);

class BandPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kHeight = 104;
    BandPanel (PotatoEQProcessor&);
    ~BandPanel() override;
    void setBand (int b);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void (int)> onDelete;

private:
    void timerCallback() override;
    PotatoEQProcessor& proc;
    int band = -1;
    juce::ComboBox type, slope, place;
    juce::Label typeL, slopeL, placeL;
    Knob freq { "FREQUENCY" }, gain { "GAIN" }, q { "Q" }, thr { "THRESHOLD" }, range { "RANGE" };
    juce::ToggleButton dyn { "Dynamic" }, bypass { "Bypass" }, sc { "Sidechain" };
    juce::TextButton del { "Delete" };
    std::unique_ptr<APVTS::ComboBoxAttachment> aType, aSlope, aPlace;
    std::unique_ptr<APVTS::SliderAttachment> aFreq, aGain, aQ, aThr, aRange;
    std::unique_ptr<APVTS::ButtonAttachment> aDyn, aBypass, aSc;
};

class LoudnessPanel : public juce::Component, private juce::Timer
{
public:
    LoudnessPanel (PotatoEQProcessor&);
    ~LoudnessPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    PotatoEQProcessor& proc;
    juce::ComboBox target;
    std::unique_ptr<APVTS::ComboBoxAttachment> aTarget;
    juce::TextButton match { "Match target" }, reset { "Reset" };
    juce::ToggleButton autoLevel { "Auto level to target" }, limiterOn { "Limiter" };
    juce::Slider ceiling, custom;
    juce::Label ceilingL, customL;
    std::unique_ptr<APVTS::ButtonAttachment> aAutoLevel, aLimiter;
    std::unique_ptr<APVTS::SliderAttachment> aCeiling, aCustom;
    float grShown = 0;
    juce::String status;
    arc::LoudnessMeter::Readings r;
    std::vector<float> history;   // momentary loudness, 10 per second
    int tick = 0;
};

class AutoPanel : public juce::Component, private juce::Timer
{
public:
    AutoPanel (PotatoEQProcessor&);
    ~AutoPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    PotatoEQProcessor& proc;
    juce::ComboBox profile, mode;
    juce::Slider amount;
    juce::TextButton relearn { "Re-learn" }, commit { "Turn into bands" }, loadRef { "Load track..." };
    std::unique_ptr<juce::FileChooser> chooser;
    bool refShown = false;
    std::unique_ptr<APVTS::ComboBoxAttachment> aProfile, aMode, aSmooth;
    std::unique_ptr<APVTS::SliderAttachment> aAmount, aTilt, aBass, aBoost, aCut;
    juce::Slider tilt, bass, boost, cut;
    juce::Label tiltL, bassL, boostL, cutL;
    juce::ComboBox smooth;
    PotatoEQProcessor::AutoSnapshot snap;
    juce::String status;
};
