#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"

// The main curve display: spectrum analyzer, band curves, draggable band nodes.
class EqDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EqDisplay (PotatoEQProcessor&);
    ~EqDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseExit (const juce::MouseEvent&) override;

    int getSelected() const { return selected; }
    void setSelected (int b);
    std::function<void (int)> onSelectionChanged;

private:
    void timerCallback() override;
    void pullSpectrum (SampleFifo&, std::vector<float>& ring, int& pos, std::vector<float>& display);

    juce::Rectangle<float> plotArea() const;
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForGain (double db) const;
    double gainForY (float y) const;
    float yForSpectrum (double db) const;
    double displayRange() const;

    juce::Point<float> nodePosition (int band, const arc::BandSettings&) const;
    int hitNode (juce::Point<float>) const;
    void addBandAt (juce::Point<float>);
    void deleteBand (int b);
    void showBandMenu (int b);
    void setParamPlain (int band, const char* what, float v);

    PotatoEQProcessor& proc;
    int selected = -1, hovered = -1, dragging = -1;
    juce::Point<float> dragStart;
    double dragStartFreq = 0, dragStartGain = 0;

    static constexpr int kFftOrder = 13, kFftSize = 1 << kFftOrder;
    juce::dsp::FFT fft { kFftOrder };
    std::vector<float> preRing, postRing, fftData, window;
    int prePos = 0, postPos = 0;
    std::vector<float> preDisplay, postDisplay;   // dB per plot column
    juce::Image cache;
    std::array<arc::BandSettings, arc::kNumBands> bands;
};
