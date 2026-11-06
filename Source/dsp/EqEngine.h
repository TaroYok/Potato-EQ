#pragma once
// Zero-latency (minimum-phase) EQ engine: 24 parametric bands + the auto-EQ layer.
// Plain C++ so it can be tested without JUCE.

#include "Filters.h"
#include <atomic>
#include <vector>

namespace arc
{
constexpr int kNumBands = 24;

// ---- Auto-EQ layer: a fixed bank of 2/3-octave bells whose gains are fitted to
// the correction curve, plus an optional high-pass. Fixed frequencies mean the
// gains can glide smoothly while the analyser keeps updating them.
constexpr int kAutoBells = 15;
constexpr double kAutoQ = 2.15;   // 2/3-octave bandwidth
inline double autoBellFreq (int i) { return 31.5 * std::pow (2.0, 2.0 * i / 3.0); }

struct AutoCurve
{
    std::array<double, kAutoBells> gainsDb {};
    double hpfHz = 0.0;   // 0 = off
    bool valid = false;
};

inline SectionSet designAutoBell (int i, double gainDb, double fs)
{
    BandSettings b; b.type = BandType::Bell; b.freq = autoBellFreq (i); b.q = kAutoQ;
    return designBand (b, gainDb, fs);
}
inline SectionSet designAutoHpf (double hz, double fs)
{
    BandSettings b; b.type = BandType::LowCut; b.freq = hz; b.q = 0.7071; b.slope = 12;
    return designBand (b, 0.0, fs);
}

// Response (dB) of the auto layer at frequency f, for the GUI and the fitter.
inline double autoCurveDb (const AutoCurve& c, double amount, double f, double fs)
{
    if (! c.valid) return 0.0;
    double db = 0.0;
    for (int i = 0; i < kAutoBells; ++i)
        if (std::abs (c.gainsDb[(size_t) i] * amount) > 1e-3)
            db += designAutoBell (i, c.gainsDb[(size_t) i] * amount, fs).magnitudeDb (f, fs);
    if (c.hpfHz > 0.0 && amount > 0.0) db += designAutoHpf (c.hpfHz, fs).magnitudeDb (f, fs);
    return db;
}

class EqEngine
{
public:
    static constexpr int kSub = 32;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        smooth = 1.0 - std::exp (-kSub / (fs * 0.025));
        autoSmooth = 1.0 - std::exp (-kSub / (fs * 0.4));
        envAtt = std::exp (-1.0 / (fs * 0.003));
        envRel = std::exp (-1.0 / (fs * 0.120));
        reset();
    }

    void reset()
    {
        for (auto& b : bands) { b.init = false; b.resetState(); b.env = 0; b.delta.store (0.0f); }
        for (auto& a : autoSt) for (auto& s : a) s.reset();
        for (auto& a : hpfSt) for (auto& s : a) s.reset();
        autoCur.fill (0.0);
        hpfCurHz = 0.0;
    }

    void setBands (const std::array<BandSettings, kNumBands>& b) { for (int i = 0; i < kNumBands; ++i) bands[(size_t) i].target = b[(size_t) i]; }
    void setGainScale (double s) { gainScale = s; }
    void setAuto (const AutoCurve& c, double amount, bool on)
    {
        autoTarget = c; autoAmount = amount; autoOn = on && c.valid;
    }
    float dynamicDelta (int band) const { return bands[(size_t) band].delta.load (std::memory_order_relaxed); }
    float autoGainNow (int i) const { return (float) autoCur[(size_t) i]; }

    // sc / scCh: optional external sidechain (used by dynamic bands with sidechain enabled)
    void process (float* const* io, int numCh, int numSamples, const float* const* sc = nullptr, int scCh = 0)
    {
        numCh = std::min (numCh, 2);
        for (int off = 0; off < numSamples; off += kSub)
        {
            const int n = std::min (kSub, numSamples - off);
            double L[kSub], R[kSub], SC[kSub];
            for (int i = 0; i < n; ++i) { L[i] = io[0][off + i]; R[i] = numCh > 1 ? io[1][off + i] : L[i]; }
            scNow = nullptr;
            if (sc != nullptr && scCh > 0)
            {
                for (int i = 0; i < n; ++i) SC[i] = scCh > 1 ? 0.5 * ((double) sc[0][off + i] + sc[1][off + i]) : sc[0][off + i];
                scNow = SC;
            }

            bool anyMS = false;
            for (auto& b : bands)
            {
                if (! b.target.active()) { b.init = false; b.delta.store (0.0f); continue; }
                const auto p = b.target.place;
                if (p == Placement::Mid || p == Placement::Side) { anyMS = true; continue; }
                if (numCh == 1 && p == Placement::Right) continue;
                runBand (b, (p != Placement::Right) ? L : nullptr,
                            (numCh > 1 && p != Placement::Left) ? R : nullptr, n);
            }

            if (anyMS && numCh > 1)
            {
                double M[kSub], S[kSub];
                for (int i = 0; i < n; ++i) { M[i] = 0.5 * (L[i] + R[i]); S[i] = 0.5 * (L[i] - R[i]); }
                for (auto& b : bands)
                {
                    if (! b.target.active()) continue;
                    if (b.target.place == Placement::Mid)  runBand (b, M, nullptr, n);
                    if (b.target.place == Placement::Side) runBand (b, S, nullptr, n);
                }
                for (int i = 0; i < n; ++i) { L[i] = M[i] + S[i]; R[i] = M[i] - S[i]; }
            }
            else if (anyMS && numCh == 1)
            {
                for (auto& b : bands)
                    if (b.target.active() && b.target.place == Placement::Mid) runBand (b, L, nullptr, n);
            }

            runAuto (L, numCh > 1 ? R : nullptr, n);

            for (int i = 0; i < n; ++i) { io[0][off + i] = (float) L[i]; if (numCh > 1) io[1][off + i] = (float) R[i]; }
        }
    }

private:
    struct Band
    {
        BandSettings target, cur;
        bool init = false;
        double logF = 0, gain = 0, logQ = 0;
        SectionSet sec;
        Section det;
        std::array<std::array<SectionState, kMaxSections>, 2> st;
        std::array<SectionState, 2> detSt;
        double env = 0;
        std::atomic<float> delta { 0.0f };

