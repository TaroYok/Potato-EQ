#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstring>

using namespace juce;

PotatoEQProcessor::PotatoEQProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      Thread ("PotatoEQ worker"),
      apvts (*this, nullptr, "PotatoEQ", params::createLayout())
{
    for (int b = 0; b < arc::kNumBands; ++b) bandParams[(size_t) b].attach (apvts, b);
    pOut = apvts.getRawParameterValue (params::id::outGain);
    pScale = apvts.getRawParameterValue (params::id::gainScale);
    pAutoGain = apvts.getRawParameterValue (params::id::autoGain);
    pPhase = apvts.getRawParameterValue (params::id::phaseMode);
    pProfile = apvts.getRawParameterValue (params::id::autoProfile);
    pAutoMode = apvts.getRawParameterValue (params::id::autoMode);
    pAmount = apvts.getRawParameterValue (params::id::autoAmount);
    pTarget = apvts.getRawParameterValue (params::id::lufsTarget);
    pAutoLevel = apvts.getRawParameterValue (params::id::autoLevel);
    pLimiter = apvts.getRawParameterValue (params::id::limiter);
    pCeiling = apvts.getRawParameterValue (params::id::ceiling);
    pCustom = apvts.getRawParameterValue (params::id::lufsCustom);
    pExperimental = apvts.getRawParameterValue (params::id::experimental);
    pMaxBoost = apvts.getRawParameterValue (params::id::autoMaxBoost);
    pMaxCut = apvts.getRawParameterValue (params::id::autoMaxCut);
    pTilt = apvts.getRawParameterValue (params::id::autoTilt);
    pBass = apvts.getRawParameterValue (params::id::autoBass);
    pSmooth = apvts.getRawParameterValue (params::id::autoSmooth);
    meter.prepare (48000.0);
    startThread (Thread::Priority::low);
}

PotatoEQProcessor::~PotatoEQProcessor() { stopThread (2000); }

bool PotatoEQProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (! (in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()))) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! (sc.isDisabled() || sc == AudioChannelSet::mono() || sc == AudioChannelSet::stereo())) return false;
    }
    return true;
}

void PotatoEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    engine.prepare (sampleRate);
    meter.prepare (sampleRate);
    preMeter.prepare (sampleRate);
    limiter.prepare (sampleRate);
    outSmooth.reset (sampleRate, 0.05);
    outSmooth.setCurrentAndTargetValue (Decibels::decibelsToGain (pOut->load()));
    monoScratch.assign ((size_t) samplesPerBlock + 16, 0.0f);

    // learning analysis
    for (auto& v : learnIn) v.assign (kLearnSize, 0.0f);
    learnWork.assign (2 * kLearnSize, 0.0f);
    learnWindow.resize (kLearnSize);
    for (int i = 0; i < kLearnSize; ++i) learnWindow[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / kLearnSize);
    learnPos = 0;
    learnFramesNeeded = 12.0 * sampleRate / kLearnHop;   // ~12 s of non-silent audio
    // band edges in (fractional) FFT bins; band energy is integrated exactly over the band,
    // so narrow low-frequency bands aren't over-measured by whole bins
    binEdges.clear();
    for (int i = 0; i < arc::kSpecBins; ++i)
    {
        const double binHz = sampleRate / kLearnSize;
        binEdges.emplace_back (arc::specBinLo (i) / binHz, arc::specBinHi (i) / binHz);
    }
    learnCum.assign ((size_t) kLearnSize / 2 + 2, 0.0);

    // linear phase
    int mult = 1;
    while (sampleRate / mult > 60000.0) mult *= 2;
    firSize = 8192 * mult;
    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, 2 };
    convLR.prepare (spec); convMS.prepare (spec);
    firDirty = true;
    lastMode = (int) pPhase->load();
    linearActive = lastMode == 1;
    updateLatency();
}

std::array<arc::BandSettings, arc::kNumBands> PotatoEQProcessor::readAllBands() const
{
    std::array<arc::BandSettings, arc::kNumBands> b;
    for (int i = 0; i < arc::kNumBands; ++i) b[(size_t) i] = bandParams[(size_t) i].read();
    return b;
}

void PotatoEQProcessor::updateLatency()
{
    setLatencySamples ((lastMode == 1 ? firSize : 0) + limiter.latency());
}

