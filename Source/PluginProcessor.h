#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "dsp/Loudness.h"
#include "dsp/Limiter.h"
#include "dsp/Spectral.h"
#include "SpectrumShare.h"

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
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void updateTrackProperties (const TrackProperties&) override;
    int shareId() const { return shareIdent; }
    juce::String instanceName() const;             // shown to other Potato EQs (your name > DAW track name > "Potato EQ n")
    void setInstanceName (const juce::String&);  // saved with the project
    void setStateInformation (const void*, int) override;

    // ---- used by the editor (message thread) ----
    juce::AudioProcessorValueTreeState apvts;
    SampleFifo preFifo, postFifo, scFifo;
    arc::LoudnessMeter meter, preMeter;   // output, and after-EQ/before-gain (used by auto level)
    arc::LoudnessMeter inMeter;           // plugin input, for the In meter
    float autoLevelDb() const { return autoLevelNow.load(); }
    // Auto level "learn, then lock": progress 0..1 while learning; locked = gain frozen (saved with the project)
    struct LevelState { bool lockMode = false, locked = false; float progress = 0, lockedDb = 0; };
    LevelState levelState() const;
    void relearnLevel() { relearnRequest = true; }
    float limiterReductionDb() const { return limiterGr.load(); }
    bool sidechainActive() const { return scActive.load(); }
    float targetLufs() const;
    double getFs() const { return fs.load(); }

    arc::BandSettings bandSettings (int b) const { return bandParams[(size_t) b].read(); }
    float dynamicDelta (int b) const { return engine.dynamicDelta (b); }
    // spectral dynamics (display)
    const arc::SpectralDynamics& spectralDynamics() const { return spectral; }
    bool spectralActive() const { return specActive.load(); }

    struct AutoSnapshot { arc::AutoResult result; float progress = 0; bool learning = false; };
    AutoSnapshot autoSnapshot() const;
    void startLearning();                 // (re)learn the incoming audio
    int effectiveAutoMode() const;        // auto mode, or Off while Experimental tools are switched off
    bool usingReference() const;          // profile is "Match a reference track"

    // reference track (analysed on a background thread; saved with the project)
    void loadReference (const juce::File&);
    juce::String referenceStatus() const;
    bool referenceReady() const { return refValid.load(); }
    int commitAutoToBands();              // copies the auto curve into editable bands, returns bands used
    float autoGainDb() const { return autoGainNow.load(); }

    struct MatchResult { bool ok = false; float appliedDb = 0, wantedDb = 0; bool limitedByPeak = false; };
    MatchResult matchLoudnessToTarget();
    void resetMeter() { meterResetRequest = true; }

    void applyPreset (int index);         // loads a factory preset (message thread)
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
    static constexpr int kLearnOrder = 14, kLearnSize = 1 << kLearnOrder, kLearnHop = kLearnSize / 2;   // 2.9 Hz bins at 48 kHz
    juce::dsp::FFT learnFft { kLearnOrder };
    std::vector<float> learnIn[2], learnWork, learnWindow;
    int learnPos = 0;
    juce::SpinLock learnLock;
    std::array<double, arc::kSpecBins> learnAccum {};
    double learnFrames = 0;
    std::atomic<bool> learnResetRequest { false }, learnDone { false };
    std::atomic<float> learnProgress { 0 };
    double learnFramesNeeded = 100;
    std::vector<std::pair<double, double>> binEdges;   // fractional FFT-bin edges of each analysis band
    std::vector<double> learnCum;                       // prefix sums of bin power

    // auto result (background -> audio/editor)
    mutable juce::SpinLock resultLock;
    arc::AutoResult autoResult;
    bool autoResultFresh = false;
    arc::AutoCurve engineCurve;
    std::atomic<float> autoGainNow { 0 }, autoLevelNow { 0 }, limiterGr { 0 };
    std::atomic<bool> scActive { false };
    arc::Limiter limiter;
    arc::SpectralDynamics spectral;
    std::atomic<bool> specActive { false };
    int specResShown = -1;
    std::atomic<float>* pSpecRes {};
    std::atomic<float>* pAutoLevel {}; std::atomic<float>* pLimiter {}; std::atomic<float>* pCeiling {}; std::atomic<float>* pCustom {};
    std::atomic<float>* pExperimental {};
    std::atomic<float>* pMaxBoost {}; std::atomic<float>* pMaxCut {}; std::atomic<float>* pTilt {}; std::atomic<float>* pBass {}; std::atomic<float>* pSmooth {};
    arc::AutoTweaks readTweaks() const;
    void updateLatency();
    int lastProfile = -1;

    // linear phase
    int firSize = 4096;
    juce::dsp::Convolution convLR, convMS;
    std::atomic<bool> linearActive { false }, firDirty { true };
    juce::int64 firHash = 0;
    std::atomic<bool> meterResetRequest { false };
    int lastMode = -1;
    // cross-track spectrum sharing (collision detection)
    int shareIdent = 0;
    SampleFifo shareFifo;
    std::vector<float> shareRing, shareWork, shareWindow;
    int shareRingPos = 0;
    arc::collide::Spectrum shareDb {};
    juce::dsp::FFT shareFft { 12 };
    void updateShare();
    int currentPreset = 0;
    mutable juce::SpinLock refLock;
    std::array<double, arc::kSpecBins> refPower {};
    juce::String refName, refMessage;
    std::atomic<bool> refValid { false }, refLoading { false };
    std::atomic<int> refVersion { 0 };
    int lastRefVersion = -1;
    void analyseReference (juce::File);
    float levelTrim = 0;
    std::atomic<bool> levelLocked { false }, relearnRequest { false };
    std::atomic<float> lockedDb { 0 }, lockTarget { -14 }, levelProgress { 0 };
    std::atomic<float>* pLevelMode {};
    double levelSettledFor = 0;
    static constexpr float kLevelLearnSeconds = 15.0f;
    double lastTweakHash = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoEQProcessor)
};