        void resetState() { for (auto& c : st) for (auto& s : c) s.reset(); for (auto& s : detSt) s.reset(); }
    };

    void runBand (Band& b, double* a, double* c, int n)
    {
        const auto& t = b.target;
        const bool structural = ! b.init || t.type != b.cur.type || t.slope != b.cur.slope || t.place != b.cur.place;
        if (structural)
        {
            b.logF = std::log (t.freq); b.gain = t.gainDb; b.logQ = std::log (t.q);
            b.resetState(); b.env = 0; b.init = true;
        }
        else
        {
            b.logF += (std::log (t.freq) - b.logF) * smooth;
            b.gain += (t.gainDb - b.gain) * smooth;
            b.logQ += (std::log (t.q) - b.logQ) * smooth;
        }
        b.cur = t;
        b.cur.freq = std::exp (b.logF); b.cur.q = std::exp (b.logQ);

        // dynamic EQ: listen to this band's region of the incoming signal
        double delta = 0.0;
        if (t.dynamic && typeSupportsDynamic (t.type))
        {
            b.det = detectorSection (b.cur, fs);
            double env = b.env;
            const bool useSc = t.sidechain && scNow != nullptr;
            for (int i = 0; i < n; ++i)
            {
                double x = 0.0;
                if (useSc) x = std::abs (b.detSt[0].process (b.det, scNow[i]));
                else
                {
                    if (a) x = std::max (x, std::abs (b.detSt[0].process (b.det, a[i])));
                    if (c) x = std::max (x, std::abs (b.detSt[1].process (b.det, c[i])));
                }
                env = x > env ? envAtt * env + (1 - envAtt) * x : envRel * env + (1 - envRel) * x;
            }
            b.env = env;
            delta = arc::dynamicDelta (20.0 * std::log10 (env + 1e-9), t.thresholdDb, t.rangeDb);
        }
        else b.env = 0;
        b.delta.store ((float) delta, std::memory_order_relaxed);

        b.sec = designBand (b.cur, b.gain * gainScale + delta, fs);
        if (a) for (int i = 0; i < n; ++i) { double x = a[i]; for (int s = 0; s < b.sec.n; ++s) x = b.st[0][(size_t) s].process (b.sec.s[(size_t) s], x); a[i] = x; }
        if (c) for (int i = 0; i < n; ++i) { double x = c[i]; for (int s = 0; s < b.sec.n; ++s) x = b.st[1][(size_t) s].process (b.sec.s[(size_t) s], x); c[i] = x; }
    }

    void runAuto (double* a, double* c, int n)
    {
        bool any = false;
        for (int i = 0; i < kAutoBells; ++i)
        {
            const double target = autoOn ? autoTarget.gainsDb[(size_t) i] * autoAmount : 0.0;
            autoCur[(size_t) i] += (target - autoCur[(size_t) i]) * autoSmooth;
            if (std::abs (target - autoCur[(size_t) i]) < 1e-4) autoCur[(size_t) i] = target;
            if (std::abs (autoCur[(size_t) i]) < 1e-4) continue;
            any = true;
            const auto set = designAutoBell (i, autoCur[(size_t) i], fs);
            const auto& sec = set.s[0];
            if (a) for (int k = 0; k < n; ++k) a[k] = autoSt[(size_t) i][0].process (sec, a[k]);
            if (c) for (int k = 0; k < n; ++k) c[k] = autoSt[(size_t) i][1].process (sec, c[k]);
        }
        const double hpTarget = (autoOn && autoAmount > 0.0) ? autoTarget.hpfHz : 0.0;
        if (hpTarget <= 0.0) { if (hpfCurHz > 0) for (auto& s : hpfSt) for (auto& x : s) x.reset(); hpfCurHz = 0.0; }
        else
        {
            hpfCurHz = hpfCurHz <= 0.0 ? hpTarget : hpfCurHz + (hpTarget - hpfCurHz) * autoSmooth;
            const auto set = designAutoHpf (hpfCurHz, fs);
            const auto& sec = set.s[0];
            if (a) for (int k = 0; k < n; ++k) a[k] = hpfSt[0][0].process (sec, a[k]);
            if (c) for (int k = 0; k < n; ++k) c[k] = hpfSt[1][0].process (sec, c[k]);
        }
        (void) any;
    }

    double fs = 48000.0, smooth = 0.1, autoSmooth = 0.01, envAtt = 0.99, envRel = 0.999;
    double gainScale = 1.0;
    const double* scNow = nullptr;
    std::array<Band, kNumBands> bands;

    AutoCurve autoTarget;
    double autoAmount = 1.0;
    bool autoOn = false;
    std::array<double, kAutoBells> autoCur {};
    std::array<std::array<SectionState, 2>, kAutoBells> autoSt;
    std::array<std::array<SectionState, 1>, 2> hpfSt;
    double hpfCurHz = 0.0;
};
} // namespace arc
