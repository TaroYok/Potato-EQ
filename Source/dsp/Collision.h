#pragma once
// Spectral collision detection between two tracks.
// Both spectra are on the same 1/12-octave grid (20 Hz .. 20 kHz). A "collision" is a region where
// BOTH tracks are among their own strongest frequencies, so they fight for the same space in the mix.
// Each track is judged against its own loudest region, so fader levels (which a plugin can't see) don't matter.

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace arc::collide
{
constexpr int kBands = 120;
inline double bandFreq (int i) { return 20.0 * std::pow (1000.0, i / (double) (kBands - 1)); }
// band edges are half a step (1/24 octave-ish) either side of the centre
inline double bandLo (int i) { return bandFreq (i) * std::pow (1000.0, -0.5 / (kBands - 1)); }
inline double bandHi (int i) { return bandFreq (i) * std::pow (1000.0, 0.5 / (kBands - 1)); }

using Spectrum = std::array<float, kBands>;

struct Region
{
    int lo = 0, hi = 0, peak = 0;   // band indices (inclusive)
    float amount = 0;               // 0..1 at the peak
    double centreHz() const { return std::sqrt (bandLo (lo) * bandHi (hi)); }
    double widthOct() const { return std::log2 (bandHi (hi) / bandLo (lo)); }
};

struct Result
{
    Spectrum amount {};             // 0..1 per band
    std::vector<Region> regions;    // strongest first
};

// Levels are energy per 1/12-octave band (pink noise reads flat). A gentle +1.5 dB/oct evens out the typical
// downward slope of a real mix, then 1/3-octave smoothing.
inline Spectrum weigh (const Spectrum& s)
{
    Spectrum t {}, out {};
    for (int i = 0; i < kBands; ++i) t[(size_t) i] = s[(size_t) i] + 1.5f * (float) std::log2 (bandFreq (i) / 1000.0);   // real mixes fall gently per octave
    for (int i = 0; i < kBands; ++i)
    {
        double p = 0; int n = 0;
        for (int j = std::max (0, i - 2); j <= std::min (kBands - 1, i + 2); ++j) { p += std::pow (10.0, t[(size_t) j] / 10.0); ++n; }
        out[(size_t) i] = (float) (10.0 * std::log10 (std::max (1e-30, p / n)));
    }
    return out;
}

// sensitivity: 0 = only the very strongest overlaps, 1 = normal, 2 = also milder overlaps
inline Result find (const Spectrum& a, const Spectrum& b, float sensitivity = 1.0f)
{
    Result r;
    const float rawMaxA = *std::max_element (a.begin(), a.end()), rawMaxB = *std::max_element (b.begin(), b.end());
    if (rawMaxA < -90.0f || rawMaxB < -90.0f) return r;   // one of them is silent
    const auto wa = weigh (a), wb = weigh (b);
    const float maxA = *std::max_element (wa.begin(), wa.end()), maxB = *std::max_element (wb.begin(), wb.end());
    const float window = 12.0f + 6.0f * std::clamp (sensitivity, 0.0f, 2.0f);   // dB below each track's peak that still counts
    for (int i = 0; i < kBands; ++i)
    {
        if (a[(size_t) i] < -90.0f || b[(size_t) i] < -90.0f) continue;
        const float ra = wa[(size_t) i] - maxA, rb = wb[(size_t) i] - maxB;   // 0 = this track's strongest region
        const float k = (std::min (ra, rb) + window) / (window - 4.0f);    // full strength within 4 dB of the peak
        r.amount[(size_t) i] = std::clamp (k, 0.0f, 1.0f);
    }
    // group into regions
    for (int i = 0; i < kBands;)
    {
        if (r.amount[(size_t) i] < 0.25f) { ++i; continue; }
        Region g; g.lo = i; g.peak = i;
        while (i < kBands && r.amount[(size_t) i] >= 0.25f)
        {
            if (r.amount[(size_t) i] > r.amount[(size_t) g.peak]) g.peak = i;
            ++i;
        }
        g.hi = i - 1; g.amount = r.amount[(size_t) g.peak];
        r.regions.push_back (g);
    }
    std::sort (r.regions.begin(), r.regions.end(), [] (const Region& x, const Region& y) { return x.amount > y.amount; });
    return r;
}

// A gentle bell that makes room for the other track: depth grows with the overlap, Q follows the region width.
struct Suggestion { double freq = 1000, gainDb = -3, q = 1; };
inline Suggestion suggestCut (const Region& g)
{
    Suggestion s;
    s.freq = bandFreq (g.peak);
    s.gainDb = -(2.0 + 2.0 * g.amount);                                   // -2 .. -4 dB
    const double bw = std::clamp (g.widthOct(), 0.25, 3.0);
    s.q = std::clamp (std::sqrt (std::pow (2.0, bw)) / (std::pow (2.0, bw) - 1.0), 0.5, 6.0);   // bandwidth (octaves) to Q
    return s;
}
} // namespace arc::collide