float PotatoEQProcessor::targetLufs() const
{
    const auto& t = params::lufsTargets()[(size_t) jlimit (0, (int) params::lufsTargets().size() - 1, (int) pTarget->load())];
    return t.lufs > 0 ? pCustom->load() : t.lufs;
}

arc::AutoTweaks PotatoEQProcessor::readTweaks() const
{
    arc::AutoTweaks t;
    t.maxBoost = pMaxBoost->load(); t.maxCut = pMaxCut->load();
    t.tiltDbPerOct = pTilt->load(); t.bassDb = pBass->load(); t.smoothing = (int) pSmooth->load();
    return t;
}

void PotatoEQProcessor::processBlock (AudioBuffer<float>& fullBuffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    auto buffer = getBusBuffer (fullBuffer, true, 0);
    const int n = buffer.getNumSamples();
    const int numCh = jmin (2, buffer.getNumChannels());
    if (n == 0 || numCh == 0) return;

    if ((int) monoScratch.size() < n) monoScratch.resize ((size_t) n);   // only if the host lied about block size

    // optional sidechain input
    const float* scPtr[2] = { nullptr, nullptr };
    int scCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (fullBuffer, true, 1);
        scCh = jmin (2, sc.getNumChannels());
        for (int c = 0; c < scCh; ++c) scPtr[c] = sc.getReadPointer (c);
    }
    scActive = scCh > 0;
    if (scCh > 0)
    {
        for (int i = 0; i < n; ++i) monoScratch[(size_t) i] = scCh > 1 ? 0.5f * (scPtr[0][i] + scPtr[1][i]) : scPtr[0][i];
        scFifo.push (monoScratch.data(), n);
    }

    auto pushMono = [&] (SampleFifo& f)
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (numCh > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i) monoScratch[(size_t) i] = 0.5f * (l[i] + r[i]);
        f.push (monoScratch.data(), n);
    };
    pushMono (preFifo);

    const int autoMode = effectiveAutoMode();
    if (autoMode != params::autoOff) analyseForLearning (buffer, n);

    {
        const SpinLock::ScopedTryLockType lock (resultLock);
        if (lock.isLocked() && autoResultFresh) { engineCurve = autoResult.curve; autoResultFresh = false; }
    }

    const int mode = (int) pPhase->load();
    if (mode != lastMode)
    {
        lastMode = mode;
        linearActive = mode == 1;
        updateLatency();
        if (mode == 1) { firDirty = true; convLR.reset(); convMS.reset(); }
        else engine.reset();
    }

    if (mode == 0)
    {
        engine.setBands (readAllBands());
        engine.setGainScale (pScale->load() / 100.0);
        engine.setAuto (engineCurve, pAmount->load() / 100.0, autoMode != params::autoOff);
        engine.process (buffer.getArrayOfWritePointers(), numCh, n, scCh > 0 ? scPtr : nullptr, scCh);
    }
    else
    {
        dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        convLR.process (dsp::ProcessContextReplacing<float> (block));
        if (numCh == 2)
        {
            float* l = buffer.getWritePointer (0); float* r = buffer.getWritePointer (1);
            for (int i = 0; i < n; ++i) { const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]); l[i] = m; r[i] = s; }
            convMS.process (dsp::ProcessContextReplacing<float> (block));
            for (int i = 0; i < n; ++i) { const float m = l[i], s = r[i]; l[i] = m + s; r[i] = m - s; }
        }
        else convMS.process (dsp::ProcessContextReplacing<float> (block));   // keeps latency identical in mono
    }

    if (meterResetRequest.load()) preMeter.reset();
    preMeter.process (buffer.getArrayOfReadPointers(), numCh, n);
    // auto level: ride the gain toward the loudness target (sample-time based, so offline renders behave too)
    {
        const double sr = fs.load();
        float lvl = autoLevelNow.load();
        if (pAutoLevel->load() > 0.5f)
        {
            const auto pr = preMeter.get();
            const float in = pr.integratedValid ? pr.integrated : pr.shortTerm;
            if (in > -60.0f)
            {
                const float target = targetLufs();
                const auto outR = meter.get();
                if (outR.shortTerm > -60.0f)   // small correction for what the limiter takes away
                    levelTrim = jlimit (-6.0f, 6.0f, levelTrim + (float) (n / (sr * 4.0)) * (target - outR.shortTerm));
                const float want = jlimit (-24.0f, 24.0f, target - in - pOut->load() + levelTrim);
                lvl += (want - lvl) * (float) (1.0 - std::exp (-n / (sr * 0.8)));
            }
        }
        else { lvl *= (float) std::exp (-n / (sr * 0.3)); levelTrim = 0; }
        autoLevelNow = lvl;
    }
    float targetDb = pOut->load() + (pAutoGain->load() > 0.5f ? autoGainNow.load() : 0.0f) + autoLevelNow.load();
    outSmooth.setTargetValue (Decibels::decibelsToGain (targetDb));
    if (outSmooth.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = outSmooth.getNextValue();
            for (int c = 0; c < numCh; ++c) buffer.getWritePointer (c)[i] *= g;
        }
    }
    else
    {
        for (int c = 0; c < numCh; ++c) buffer.applyGain (c, 0, n, outSmooth.getCurrentValue());
    }

    // limiter always runs (as a pure delay when off) so the latency never changes
    limiter.process (buffer.getArrayOfWritePointers(), numCh, n, pLimiter->load() > 0.5f ? (double) pCeiling->load() : 60.0);
    limiterGr = limiter.gainReductionDb();
    limiter.clearMeter();

    if (meterResetRequest.exchange (false)) meter.reset();
    meter.process (buffer.getArrayOfReadPointers(), numCh, n);
    pushMono (postFifo);
}

