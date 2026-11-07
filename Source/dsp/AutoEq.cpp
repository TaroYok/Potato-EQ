#include "AutoEq.h"
#include <numeric>

namespace arc
{
// Target curves. These are deliberately smooth "tonal balance" starting points
// based on the typical long-term spectra of commercial releases and well-recorded
// sources (more low end than pink noise, a gradual roll-off above 1-2 kHz).
// They are references to steer toward, not rules.
const std::vector<Profile>& profiles()
{
    //                      31.5   63   125  250  500   1k    2k    4k    8k    16k
    static const std::vector<Profile> p {
        { "Mix: Balanced",       true,  {  4,   6,   5,   3,  1.5,  0,  -1,   -3,  -5.5, -11 }, 30, 16000, 0, 4, 6 },
        { "Mix: Hip-Hop / Trap", true,  {  7,   9,   6,   3,  1.5,  0,  -1.5, -3.5, -6,  -12 }, 25, 16000, 0, 4, 6 },
        { "Mix: EDM",            true,  {  5,   8,   6,   3,  1.5,  0,  -1,  -2.5, -4.5, -10 }, 25, 16000, 0, 4, 6 },
        { "Mix: Dubstep / DnB",  true,  {  6,   8,   5,  2.5,  1,   0,  -1,  -2.5, -4.5, -10 }, 25, 16000, 0, 4, 6 },
        { "Mix: Pop",            true,  {  3,   6,   5,   3,  1.5,  0,  -1,  -2.5, -5,  -11 }, 30, 16000, 0, 4, 6 },
        { "Mix: R&B / Soul",     true,  {  5,   8,   6,   3,  1.5,  0,  -1.5, -3.5, -6,  -12 }, 25, 16000, 0, 4, 6 },
        { "Mix: Rock / Metal",   true,  {  0,   4,   5,  3.5,  2,   0,  -0.5, -2,  -4.5, -11 }, 35, 16000, 0, 4, 6 },
        { "Mix: Lo-fi",          true,  {  2,   5,   5,  3.5,  2,   0,  -2.5, -6,  -11,  -20 }, 30, 14000, 0, 4, 8 },
        { "Mix: Acoustic / Jazz",true,  { -3,   1,   3,  2.5, 1.5,  0,  -1.5, -3.5, -6.5, -13 }, 35, 16000, 0, 4, 6 },

        { "Lead Vocal",          false, {-20, -12,  -1,   3,  2.5,  0,  -0.5, -2,   -5,  -11 }, 100, 16000, 80, 5, 8 },
        { "Backing Vocals",      false, {-24, -16,  -5,   0,   1,   0,   0,  -1.5, -4,  -10 }, 150, 16000, 150, 5, 8 },
        { "Kick",                false, {  6,  10,   4,  -2,  -6,  -8,  -8,  -9,  -12,  -18 }, 30, 12000, 25, 6, 9 },
        { "Snare",               false, {-20, -10,   2,   4,   2,   0,  -1,  -2,   -4,  -10 }, 80, 16000, 70, 6, 9 },
        { "Drum Bus",            false, {  2,   6,   4,   2,   0,   0,  -1,  -2,   -4,  -10 }, 30, 16000, 25, 5, 8 },
        { "Bass / 808",          false, {  8,  10,   4,  -2,  -6, -10, -14, -18,  -24,  -32 }, 25, 5000, 25, 6, 9 },
        { "Electric Guitar",     false, {-24, -12,   0,   3,  2.5,  0,  -1,  -3,   -9,  -20 }, 90, 10000, 80, 5, 8 },
        { "Acoustic Guitar",     false, {-20,  -8,   1,   2,   1,   0,  -0.5, -2,   -4,   -9 }, 80, 16000, 70, 5, 8 },
        { "Piano / Keys",        false, {-14,  -4,   2,   3,   2,   0,  -1.5, -4,   -7,  -14 }, 40, 16000, 35, 5, 8 },
        { "Synth Lead",          false, {-24, -14,  -4,   1,  1.5,  0,  -0.5, -2,  -4.5, -10 }, 120, 16000, 100, 5, 8 },
        { "Synth Pad",           false, {-14,  -4,   2,   3,   2,   0,  -1.5, -3.5, -6,  -12 }, 60, 16000, 50, 5, 8 },
    };
    return p;
}

static double targetAt (const Profile& p, double f)
{
    static const double oct[10] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    if (f <= oct[0]) return p.octaveDb[0];
    if (f >= oct[9]) return p.octaveDb[9] + (p.octaveDb[9] - p.octaveDb[8]) * std::log2 (f / oct[9]);
    for (int i = 0; i < 9; ++i)
        if (f <= oct[i + 1])
        {
            const double t = std::log2 (f / oct[i]);
            return p.octaveDb[(size_t) i] + t * (p.octaveDb[(size_t) i + 1] - p.octaveDb[(size_t) i]);
        }
    return 0;
}

double profileTargetDb (const Profile& p, double f) { return targetAt (p, f); }

// weight 1 inside [lo, hi], fading to 0 over half an octave outside it
static double rangeWeight (double f, double lo, double hi)
{
    if (f < lo) return std::max (0.0, 1.0 - 2.0 * std::log2 (lo / f));
    if (f > hi) return std::max (0.0, 1.0 - 2.0 * std::log2 (f / hi));
    return 1.0;
}

static void solveAutoBells (const std::array<double, kSpecBins>& desired, AutoCurve& c, double fs)
{
    auto desiredAt = [&] (double f)
    {
        const double x = std::clamp (6.0 * std::log2 (f / 20.0), 0.0, (double) kSpecBins - 1.0001);
        const int i = (int) x; const double t = x - i;
        return desired[(size_t) i] * (1 - t) + desired[(size_t) i + 1] * t;
    };
    for (int k = 0; k < kAutoBells; ++k) c.gainsDb[(size_t) k] = desiredAt (autoBellFreq (k));
    c.valid = true;
    for (int it = 0; it < 16; ++it)
    {
        for (int k = 0; k < kAutoBells; ++k)
        {
            const double f = autoBellFreq (k);
            double actual = 0.0;
            for (int j = 0; j < kAutoBells; ++j) actual += designAutoBell (j, c.gainsDb[(size_t) j], fs).magnitudeDb (f, fs);
            if (c.hpfHz > 0) actual += designAutoHpf (c.hpfHz, fs).magnitudeDb (f, fs);
            if (f < c.hpfHz * 1.5) { c.gainsDb[(size_t) k] = 0; continue; }   // don't fight the high-pass
            c.gainsDb[(size_t) k] = std::clamp (c.gainsDb[(size_t) k] + 0.7 * (desiredAt (f) - actual), -12.0, 12.0);
        }
    }
}

AutoResult computeAutoEq (const std::array<double, kSpecBins>& power, const Profile& p, double fs, const AutoTweaks& tw,
                          const std::array<double, kSpecBins>* targetPower)
{
    AutoResult r;
    double peak = 0;
    for (auto v : power) peak = std::max (peak, v);
    if (peak <= 1e-14) return r;

    std::array<bool, kSpecBins> has {};
    for (int i = 0; i < kSpecBins; ++i)
    {
        r.measuredDb[(size_t) i] = 10.0 * std::log10 (std::max (power[(size_t) i], peak * 1e-12));
        const double f = specBinFreq (i);
        const double base = targetPower != nullptr ? 10.0 * std::log10 (std::max ((*targetPower)[(size_t) i], 1e-30)) : targetAt (p, f);
        r.targetDb[(size_t) i] = base + tw.tiltDbPerOct * std::log2 (f / 1000.0)
                               + tw.bassDb / (1.0 + std::pow (f / 120.0, 4.0));
        has[(size_t) i] = power[(size_t) i] > peak * 1e-6 && specBinFreq (i) < fs * 0.45;   // within 60 dB of the loudest band
    }

    // line the target up with the measured level (average over the fit range)
    double num = 0, den = 0;
    for (int i = 0; i < kSpecBins; ++i)
    {
        const double w = has[(size_t) i] ? rangeWeight (specBinFreq (i), p.fitLo, p.fitHi) : 0.0;
        num += w * (r.measuredDb[(size_t) i] - r.targetDb[(size_t) i]); den += w;
    }
    if (den < 1.0) return r;
    const double offset = num / den;
    for (auto& t : r.targetDb) t += offset;

    // raw difference, smoothed to 1/3 octave so we only chase broad tonal trends
    std::array<double, kSpecBins> raw {};
    for (int i = 0; i < kSpecBins; ++i) raw[(size_t) i] = r.targetDb[(size_t) i] - r.measuredDb[(size_t) i];
    const double meanLevel = offset + 0.0;
    for (int i = 0; i < kSpecBins; ++i)
    {
        double s = 0, ws = 0;
        const int half = tw.smoothing <= 0 ? 6 : tw.smoothing == 1 ? 3 : 1;
        for (int d = -half; d <= half; ++d)
        {
            const int j = i + d;
            if (j < 0 || j >= kSpecBins || ! has[(size_t) j]) continue;
            const double w = half + 1.0 - std::abs (d);
            s += w * raw[(size_t) j]; ws += w;
        }
        double c = ws > 0 ? s / ws : 0.0;
        c = std::clamp (c, -tw.maxCut, tw.maxBoost);
        // never boost a region that has (almost) nothing in it, e.g. sub on a vocal
        if (c > 0 && (! has[(size_t) i] || r.measuredDb[(size_t) i] < r.targetDb[(size_t) i] - 20.0)) c = 0.0;
        c *= rangeWeight (specBinFreq (i), p.fitLo, p.fitHi);
        r.correctionDb[(size_t) i] = c;
    }
    (void) meanLevel;

    r.curve.hpfHz = p.hpfHz;
    solveAutoBells (r.correctionDb, r.curve, fs);
    r.valid = true;
    return r;
}

std::vector<BandSettings> fitParametric (const std::array<double, kSpecBins>& curveDb, int maxBands, double hpfHz, double fs)
{
    std::vector<BandSettings> out;
    std::array<double, kSpecBins> res = curveDb;

    auto subtract = [&] (const BandSettings& b, double sign)
    {
        const auto set = designBand (b, b.gainDb, fs);
        for (int i = 0; i < kSpecBins; ++i) res[(size_t) i] -= sign * set.magnitudeDb (specBinFreq (i), fs);
    };

    if (hpfHz > 0 && maxBands > 0)
    {
        BandSettings b; b.used = true; b.type = BandType::LowCut; b.freq = hpfHz; b.q = 0.7071; b.slope = 12;
        out.push_back (b); subtract (b, 1.0);
    }

    while ((int) out.size() < maxBands)
    {
        int j = 0; double best = 0;
        for (int i = 0; i < kSpecBins; ++i)
            if (specBinFreq (i) > std::max (25.0, hpfHz * 1.5) && specBinFreq (i) < 18000 && std::abs (res[(size_t) i]) > best)
            { best = std::abs (res[(size_t) i]); j = i; }
        if (best < 0.4) break;

        const double v = res[(size_t) j];
        int lo = j, hi = j;
        while (lo > 0 && res[(size_t) lo - 1] * v > 0 && std::abs (res[(size_t) lo - 1]) > std::abs (v) * 0.5) --lo;
        while (hi < kSpecBins - 1 && res[(size_t) hi + 1] * v > 0 && std::abs (res[(size_t) hi + 1]) > std::abs (v) * 0.5) ++hi;

        BandSettings b; b.used = true; b.gainDb = v;
        if (lo == 0 && hi < kSpecBins - 1 && specBinFreq (j) < 150)
        { b.type = BandType::LowShelf; b.freq = specBinFreq (hi); b.q = 0.7071; }
        else if (hi == kSpecBins - 1 && specBinFreq (j) > 5000)
        { b.type = BandType::HighShelf; b.freq = specBinFreq (lo); b.q = 0.7071; }
        else
        {
            const double bw = std::max (0.33, (hi - lo + 1) / 6.0);   // octaves at half gain
            const double p2 = std::pow (2.0, bw);
            b.type = BandType::Bell; b.freq = specBinFreq (j);
            b.q = std::clamp (std::sqrt (p2) / (p2 - 1.0), 0.3, 6.0);
        }
        out.push_back (b); subtract (b, 1.0);
    }

    // refine gains a little (bands overlap)
    for (int it = 0; it < 6; ++it)
        for (auto& b : out)
        {
            if (! typeHasGain (b.type)) continue;
            subtract (b, -1.0);
            const double x = std::clamp (6.0 * std::log2 (b.freq / 20.0), 0.0, (double) kSpecBins - 1);
            const int i = (int) std::lround (x);
            const double want = res[(size_t) i];
            b.gainDb = std::clamp (b.type == BandType::Bell ? want : b.gainDb * 0.5 + want * 0.5, -18.0, 18.0);
            subtract (b, 1.0);
        }
    return out;
}

double autoGainFor (const std::array<double, kSpecBins>& curveDb)
{
    const auto& ref = profiles()[0];
    double num = 0, den = 0;
    for (int i = 0; i < kSpecBins; ++i)
    {
        const double w = std::pow (10.0, targetAt (ref, specBinFreq (i)) / 10.0);
        num += w * std::pow (10.0, curveDb[(size_t) i] / 10.0);
        den += w;
    }
    return -10.0 * std::log10 (num / den);
}
} // namespace arc
