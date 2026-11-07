#pragma once
// Auto-EQ: compares the long-term spectrum of the incoming audio with a target
// tonal-balance curve and works out a gentle correction.
// Plain C++ so it can be tested without JUCE.

#include "EqEngine.h"
#include <string>
#include <vector>

namespace arc
{
// Analysis grid: 1/6-octave bands from 20 Hz to ~20 kHz.
constexpr int kSpecBins = 61;
inline double specBinFreq (int i) { return 20.0 * std::pow (2.0, i / 6.0); }
inline double specBinLo (int i)   { return specBinFreq (i) * std::pow (2.0, -1.0 / 12.0); }
inline double specBinHi (int i)   { return specBinFreq (i) * std::pow (2.0,  1.0 / 12.0); }

struct Profile
{
    std::string name;
    bool isMix = true;
    // Octave-band energy relative to the 1 kHz band at 31.5, 63, 125 ... 16k Hz.
    // On this scale pink noise is a flat line.
    std::array<double, 10> octaveDb {};
    double fitLo = 30, fitHi = 16000;   // only correct inside this range
    double hpfHz = 0;                   // suggested high-pass (sources)
    double maxBoost = 4, maxCut = 6;
};

const std::vector<Profile>& profiles();

// The profile's target level (octave-band energy relative to 1 kHz) at frequency f.
double profileTargetDb (const Profile& p, double f);

struct AutoResult
{
    AutoCurve curve;
    std::array<double, kSpecBins> measuredDb {}, targetDb {}, correctionDb {};
    bool valid = false;
};

// User adjustments on top of a profile.
struct AutoTweaks
{
    double maxBoost = 3, maxCut = 4;   // dB limits for the correction
    double tiltDbPerOct = 0;           // brightness: tilts the target around 1 kHz
    double bassDb = 0;                 // low end: shelf on the target below ~120 Hz
    int smoothing = 1;                 // 0 broad (1 oct), 1 medium (1/3 oct), 2 fine (1/6 oct)
};

// measuredPower: average power in each 1/6-octave bin (linear).
// targetPowerOverride: use this spectrum (e.g. a reference track, same bins as measuredPower) as the target.
AutoResult computeAutoEq (const std::array<double, kSpecBins>& measuredPower, const Profile& p, double fs, const AutoTweaks& t = {},
                          const std::array<double, kSpecBins>* targetPowerOverride = nullptr);

// Converts any target curve (dB at the analysis bins) into at most maxBands
// editable parametric bands (bells/shelves), plus a low cut if hpfHz > 0.
std::vector<BandSettings> fitParametric (const std::array<double, kSpecBins>& curveDb, int maxBands, double hpfHz, double fs);

// Output trim (dB) that keeps the loudness of typical music roughly constant
// when the given EQ curve (dB at the analysis bins) is applied.
double autoGainFor (const std::array<double, kSpecBins>& curveDb);
} // namespace arc