void PotatoEQProcessor::analyseForLearning (const AudioBuffer<float>& buffer, int numSamples)
{
    const int numCh = jmin (2, buffer.getNumChannels());
    int done = 0;
    while (done < numSamples)
    {
        const int k = jmin (numSamples - done, kLearnSize - learnPos);
        for (int c = 0; c < 2; ++c)
            std::copy_n (buffer.getReadPointer (jmin (c, numCh - 1)) + done, k, learnIn[c].begin() + learnPos);
        learnPos += k; done += k;
        if (learnPos < kLearnSize) continue;

        // one analysis frame
        std::array<double, arc::kSpecBins> frame {};
        double ms = 0;
        for (int c = 0; c < numCh; ++c)
        {
            for (int i = 0; i < kLearnSize; ++i) { const float x = learnIn[c][(size_t) i]; ms += (double) x * x; learnWork[(size_t) i] = x * learnWindow[(size_t) i]; }
            std::fill (learnWork.begin() + kLearnSize, learnWork.end(), 0.0f);
            learnFft.performFrequencyOnlyForwardTransform (learnWork.data(), true);
            // prefix sums of power (bin k covers [k-0.5, k+0.5]); skip DC
            const int nb = kLearnSize / 2 + 1;
            learnCum[0] = 0.0;
            for (int k = 0; k < nb; ++k)
                learnCum[(size_t) k + 1] = learnCum[(size_t) k] + (k < 1 ? 0.0 : (double) learnWork[(size_t) k] * learnWork[(size_t) k]);
            auto upTo = [&] (double x)
            {
                const double p = jlimit (0.0, (double) nb, x + 0.5);
                const int i = jmin (nb - 1, (int) p);
                return learnCum[(size_t) i] + (p - i) * (learnCum[(size_t) i + 1] - learnCum[(size_t) i]);
            };
            for (int b = 0; b < arc::kSpecBins; ++b)
                frame[(size_t) b] += upTo (binEdges[(size_t) b].second) - upTo (binEdges[(size_t) b].first);
        }
        ms /= (double) (kLearnSize * numCh);
        for (int c = 0; c < 2; ++c) std::copy (learnIn[c].begin() + kLearnHop, learnIn[c].end(), learnIn[c].begin());
        learnPos = kLearnSize - kLearnHop;

        const SpinLock::ScopedTryLockType lock (learnLock);
        if (! lock.isLocked()) continue;
        if (learnResetRequest.exchange (false)) { learnAccum.fill (0); learnFrames = 0; learnDone = false; }
        if (ms < 1e-6) continue;   // ignore silence (< -60 dBFS)

        if (effectiveAutoMode() == params::autoLive)
        {
            const double decay = std::exp (-kLearnHop / (fs.load() * 6.0));   // ~6 s memory
            for (int b = 0; b < arc::kSpecBins; ++b) learnAccum[(size_t) b] = learnAccum[(size_t) b] * decay + frame[(size_t) b];
            learnFrames = learnFrames * decay + 1.0;
            learnProgress = 1.0f;
        }
        else if (! learnDone)
        {
            for (int b = 0; b < arc::kSpecBins; ++b) learnAccum[(size_t) b] += frame[(size_t) b];
            learnFrames += 1.0;
            learnProgress = (float) jmin (1.0, learnFrames / learnFramesNeeded);
            if (learnFrames >= learnFramesNeeded) learnDone = true;
        }
    }
}

