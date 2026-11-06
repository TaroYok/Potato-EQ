// Offline checks for the Potato EQ DSP (filters, engine, loudness meter, auto-EQ).
#include "dsp/AutoEq.h"
#include "dsp/Loudness.h"
#include <cstdio>
#include <random>

using namespace arc;
static int failures = 0;
static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("%s  %-58s got %9.3f  want %9.3f\n", ok ? "PASS" : "FAIL", what, got, want);
    if (! ok) ++failures;
}
static void near (const char* what, double got, double want, double tol) { check (std::abs (got - want) <= tol, what, got, want); }

static double runSineRmsDb (EqEngine& eq, double f, double fs, double amp = 0.25, int ch = 2)
{
    const int n = (int) fs;   // 1 s
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (2 * kPi * f * i / fs));
    float* io[2] = { L.data(), R.data() };
    for (int off = 0; off < n; off += 512) { float* p[2] = { io[0] + off, io[1] + off }; eq.process (p, ch, std::min (512, n - off)); }
    double s = 0; for (int i = n / 2; i < n; ++i) s += (double) L[(size_t) i] * L[(size_t) i];
    return 10 * std::log10 (s / (n / 2)) - 20 * std::log10 (amp / std::sqrt (2.0));
}

int main()
{
    const double fs = 48000;

    std::puts ("-- filter design --");
    BandSettings b; b.used = true; b.freq = 1000; b.q = 1.0;
    b.type = BandType::Bell;      near ("bell +6 dB at centre", designBand (b, 6, fs).magnitudeDb (1000, fs), 6, 0.01);
                                  near ("bell -9 dB at centre", designBand (b, -9, fs).magnitudeDb (1000, fs), -9, 0.01);
    b.type = BandType::LowShelf;  near ("low shelf +6: 20 Hz", designBand (b, 6, fs).magnitudeDb (20, fs), 6, 0.1);
                                  near ("low shelf +6: midpoint at freq", designBand (b, 6, fs).magnitudeDb (1000, fs), 3, 0.1);
                                  near ("low shelf +6: 15 kHz", designBand (b, 6, fs).magnitudeDb (15000, fs), 0, 0.15);
    b.type = BandType::HighShelf; near ("high shelf -4: 18 kHz", designBand (b, -4, fs).magnitudeDb (18000, fs), -4, 0.15);
                                  near ("high shelf -4: 30 Hz", designBand (b, -4, fs).magnitudeDb (30, fs), 0, 0.05);
    b.type = BandType::TiltShelf; near ("tilt +6: lows -3", designBand (b, 6, fs).magnitudeDb (20, fs), -3, 0.2);
                                  near ("tilt +6: highs +3", designBand (b, 6, fs).magnitudeDb (20000, fs), 3, 0.25);
    b.type = BandType::LowCut; b.q = 0.7071;
    for (int s : kSlopes)
    {
        b.slope = s;
        char name[80]; std::snprintf (name, sizeof name, "low cut %2d dB/oct: -3 dB at cutoff", s);
        near (name, designBand (b, 0, fs).magnitudeDb (1000, fs), -3.01, 0.05);
        std::snprintf (name, sizeof name, "low cut %2d dB/oct: slope one octave down", s);
        near (name, designBand (b, 0, fs).magnitudeDb (500, fs) - designBand (b, 0, fs).magnitudeDb (250, fs), s, std::max (1.0, s * 0.06));
    }
    b.type = BandType::HighCut; b.slope = 24; near ("high cut 24 at cutoff", designBand (b, 0, fs).magnitudeDb (1000, fs), -3.01, 0.05);
    b.type = BandType::Notch; b.q = 4; check (designBand (b, 0, fs).magnitudeDb (1000, fs) < -60, "notch depth at centre", designBand (b, 0, fs).magnitudeDb (1000, fs), -60);

    std::puts ("-- engine (processing matches design) --");
    {
        std::array<BandSettings, kNumBands> bands {};
        bands[0].used = true; bands[0].type = BandType::Bell; bands[0].freq = 1000; bands[0].gainDb = 6; bands[0].q = 1;
        bands[1].used = true; bands[1].type = BandType::LowCut; bands[1].freq = 100; bands[1].slope = 24; bands[1].q = 0.7071;
        for (double f : { 60.0, 300.0, 1000.0, 5000.0 })
        {
            EqEngine eq; eq.prepare (fs); eq.setBands (bands);
            const double want = designBand (bands[0], 6, fs).magnitudeDb (f, fs) + designBand (bands[1], 0, fs).magnitudeDb (f, fs);
            char name[80]; std::snprintf (name, sizeof name, "sine %5.0f Hz through bell+cut", f);
            near (name, runSineRmsDb (eq, f, fs), want, 0.1);
        }
        // mid/side: a side-only cut leaves a mono signal untouched
        std::array<BandSettings, kNumBands> ms {};
        ms[0].used = true; ms[0].type = BandType::Bell; ms[0].freq = 1000; ms[0].gainDb = -12; ms[0].q = 1; ms[0].place = Placement::Side;
        EqEngine eq; eq.prepare (fs); eq.setBands (ms);
        near ("side-only band leaves mono signal alone", runSineRmsDb (eq, 1000, fs), 0, 0.01);
        ms[0].place = Placement::Mid; EqEngine eq2; eq2.prepare (fs); eq2.setBands (ms);
        near ("mid-only band cuts mono signal", runSineRmsDb (eq2, 1000, fs), -12, 0.1);
        // dynamic: loud signal over threshold -> range reached
        std::array<BandSettings, kNumBands> dy {};
        dy[0].used = true; dy[0].type = BandType::Bell; dy[0].freq = 1000; dy[0].q = 1; dy[0].gainDb = 0;
        dy[0].dynamic = true; dy[0].thresholdDb = -40; dy[0].rangeDb = -6;
        EqEngine eq3; eq3.prepare (fs); eq3.setBands (dy);
        near ("dynamic bell pulls loud 1 kHz down by range", runSineRmsDb (eq3, 1000, fs, 0.5), -6, 0.3);
        EqEngine eq4; eq4.prepare (fs); eq4.setBands (dy);
        near ("dynamic bell leaves quiet signal alone", runSineRmsDb (eq4, 1000, fs, 0.001), 0, 0.3);
        // sidechain: band listens to a separate loud signal -> ducks the (quiet) main signal anyway
        dy[0].sidechain = true;
        EqEngine eq5; eq5.prepare (fs); eq5.setBands (dy);
        {
            const int n = (int) fs; std::vector<float> L ((size_t) n), R ((size_t) n), S ((size_t) n);
            for (int i = 0; i < n; ++i) { L[(size_t) i] = R[(size_t) i] = (float) (0.001 * std::sin (2 * kPi * 1000 * i / fs)); S[(size_t) i] = (float) (0.5 * std::sin (2 * kPi * 1000 * i / fs)); }
            for (int off = 0; off < n; off += 512)
            {
                float* io[2] = { L.data() + off, R.data() + off }; const float* sc[1] = { S.data() + off };
                eq5.process (io, 2, std::min (512, n - off), sc, 1);
            }
            double e = 0; for (int i = n / 2; i < n; ++i) e += (double) L[(size_t) i] * L[(size_t) i];
            near ("sidechain: loud key signal ducks the quiet main signal", 10 * std::log10 (e / (n / 2)) - 20 * std::log10 (0.001 / std::sqrt (2.0)), -6, 0.3);
        }
    }

    std::puts ("-- loudness (EBU test signals) --");
    auto runMeter = [&] (LoudnessMeter& m, double ampDb, double secs, double f = 1000, double phase = 0)
    {
        const double a = std::pow (10.0, ampDb / 20.0);
        const int n = (int) (secs * fs);
        std::vector<float> x (1024);
        static long t = 0;
        for (int off = 0; off < n; off += 1024)
        {
            const int k = std::min (1024, n - off);
            for (int i = 0; i < k; ++i) x[(size_t) i] = (float) (a * std::sin (2 * kPi * f * (double) (t + i) / fs + phase));
            t += k;
            const float* ch[2] = { x.data(), x.data() };
            m.process (ch, 2, k);
        }
    };
    {
        LoudnessMeter m; m.prepare (fs); runMeter (m, -23, 20);
        auto r = m.get();
        near ("1 kHz stereo sine at -23 dBFS: integrated", r.integrated, -23.0, 0.1);
        near ("  momentary", r.momentary, -23.0, 0.1);
        near ("  short-term", r.shortTerm, -23.0, 0.1);
    }
    {
        LoudnessMeter m; m.prepare (fs); runMeter (m, -20, 20); runMeter (m, -30, 20);
        near ("EBU 3342 #1: 20 s @-20 then 20 s @-30 -> LRA 10", m.get().lra, 10.0, 1.0);
        near ("  integrated (both halves pass the -10 LU gate)", m.get().integrated, 10 * std::log10 ((0.01 + 0.001) / 2), 0.1);
    }
    {
        LoudnessMeter m; m.prepare (fs); runMeter (m, 0, 2, fs / 4, kPi / 4);
        near ("0 dBFS sine at fs/4, 45 deg: sample peak", m.get().samplePeakDb, -3.01, 0.05);
        near ("  true peak (inter-sample)", m.get().truePeakDb, 0.0, 0.3);
    }
    {
        LoudnessMeter m; m.prepare (44100); // other sample rate must still be accurate
        const double a = std::pow (10.0, -23 / 20.0); std::vector<float> x (44100 * 10);
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (a * std::sin (2 * kPi * 1000 * (double) i / 44100));
        const float* ch[2] = { x.data(), x.data() }; m.process (ch, 2, (int) x.size());
        near ("same at 44.1 kHz", m.get().integrated, -23.0, 0.1);
    }

    std::puts ("-- auto EQ --");
    {
        const auto& prof = profiles()[0];
        std::array<double, kSpecBins> pw {};
        // a mix that matches the target, except +5 dB too much around 300 Hz and 3 dB too dull up top
        for (int i = 0; i < kSpecBins; ++i)
        {
            const double f = specBinFreq (i);
            double db = 0;
            { // target value
                BandSettings dummy; (void) dummy;
            }
            static const double oct[10] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
            const double x = std::clamp (std::log2 (f / 31.5), 0.0, 9.0);
            const int k = std::min (8, (int) x); const double t = x - k;
            db = prof.octaveDb[(size_t) k] * (1 - t) + prof.octaveDb[(size_t) k + 1] * t;
            (void) oct;
            db += 5.0 * std::exp (-std::pow (std::log2 (f / 300.0) / 0.4, 2));
            if (f > 6000) db -= 3.0;
            pw[(size_t) i] = std::pow (10.0, (db - 40) / 10.0);
        }
        auto r = computeAutoEq (pw, prof, fs);
        check (r.valid, "auto EQ produced a curve", r.valid, 1);
        const double at300 = autoCurveDb (r.curve, 1.0, 300, fs);
        const double at1k = autoCurveDb (r.curve, 1.0, 1000, fs);
        const double at12k = autoCurveDb (r.curve, 1.0, 12000, fs);
        check (at300 < -2.5 && at300 > -6.5, "cuts the 300 Hz build-up", at300, -4.5);
        check (std::abs (at1k) < 1.2, "leaves 1 kHz roughly alone", at1k, 0);
        check (at12k > 1.0 && at12k < 4.5, "adds air when the top is dull", at12k, 2.5);
        std::array<double, kSpecBins> c {};
        for (int i = 0; i < kSpecBins; ++i) c[(size_t) i] = autoCurveDb (r.curve, 1.0, specBinFreq (i), fs);
        auto bandsFit = fitParametric (c, 8, 0, fs);
        double worst = 0;
        std::array<double, kSpecBins> got {};
        for (auto& bb : bandsFit) for (int i = 0; i < kSpecBins; ++i) got[(size_t) i] += designBand (bb, bb.gainDb, fs).magnitudeDb (specBinFreq (i), fs);
        for (int i = 0; i < kSpecBins; ++i) if (specBinFreq (i) > 30 && specBinFreq (i) < 16000) worst = std::max (worst, std::abs (got[(size_t) i] - c[(size_t) i]));
        check (worst < 1.0, "commit-to-bands matches auto curve (max error dB)", worst, 1.0);
        std::printf ("      (used %zu parametric bands)\n", bandsFit.size());
        const double ag = autoGainFor (c);
        check (std::abs (ag) < 3, "auto gain compensation is sensible", ag, 0);
    }
    {
        // vocal: sub-bass is empty -> must never be boosted; HPF suggested
        const auto& voc = profiles()[9];
        std::array<double, kSpecBins> pw {};
        for (int i = 0; i < kSpecBins; ++i) { const double f = specBinFreq (i); pw[(size_t) i] = f < 90 ? 1e-12 : std::pow (10.0, -3 * std::log2 (f / 300) / 10.0); }
        auto r = computeAutoEq (pw, voc, fs);
        check (autoCurveDb (r.curve, 1, 40, fs) < 0.01, "vocal: no boost where there's no content", autoCurveDb (r.curve, 1, 40, fs), 0);
        near ("vocal: high-pass suggested at", r.curve.hpfHz, 80, 0.1);
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
