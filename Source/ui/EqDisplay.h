#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"

// The main curve display: spectrum analyzer, band curves, draggable band nodes,
// spectrum grab (drag a peak in the analyzer to create a band) and note readouts.
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
    void clearPeakHold();
    bool isDragging() const { return dragging >= 0; }
    juce::Point<float> bandNodePosition (int b) const { return nodePosition (bands[(size_t) b]); }
    void showMessage (const juce::String& m) { message = m; messageUntil = juce::Time::getMillisecondCounter() + 5000; repaint(); }
    std::function<void (int)> onSelectionChanged;

private:
    struct Spectrum
    {
        std::vector<float> ring, display, peak, bins;   // bins: last raw FFT magnitudes
        int pos = 0;
        double binHz = 1.0;
    };
    struct Peak { bool valid = false; double freq = 0, db = 0; juce::Point<float> pt; };

    void timerCallback() override;
    void pullSpectrum (SampleFifo&, Spectrum&, bool analyse);
    void findPeakNear (juce::Point<float>);
    const Spectrum* grabSource() const;

    juce::Rectangle<float> plotArea() const;
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForGain (double db) const;
    double gainForY (float y) const;
    float yForSpectrum (double db) const;
    double displayRange() const;
    int setting (const juce::Identifier& id, int def) const { return ana::get (proc.apvts.state, id, def); }

    juce::Point<float> nodePosition (const arc::BandSettings&) const;
    int hitNode (juce::Point<float>) const;
    int addBand (arc::BandType, double freq, double gainDb, double q);
    void deleteBand (int b);
    void showBandMenu (int b);
    void setParamPlain (int band, const char* what, float v);

    PotatoEQProcessor& proc;
    int selected = -1, hovered = -1, dragging = -1;
    bool grabbing = false;               // dragging a band created from a spectrum peak
    juce::Point<float> dragStart, mousePos;
    bool mouseInside = false;
    double dragStartFreq = 0, dragStartGain = 0;
    Peak hoverPeak;
    juce::String message;
    juce::uint32 messageUntil = 0;

    static constexpr int kMaxOrder = 14, kMaxSize = 1 << kMaxOrder;
    std::array<std::unique_ptr<juce::dsp::FFT>, kMaxOrder + 1> ffts;
    int fftOrder = 13;
    std::vector<float> fftData, window;
    std::vector<double> cum;   // prefix sums of bin power (spectrum smoothing)
    Spectrum pre, post, side;
    std::array<arc::BandSettings, arc::kNumBands> bands;

    // per-band response curves are cached and only recomputed when a band changes
    struct CurveCache { std::vector<float> db; arc::BandSettings key; double eff = 1e9, fs = 0; float width = 0; };
    std::array<CurveCache, arc::kNumBands> curveCache;
    CurveCache autoCache;
    std::array<double, arc::kAutoBells> autoKey {};
    const std::vector<float>& bandCurve (int band, const arc::BandSettings&, double effGain, int cols);
};