int PotatoEQProcessor::effectiveAutoMode() const
{
    return pExperimental->load() > 0.5f ? (int) pAutoMode->load() : (int) params::autoOff;
}

bool PotatoEQProcessor::usingReference() const
{
    return (int) pProfile->load() >= (int) arc::profiles().size();
}

String PotatoEQProcessor::referenceStatus() const
{
    const SpinLock::ScopedLockType lock (refLock);
    if (refLoading.load()) return "Analysing " + refName + " ...";
    if (refMessage.isNotEmpty()) return refMessage;
    return refValid.load() ? "Reference: " + refName : String ("No reference loaded");
}

void PotatoEQProcessor::loadReference (const File& f)
{
    if (refLoading.exchange (true)) return;
    { const SpinLock::ScopedLockType lock (refLock); refName = f.getFileName(); refMessage = {}; }
    Thread::launch ([this, f] { analyseReference (f); });
}

// Measures a song's long-term spectrum exactly the way the incoming audio is measured.
void PotatoEQProcessor::analyseReference (File f)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (f));
    std::array<double, arc::kSpecBins> acc {};
    double frames = 0;
    String msg;
    if (reader == nullptr || reader->sampleRate <= 0) msg = "Couldn't read " + f.getFileName() + " (use WAV, AIFF, FLAC, OGG or MP3)";
    else
    {
        const double sr = reader->sampleRate;
        const int N = kLearnSize, hop = kLearnHop, nb = N / 2 + 1;
        const int numCh = jlimit (1, 2, (int) reader->numChannels);
        dsp::FFT fft (kLearnOrder);
        std::vector<float> win ((size_t) N), work ((size_t) 2 * N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / N);
        std::vector<double> cum ((size_t) nb + 1, 0.0);
        AudioBuffer<float> buf (numCh, N);
        const int64 total = jmin (reader->lengthInSamples, (int64) (sr * 900));   // first 15 minutes at most
        for (int64 pos = 0; pos + N <= total; pos += hop)
        {
            reader->read (&buf, 0, N, pos, true, numCh > 1);
            double ms = 0;
            std::array<double, arc::kSpecBins> frame {};
            for (int c = 0; c < numCh; ++c)
            {
                const float* x = buf.getReadPointer (c);
                for (int i = 0; i < N; ++i) { ms += (double) x[i] * x[i]; work[(size_t) i] = x[i] * win[(size_t) i]; }
                std::fill (work.begin() + N, work.end(), 0.0f);
                fft.performFrequencyOnlyForwardTransform (work.data(), true);
                cum[0] = 0;
                for (int k = 0; k < nb; ++k) cum[(size_t) k + 1] = cum[(size_t) k] + (k < 1 ? 0.0 : (double) work[(size_t) k] * work[(size_t) k]);
                auto upTo = [&] (double xb)
                {
                    const double p = jlimit (0.0, (double) nb, xb + 0.5);
                    const int i = jmin (nb - 1, (int) p);
                    return cum[(size_t) i] + (p - i) * (cum[(size_t) i + 1] - cum[(size_t) i]);
                };
                const double binHz = sr / N;
                for (int b = 0; b < arc::kSpecBins; ++b)
                    frame[(size_t) b] += upTo (arc::specBinHi (b) / binHz) - upTo (arc::specBinLo (b) / binHz);
            }
            if (ms / (N * numCh) < 1e-6) continue;   // skip silence
            for (int b = 0; b < arc::kSpecBins; ++b) acc[(size_t) b] += frame[(size_t) b];
            frames += 1;
        }
        if (frames < 20) msg = f.getFileName() + " is too short or silent";
    }
    {
        const SpinLock::ScopedLockType lock (refLock);
        refMessage = msg;
        if (msg.isEmpty()) { refPower = acc; refValid = true; ++refVersion; }
    }
    refLoading = false;
}

