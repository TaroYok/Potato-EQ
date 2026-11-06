#pragma once
// Filter design + response math. Plain C++ (no JUCE) so it can be unit-tested.
//
// Every section is a topology-preserving-transform (TPT) state-variable filter
// (Andrew Simper / Cytomic form). It is the bilinear transform of an analog
// prototype, so its exact digital response is the analog response evaluated
// at the pre-warped frequency  s = j * tan(pi f / fs) / g.
// SVFs also stay clean while their coefficients are being modulated, which the
// dynamic EQ and parameter smoothing rely on.

#include <array>
#include <cmath>
#include <complex>
#include <algorithm>

namespace arc
{
constexpr double kPi = 3.14159265358979323846;

enum class BandType { Bell, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, TiltShelf, Count };
enum class Placement { Stereo, Left, Right, Mid, Side, Count };

inline const char* bandTypeName (BandType t)
{
    static const char* n[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf" };
    return n[(int) t];
}
inline const char* placementName (Placement p)
{
    static const char* n[] = { "Stereo", "Left", "Right", "Mid", "Side" };
    return n[(int) p];
}

// Slopes offered for cut filters (dB/oct). Shelves use 6 or 12.
constexpr std::array<int, 8> kSlopes { 6, 12, 18, 24, 36, 48, 72, 96 };

inline bool typeHasGain (BandType t)
{
    return t == BandType::Bell || t == BandType::LowShelf || t == BandType::HighShelf || t == BandType::TiltShelf;
}
inline bool typeHasSlope (BandType t)
{
    return t == BandType::LowCut || t == BandType::HighCut || t == BandType::LowShelf || t == BandType::HighShelf;
}
inline bool typeSupportsDynamic (BandType t)
{
    return t == BandType::Bell || t == BandType::LowShelf || t == BandType::HighShelf;
}

struct BandSettings
{
    bool used = false, bypass = false;
    BandType type = BandType::Bell;
    double freq = 1000.0, gainDb = 0.0, q = 0.707;
    int slope = 12;
    Placement place = Placement::Stereo;
    bool dynamic = false;
    double thresholdDb = -24.0, rangeDb = -6.0;

    bool active() const { return used && ! bypass; }
};

struct Section
{
    bool firstOrder = false;
    double g = 0, k = 0, m0 = 1, m1 = 0, m2 = 0, outGain = 1.0;
    double a1 = 0, a2 = 0, a3 = 0;   // derived processing coefficients

    void finalise()
    {
        if (firstOrder) { a1 = g / (1.0 + g); return; }
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    std::complex<double> response (double f, double fs) const
    {
        const double w = std::tan (kPi * std::min (f, fs * 0.4999) / fs) / g;
        const std::complex<double> s (0.0, w);
        if (firstOrder)
            return outGain * (m0 * s + m2) / (s + 1.0);
        return outGain * (m0 + (m1 * s + m2) / (s * s + k * s + 1.0));
    }
};

struct SectionState
{
    double ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }

    inline double process (const Section& c, double x)
    {
        if (c.firstOrder)
        {
            const double v = (x - ic1) * c.a1;
            const double lp = v + ic1;
            ic1 = lp + v;
            return c.outGain * (c.m0 * (x - lp) + c.m2 * lp);
        }
        const double v3 = x - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return c.outGain * (c.m0 * x + c.m1 * v1 + c.m2 * v2);
    }
};

constexpr int kMaxSections = 9;

struct SectionSet
{
    std::array<Section, kMaxSections> s;
    int n = 0;

    Section& add() { return s[(size_t) n++]; }

    std::complex<double> response (double f, double fs) const
    {
        std::complex<double> h (1.0, 0.0);
        for (int i = 0; i < n; ++i) h *= s[(size_t) i].response (f, fs);
        return h;
    }
    double magnitudeDb (double f, double fs) const
    {
        return 20.0 * std::log10 (std::max (1e-12, std::abs (response (f, fs))));
    }
};

inline double prewarp (double f, double fs)
{
    return std::tan (kPi * std::clamp (f, 1.0, fs * 0.49) / fs);
}

// Builds the sections for one band. gainDb is passed separately so the dynamic
// EQ can feed in its moving gain.
inline SectionSet designBand (const BandSettings& b, double gainDb, double fs)
{
    SectionSet set;
    const double q = std::clamp (b.q, 0.025, 40.0);
    const double g = prewarp (b.freq, fs);
    const double A = std::pow (10.0, gainDb / 40.0);   // sqrt of linear gain

    auto second = [&] (double gg, double kk, double m0, double m1, double m2)
    {
        auto& s = set.add();
        s.g = gg; s.k = kk; s.m0 = m0; s.m1 = m1; s.m2 = m2;
        s.finalise();
    };
    auto first = [&] (double gg, double hpMix, double lpMix, double out = 1.0)
    {
        auto& s = set.add();
        s.firstOrder = true; s.g = gg; s.m0 = hpMix; s.m2 = lpMix; s.outGain = out;
        s.finalise();
    };

    switch (b.type)
    {
        case BandType::Bell:
        {
            const double k = 1.0 / (q * A);
            second (g, k, 1.0, k * (A * A - 1.0), 0.0);
            break;
        }
        case BandType::LowShelf:
            if (b.slope <= 6) first (g / A, 1.0, A * A);    // zero/pole centred on freq
            else second (g / std::sqrt (A), 1.0 / q, 1.0, (1.0 / q) * (A - 1.0), A * A - 1.0);
            break;
        case BandType::HighShelf:
            if (b.slope <= 6) first (g * A, A * A, 1.0);
            else second (g * std::sqrt (A), 1.0 / q, A * A, (1.0 / q) * (1.0 - A) * A, 1.0 - A * A);
            break;
        case BandType::TiltShelf:
        {
            // low end at -gain/2, high end at +gain/2, pivot at freq
            const double G = std::pow (10.0, -gainDb / 20.0);       // low-shelf linear gain
            first (g / std::sqrt (G), 1.0, G, std::pow (10.0, gainDb / 40.0));
            break;
        }
        case BandType::LowCut:
        case BandType::HighCut:
        {
            const bool hp = b.type == BandType::LowCut;
            const int order = std::clamp (b.slope / 6, 1, 16);
            const int pairs = order / 2;
            for (int i = 1; i <= pairs; ++i)
            {
                // Butterworth pole Qs; the user's Q scales the resonance of the sharpest pair
                double secQ = 1.0 / (2.0 * std::sin ((2.0 * i - 1.0) * kPi / (2.0 * order)));
                if (i == 1) secQ *= q / 0.7071;
                const double k = 1.0 / secQ;
                if (hp) second (g, k, 1.0, -k, -1.0);
                else    second (g, k, 0.0, 0.0, 1.0);
            }
            if (order % 2 == 1)
            {
                if (hp) first (g, 1.0, 0.0);
                else    first (g, 0.0, 1.0);
            }
            break;
        }
        case BandType::Notch:
        {
            const double k = 1.0 / q;
            second (g, k, 1.0, -k, 0.0);
            break;
        }
        case BandType::BandPass:
        {
            const double k = 1.0 / q;
            second (g, k, 0.0, k, 0.0);
            break;
        }
        default: break;
    }
    return set;
}

// Section used by the dynamic EQ to listen to the part of the spectrum a band works on.
inline Section detectorSection (const BandSettings& b, double fs)
{
    Section s;
    const double g = prewarp (b.freq, fs);
    s.g = g;
    if (b.type == BandType::LowShelf)       { s.k = 1.414; s.m0 = 0; s.m1 = 0; s.m2 = 1; }
    else if (b.type == BandType::HighShelf) { s.k = 1.414; s.m0 = 1; s.m1 = -1.414; s.m2 = -1; }
    else { s.k = 1.0 / std::clamp (b.q, 0.3, 20.0); s.m0 = 0; s.m1 = s.k; s.m2 = 0; }
    s.finalise();
    return s;
}

// Gain change produced by the dynamic EQ for a detector level.
// Behaves like a 2:1 compressor/expander on the band, limited to +/- range.
inline double dynamicDelta (double levelDb, double thresholdDb, double rangeDb)
{
    const double over = levelDb - thresholdDb;
    if (over <= 0.0) return 0.0;
    const double d = std::min (std::abs (rangeDb), over * 0.5);
    return rangeDb < 0 ? -d : d;
}

// Log-spaced helpers
inline double freqFromNorm (double x, double lo = 10.0, double hi = 30000.0)
{
    return lo * std::pow (hi / lo, x);
}
inline double normFromFreq (double f, double lo = 10.0, double hi = 30000.0)
{
    return std::log (f / lo) / std::log (hi / lo);
}
} // namespace arc
