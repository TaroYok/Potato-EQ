#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <cstring>

using namespace juce;

PotatoEQProcessor::PotatoEQProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
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
    meter.prepare (48000.0);
    startThread (Thread::Priority::low);
}

PotatoEQProcessor::~PotatoEQProcessor() { stopThread (2000); }

bool PotatoEQProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void PotatoEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    engine.prepare (sampleRate);
    meter.prepare (sampleRate);
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
    binMap.clear();
    for (int i = 0; i < arc::kSpecBins; ++i)
    {
        const double binHz = sampleRate / kLearnSize;
        int lo = (int) std::ceil (arc::specBinLo (i) / binHz), hi = (int) std::floor (arc::specBinHi (i) / binHz);
        if (hi < lo) lo = hi = (int) std::lround (arc::specBinFreq (i) / binHz);
        binMap.emplace_back (jlimit (1, kLearnSize / 2, lo), jlimit (1, kLearnSize / 2, hi));
    }

    // linear phase
    int mult = 1;
    while (sampleRate / mult > 60000.0) mult *= 2;
    firSize = 8192 * mult;
    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, 2 };
    convLR.prepare (spec); convMS.prepare (spec);
    firDirty = true;
    lastMode = -1;
}

std::array<arc::BandSettings, arc::kNumBands> PotatoEQProcessor::readAllBands() const
{
    std::array<arc::BandSettings, arc::kNumBands> b;
    for (int i = 0; i < arc::kNumBands; ++i) b[(size_t) i] = bandParams[(size_t) i].read();
    return b;
}

void PotatoEQProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = jmin (2, getTotalNumInputChannels());
    for (int c = numCh; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || numCh == 0) return;
    if ((int) monoScratch.size() < n) monoScratch.resize ((size_t) n);   // only if the host lied about block size

    auto pushMono = [&] (SampleFifo& f)
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (numCh > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i) monoScratch[(size_t) i] = 0.5f * (l[i] + r[i]);
        f.push (monoScratch.data(), n);
    };
    pushMono (preFifo);

    const int autoMode = (int) pAutoMode->load();
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
        setLatencySamples (mode == 1 ? firSize : 0);
        if (mode == 1) { firDirty = true; convLR.reset(); convMS.reset(); }
        else engine.reset();
    }

    if (mode == 0)
    {
        engine.setBands (readAllBands());
        engine.setGainScale (pScale->load() / 100.0);
        engine.setAuto (engineCurve, pAmount->load() / 100.0, autoMode != params::autoOff);
        engine.process (buffer.getArrayOfWritePointers(), numCh, n);
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

    float targetDb = pOut->load() + (pAutoGain->load() > 0.5f ? autoGainNow.load() : 0.0f);
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
            for (int b = 0; b < arc::kSpecBins; ++b)
            {
                double p = 0;
                for (int j = binMap[(size_t) b].first; j <= binMap[(size_t) b].second; ++j) p += (double) learnWork[(size_t) j] * learnWork[(size_t) j];
                frame[(size_t) b] += p;
            }
        }
        ms /= (double) (kLearnSize * numCh);
        for (int c = 0; c < 2; ++c) std::copy (learnIn[c].begin() + kLearnHop, learnIn[c].end(), learnIn[c].begin());
        learnPos = kLearnSize - kLearnHop;

        const SpinLock::ScopedTryLockType lock (learnLock);
        if (! lock.isLocked()) continue;
        if (learnResetRequest.exchange (false)) { learnAccum.fill (0); learnFrames = 0; learnDone = false; }
        if (ms < 1e-6) continue;   // ignore silence (< -60 dBFS)

        if ((int) pAutoMode->load() == params::autoLive)
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
    s.learning = (int) pAutoMode->load() == params::autoLearn && ! learnDone.load();
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
        const int autoMode = (int) pAutoMode->load();
        const int profile = jlimit (0, (int) arc::profiles().size() - 1, (int) pProfile->load());

        if (autoMode != lastAutoMode)
        {
            if (autoMode == params::autoLive) startLearning();
            if (autoMode == params::autoLearn && ! learnDone.load()) startLearning();
            lastAutoMode = autoMode;
        }

        // ---- auto-EQ fitting
        bool compute = false;
        if (autoMode == params::autoLearn && learnDone.load() && (profile != lastProfile || ! autoSnapshot().result.valid)) compute = true;
        if (autoMode == params::autoLive && Time::currentTimeMillis() - lastLive > 500) { compute = true; lastLive = Time::currentTimeMillis(); }
        if (autoMode != params::autoOff && autoMode == params::autoLearn && profile != lastProfile && learnDone.load()) compute = true;

        if (compute)
        {
            std::array<double, arc::kSpecBins> acc {};
            double frames = 0;
            { const SpinLock::ScopedLockType lock (learnLock); acc = learnAccum; frames = learnFrames; }
            if (frames >= 3)
            {
                auto r = arc::computeAutoEq (acc, arc::profiles()[(size_t) profile], fs.load());
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
    const int autoMode = (int) pAutoMode->load();
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
    const float target = params::lufsTargets()[(size_t) jlimit (0, (int) params::lufsTargets().size() - 1, (int) pTarget->load())].lufs;
    m.wantedDb = target - current;
    m.appliedDb = m.wantedDb;
    const float room = -1.0f - r.truePeakDb;   // keep true peak at or below -1 dBTP
    if (m.appliedDb > room) { m.appliedDb = jmax (0.0f, room); m.limitedByPeak = true; }
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
        auto parts = StringArray::fromTokens (learn["accum"].toString(), " ", "");
        const SpinLock::ScopedLockType lock (learnLock);
        if (parts.size() == arc::kSpecBins)
            for (int i = 0; i < arc::kSpecBins; ++i) learnAccum[(size_t) i] = parts[i].getDoubleValue();
        learnFrames = (double) learn["frames"];
        learnDone = (bool) learn["done"];
        learnProgress = learnDone ? 1.0f : 0.0f;
        lastProfile = -1;   // forces the curve to be rebuilt from the saved spectrum
    }
    apvts.replaceState (state);
}

AudioProcessorEditor* PotatoEQProcessor::createEditor() { return new PotatoEQEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PotatoEQProcessor(); }