void PotatoEQProcessor::startLearning()
{
    learnResetRequest = true;
    learnDone = false;
    learnProgress = 0.0f;
    const SpinLock::ScopedLockType lock (resultLock);
    autoResult = {};
    autoResult.curve.valid = false;
    autoResultFresh = true;
}

PotatoEQProcessor::AutoSnapshot PotatoEQProcessor::autoSnapshot() const
{
    AutoSnapshot s;
    { const SpinLock::ScopedLockType lock (resultLock); s.result = autoResult; }
    s.progress = learnProgress.load();
    s.learning = effectiveAutoMode() == params::autoLearn && ! learnDone.load();
    return s;
}

std::array<double, arc::kSpecBins> PotatoEQProcessor::totalCurveAtBins (const std::array<arc::BandSettings, arc::kNumBands>& bands,
                                                                        const arc::AutoCurve& curve, double amount) const
{
    const double sr = fs.load(), scale = pScale->load() / 100.0;
    std::array<double, arc::kSpecBins> c {};
    for (auto& b : bands)
    {
        if (! b.active()) continue;
        const double w = (b.place == arc::Placement::Stereo || b.place == arc::Placement::Mid) ? 1.0 : 0.5;
        const auto set = arc::designBand (b, b.gainDb * scale, sr);
        for (int i = 0; i < arc::kSpecBins; ++i) c[(size_t) i] += w * set.magnitudeDb (arc::specBinFreq (i), sr);
    }
    if (curve.valid && amount > 0)
        for (int i = 0; i < arc::kSpecBins; ++i) c[(size_t) i] += arc::autoCurveDb (curve, amount, arc::specBinFreq (i), sr);
    return c;
}

void PotatoEQProcessor::run()
{
    int lastAutoMode = -1;
    int64 lastLive = 0;
    while (! threadShouldExit())
    {
        wait (60);
        const int autoMode = effectiveAutoMode();
        const bool useRef = usingReference();
        const int profile = useRef ? (int) arc::profiles().size() : jlimit (0, (int) arc::profiles().size() - 1, (int) pProfile->load());
        const int refVer = refVersion.load();

        if (autoMode != lastAutoMode)
        {
            if (autoMode == params::autoLive) startLearning();
            if (autoMode == params::autoLearn && ! learnDone.load()) startLearning();
            lastAutoMode = autoMode;
        }

        // ---- auto-EQ fitting
        const auto tw = readTweaks();
        const double twHash = tw.maxBoost * 1.1 + tw.maxCut * 3.7 + tw.tiltDbPerOct * 13.3 + tw.bassDb * 29.1 + tw.smoothing * 101.0;
        bool compute = twHash != lastTweakHash && learnDone.load() && autoMode == params::autoLearn;
        lastTweakHash = twHash;
        if (autoMode == params::autoLearn && learnDone.load() && (profile != lastProfile || ! autoSnapshot().result.valid)) compute = true;
        if (autoMode == params::autoLive && Time::currentTimeMillis() - lastLive > 500) { compute = true; lastLive = Time::currentTimeMillis(); }
        if (autoMode != params::autoOff && autoMode == params::autoLearn && profile != lastProfile && learnDone.load()) compute = true;
        if (useRef && refVer != lastRefVersion && learnDone.load() && autoMode == params::autoLearn) compute = true;
        if (useRef && ! refValid.load())
        {   // no reference loaded yet: nothing to steer toward
            compute = false;
            const SpinLock::ScopedLockType lock (resultLock);
            if (autoResult.valid) { autoResult = {}; autoResultFresh = true; }
            lastProfile = profile;
        }

        if (compute)
        {
            std::array<double, arc::kSpecBins> acc {};
            double frames = 0;
            { const SpinLock::ScopedLockType lock (learnLock); acc = learnAccum; frames = learnFrames; }
            if (frames >= 3)
            {
                std::array<double, arc::kSpecBins> ref {};
                if (useRef) { const SpinLock::ScopedLockType lock (refLock); ref = refPower; }
                arc::Profile refProfile; refProfile.name = "Reference"; refProfile.fitLo = 30; refProfile.fitHi = 16000;
                auto r = useRef ? arc::computeAutoEq (acc, refProfile, fs.load(), tw, &ref)
                                : arc::computeAutoEq (acc, arc::profiles()[(size_t) profile], fs.load(), tw);
                lastRefVersion = refVer;
                if (r.valid)
                {
                    const SpinLock::ScopedLockType lock (resultLock);
                    autoResult = r; autoResultFresh = true; lastProfile = profile;
                }
            }
        }

        // ---- auto gain + linear phase rebuild
        const auto bands = readAllBands();
        arc::AutoCurve curve;
        { const SpinLock::ScopedLockType lock (resultLock); curve = autoResult.curve; }
        const double amount = autoMode != params::autoOff ? pAmount->load() / 100.0 : 0.0;
        autoGainNow = (float) arc::autoGainFor (totalCurveAtBins (bands, curve, amount));

        if (linearActive.load())
        {
            int64 h = 1469598103934665603LL;
            auto mix = [&h] (double v) { int64 x; float f = (float) v; std::memcpy (&x, &f, sizeof f); x &= 0xffffffff; h = (h ^ x) * 1099511628211LL; };
            for (auto& b : bands)
                if (b.active()) { mix (b.freq); mix (b.gainDb); mix (b.q); mix ((int) b.type); mix (b.slope); mix ((int) b.place); }
                else mix (-1);
            mix (pScale->load()); mix (amount);
            for (auto g : curve.gainsDb) mix (g);
            mix (curve.hpfHz); mix (curve.valid ? 1 : 0); mix (fs.load());
            if (firDirty.exchange (false) || h != firHash) { firHash = h; buildLinearPhase(); }
        }
    }
}

