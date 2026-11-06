#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "dsp/Loudness.h"

// Lock-free mono sample FIFO from the audio thread to the analyzer display.
class SampleFifo
{
public:
    SampleFifo() : fifo (1 << 15) { buf.resize (1 << 15); }
    void push (const float* data, int n)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        if (n1 > 0) std::copy (data, data + n1, buf.begin() + s1);
        if (n2 > 0) std::copy (data + n1, data + n1 + n2, buf.begin() + s2);
        fifo.finishedWrite (n1 + n2);
    }
    int pull (float* dest, int maxN)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxN, s1, n1, s2, n2);
        if (n1 > 0) std::copy (buf.begin() + s1, buf.begin() + s1 + n1, dest);
        if (n2 > 0) std::copy (buf.begin() + s2, buf.begin() + s2 + n2, dest + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }
private:
    juce::AbstractFifo fifo;
    std::vector<float> buf;
};

class PotatoEQProcessor : public juce::AudioProcessor, private juce::Thread
{
public:
    PotatoEQProcessor();
    ~PotatoEQProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Potato EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- used by the editor (message thread) ----
    juce::AudioProcessorValueTreeState apvts;
    SampleFifo preFifo, postFifo;
    arc::LoudnessMeter meter;
    double getFs() const { return fs.load(); }

    arc::BandSettings bandSettings (int b) const { return bandParams[(size_t) b].read(); }
    float dynamicDelta (int b) const { return engine.dynamicDelta (b); }

    struct AutoSnapshot { arc::AutoResult result; float progress = 0; bool learning = false; };
    AutoSnapshot autoSnapshot() const;
    void startLearning();                 // (re)learn the incoming audio
    int commitAutoToBands();              // copies the auto curve into editable bands, returns bands used
    float autoGainDb() const { return autoGainNow.load(); }

    struct MatchResult { bool ok = false; float appliedDb = 0, wantedDb = 0; bool limitedByPeak = false; };
    MatchResult matchLoudnessToTarget();
    void resetMeter() { meterResetRequest = true; }

    int bandToFill() const;               // first unused band, -1 if none
    void setParam (const juce::String& id, float plainValue);

private:
    void run() override;                  // background thread: auto-EQ fitting + linear-phase FIR design
    void analyseForLearning (const juce::AudioBuffer<float>&, int numSamples);
    void buildLinearPhase();
    std::array<double, arc::kSpecBins> totalCurveAtBins (const std::array<arc::BandSettings, arc::kNumBands>&, const arc::AutoCurve&, double amount) const;
    std::array<arc::BandSettings, arc::kNumBands> readAllBands() const;

    std::array<params::BandParams, arc::kNumBands> bandParams;
    std::atomic<float>* pOut {}; std::atomic<float>* pScale {}; std::atomic<float>* pAutoGain {}; std::atomic<float>* pPhase {};
    std::atomic<float>* pProfile {}; std::atomic<float>* pAutoMode {}; std::atomic<float>* pAmount {}; std::atomic<float>* pTarget {};

    std::atomic<double> fs { 48000.0 };
    arc::EqEngine engine;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outSmooth;
    std::vector<float> monoScratch;

    // learning (audio thread fills, background thread reads)
    static constexpr int kLearnOrder = 13, kLearnSize = 1 << kLearnOrder, kLearnHop = kLearnSize / 2;
    juce::dsp::FFT learnFft { kLearnOrder };
    std::vector<float> learnIn[2], learnWork, learnWindow;
    int learnPos = 0;
    juce::SpinLock learnLock;
    std::array<double, arc::kSpecBins> learnAccum {};
    double learnFrames = 0;
    std::atomic<bool> learnResetRequest { false }, learnDone { false };
    std::atomic<float> learnProgress { 0 };
    double learnFramesNeeded = 100;
    std::vector<std::pair<int, int>> binMap;   // FFT bin range for each analysis band

    // auto result (background -> audio/editor)
    mutable juce::SpinLock resultLock;
    arc::AutoResult autoResult;
    bool autoResultFresh = false;
    arc::AutoCurve engineCurve;
    std::atomic<float> autoGainNow { 0 };
    int lastProfile = -1;

    // linear phase
    int firSize = 4096;
    juce::dsp::Convolution convLR, convMS;
    std::atomic<bool> linearActive { false }, firDirty { true };
    juce::int64 firHash = 0;
    std::atomic<bool> meterResetRequest { false };
    int lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoEQProcessor)
};