void PotatoEQProcessor::buildLinearPhase()
{
    const int N = firSize;
    const double sr = fs.load(), scale = pScale->load() / 100.0;
    const int autoMode = effectiveAutoMode();
    const double amount = autoMode != params::autoOff ? pAmount->load() / 100.0 : 0.0;
    arc::AutoCurve curve;
    { const SpinLock::ScopedLockType lock (resultLock); curve = autoResult.curve; }

    // magnitude (linear) for L, R, M, S
    std::vector<std::array<double, 4>> mag ((size_t) N / 2 + 1, { 1.0, 1.0, 1.0, 1.0 });
    for (auto& b : readAllBands())
    {
        if (! b.active()) continue;
        const auto set = arc::designBand (b, b.gainDb * scale, sr);
        for (int k = 0; k <= N / 2; ++k)
        {
            const double m = std::abs (set.response (jmax (1.0, k * sr / N), sr));
            auto& a = mag[(size_t) k];
            switch (b.place)
            {
                case arc::Placement::Stereo: a[0] *= m; a[1] *= m; break;
                case arc::Placement::Left:   a[0] *= m; break;
                case arc::Placement::Right:  a[1] *= m; break;
                case arc::Placement::Mid:    a[2] *= m; break;
                case arc::Placement::Side:   a[3] *= m; break;
                default: break;
            }
        }
    }
    if (curve.valid && amount > 0)
        for (int k = 0; k <= N / 2; ++k)
        {
            const double m = Decibels::decibelsToGain (arc::autoCurveDb (curve, amount, jmax (1.0, k * sr / N), sr), -300.0);
            mag[(size_t) k][0] *= m; mag[(size_t) k][1] *= m;
        }

    int order = 0; while ((1 << order) < N) ++order;
    dsp::FFT fft (order);
    std::vector<dsp::Complex<float>> in ((size_t) N), out ((size_t) N);
    AudioBuffer<float> irLR (2, N), irMS (2, N);
    for (int g = 0; g < 4; ++g)
    {
        double sum = 0;
        for (int k = 0; k <= N / 2; ++k)
        {
            const float v = (float) (mag[(size_t) k][(size_t) g] * ((k & 1) ? -1.0 : 1.0));   // delay of N/2 => linear phase
            in[(size_t) k] = { v, 0.0f };
            if (k > 0 && k < N / 2) in[(size_t) (N - k)] = { v, 0.0f };
            sum += mag[(size_t) k][(size_t) g] * (k == 0 || k == N / 2 ? 1.0 : 2.0);
        }
        fft.perform (in.data(), out.data(), true);
        // work out whether the inverse FFT was normalised: the centre tap equals mean |H|
        const double centre = out[(size_t) N / 2].real();
        const double norm = std::abs (centre - sum) < std::abs (centre - sum / N) ? 1.0 / N : 1.0;
        auto* dst = (g < 2 ? irLR : irMS).getWritePointer (g & 1);
        for (int i = 0; i < N; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * MathConstants<double>::pi * i / N);
            dst[i] = (float) (out[(size_t) i].real() * norm * w);
        }
    }
    convLR.loadImpulseResponse (std::move (irLR), sr, dsp::Convolution::Stereo::yes, dsp::Convolution::Trim::no, dsp::Convolution::Normalise::no);
    convMS.loadImpulseResponse (std::move (irMS), sr, dsp::Convolution::Stereo::yes, dsp::Convolution::Trim::no, dsp::Convolution::Normalise::no);
}

int PotatoEQProcessor::getNumPrograms() { return (int) presets::all().size(); }

const String PotatoEQProcessor::getProgramName (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return {};
    const auto& p = presets::all()[(size_t) i];
    return String (p.category) + ": " + p.name;
}

void PotatoEQProcessor::setCurrentProgram (int i)
{
    // ignore re-selecting the current program: some hosts do that after restoring a project
    if (i != currentPreset && isPositiveAndBelow (i, getNumPrograms())) applyPreset (i);
}

void PotatoEQProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms())) return;
    const auto& p = presets::all()[(size_t) index];
    for (int b = 0; b < arc::kNumBands; ++b)
    {
        if (b < (int) p.bands.size())
        {
            const auto& d = p.bands[(size_t) b];
            setParam (params::bandId (b, "type"), (float) d.type);
            setParam (params::bandId (b, "freq"), (float) d.freq);
            setParam (params::bandId (b, "gain"), (float) d.gain);
            setParam (params::bandId (b, "q"), (float) d.q);
            setParam (params::bandId (b, "slope"), (float) params::slopeIndex (d.slope));
            setParam (params::bandId (b, "place"), (float) d.place);
            setParam (params::bandId (b, "dyn"), d.dynamic ? 1.0f : 0.0f);
            setParam (params::bandId (b, "thr"), (float) d.thr);
            setParam (params::bandId (b, "range"), (float) d.range);
            setParam (params::bandId (b, "bypass"), 0.0f);
            setParam (params::bandId (b, "used"), 1.0f);
        }
        else if (bandParams[(size_t) b].used->load() > 0.5f || bandParams[(size_t) b].dyn->load() > 0.5f)
        {
            setParam (params::bandId (b, "used"), 0.0f);
            setParam (params::bandId (b, "gain"), 0.0f);
            setParam (params::bandId (b, "dyn"), 0.0f);
            setParam (params::bandId (b, "bypass"), 0.0f);
        }
    }
    setParam (params::id::gainScale, 100.0f);
    currentPreset = index;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

int PotatoEQProcessor::bandToFill() const
{
    for (int b = 0; b < arc::kNumBands; ++b) if (bandParams[(size_t) b].used->load() < 0.5f) return b;
    return -1;
}

void PotatoEQProcessor::setParam (const String& pid, float plainValue)
{
    if (auto* p = apvts.getParameter (pid))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

int PotatoEQProcessor::commitAutoToBands()
{
    const auto snap = autoSnapshot();
    if (! snap.result.valid) return 0;
    const double amount = pAmount->load() / 100.0, sr = fs.load();
    auto noHpf = snap.result.curve; noHpf.hpfHz = 0;
    std::array<double, arc::kSpecBins> c {};
    for (int i = 0; i < arc::kSpecBins; ++i) c[(size_t) i] = arc::autoCurveDb (noHpf, amount, arc::specBinFreq (i), sr);

    int free = 0;
    for (int b = 0; b < arc::kNumBands; ++b) if (bandParams[(size_t) b].used->load() < 0.5f) ++free;
    const auto fitted = arc::fitParametric (c, jmin (8, free), amount > 0 ? snap.result.curve.hpfHz : 0.0, sr);

    int placed = 0;
    for (auto& f : fitted)
    {
        const int b = bandToFill();
        if (b < 0) break;
        setParam (params::bandId (b, "type"), (float) f.type);
        setParam (params::bandId (b, "freq"), (float) f.freq);
        setParam (params::bandId (b, "gain"), (float) f.gainDb);
        setParam (params::bandId (b, "q"), (float) f.q);
        setParam (params::bandId (b, "slope"), (float) params::slopeIndex (f.slope));
        setParam (params::bandId (b, "place"), 0.0f);
        setParam (params::bandId (b, "dyn"), 0.0f);
        setParam (params::bandId (b, "bypass"), 0.0f);
        setParam (params::bandId (b, "used"), 1.0f);
        ++placed;
    }
    setParam (params::id::autoMode, (float) params::autoOff);
    return placed;
}

PotatoEQProcessor::MatchResult PotatoEQProcessor::matchLoudnessToTarget()
{
    MatchResult m;
    const auto r = meter.get();
    const float current = r.integratedValid ? r.integrated : r.shortTerm;
    if (current < -69.0f) return m;
    m.wantedDb = targetLufs() - current;
    m.appliedDb = m.wantedDb;
    const float room = -1.0f - r.truePeakDb;   // without the limiter, keep true peak at or below -1 dBTP
    if (pLimiter->load() < 0.5f && m.appliedDb > room) { m.appliedDb = jmax (0.0f, room); m.limitedByPeak = true; }
    setParam (params::id::outGain, jlimit (-24.0f, 24.0f, pOut->load() + m.appliedDb));
    resetMeter();
    m.ok = true;
    return m;
}

void PotatoEQProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    ValueTree learn ("Learned");
    {
        const SpinLock::ScopedLockType lock (learnLock);
        String s;
        for (auto v : learnAccum) s << String (v, 12) << " ";
        learn.setProperty ("accum", s.trim(), nullptr);
        learn.setProperty ("frames", learnFrames, nullptr);
        learn.setProperty ("done", learnDone.load(), nullptr);
    }
    state.removeChild (state.getChildWithName ("Learned"), nullptr);
    state.appendChild (learn, nullptr);
    ValueTree ref ("Reference");
    {
        const SpinLock::ScopedLockType lock (refLock);
        if (refValid.load())
        {
            String s;
            for (auto v : refPower) s << String (v, 12) << " ";
            ref.setProperty ("power", s.trim(), nullptr);
            ref.setProperty ("name", refName, nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("Reference"), nullptr);
    state.appendChild (ref, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PotatoEQProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;
    auto learn = state.getChildWithName ("Learned");
    if (learn.isValid())
    {
        // project files can come from anywhere: only accept well-formed, finite, non-negative data
        auto parts = StringArray::fromTokens (learn["accum"].toString().substring (0, 4096), " ", "");
        std::array<double, arc::kSpecBins> acc {};
        bool ok = parts.size() == arc::kSpecBins;
        for (int i = 0; ok && i < arc::kSpecBins; ++i)
        {
            acc[(size_t) i] = parts[i].getDoubleValue();
            ok = std::isfinite (acc[(size_t) i]) && acc[(size_t) i] >= 0.0 && acc[(size_t) i] < 1e30;
        }
        const double frames = (double) learn["frames"];
        ok = ok && std::isfinite (frames) && frames >= 0.0 && frames < 1e9;
        const SpinLock::ScopedLockType lock (learnLock);
        learnAccum = ok ? acc : std::array<double, arc::kSpecBins> {};
        learnFrames = ok ? frames : 0.0;
        learnDone = ok && (bool) learn["done"];
        learnProgress = learnDone ? 1.0f : 0.0f;
        lastProfile = -1;   // forces the curve to be rebuilt from the saved spectrum
    }
    auto ref = state.getChildWithName ("Reference");
    if (ref.isValid() && ref.hasProperty ("power"))
    {
        auto parts = StringArray::fromTokens (ref["power"].toString().substring (0, 4096), " ", "");
        std::array<double, arc::kSpecBins> pw {};
        bool ok = parts.size() == arc::kSpecBins;
        for (int i = 0; ok && i < arc::kSpecBins; ++i)
        {
            pw[(size_t) i] = parts[i].getDoubleValue();
            ok = std::isfinite (pw[(size_t) i]) && pw[(size_t) i] >= 0.0 && pw[(size_t) i] < 1e30;
        }
        if (ok)
        {
            const SpinLock::ScopedLockType lock (refLock);
            refPower = pw; refName = ref["name"].toString().substring (0, 200); refValid = true; ++refVersion;
        }
    }
    apvts.replaceState (state);
}

AudioProcessorEditor* PotatoEQProcessor::createEditor() { return new PotatoEQEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PotatoEQProcessor(); }
