#pragma once
// ITU-R BS.1770-4 / EBU R128 loudness meter: momentary (400 ms), short-term (3 s),
// gated integrated loudness, loudness range (EBU Tech 3342) and 4x-oversampled true peak.
// Plain C++; process() is real-time safe (no allocation).

#include <array>
#include <atomic>
#include <vector>

namespace arc
{
class LoudnessMeter
{
public:
    struct Readings
    {
        float momentary = -100, shortTerm = -100, integrated = -100, lra = 0;
        float truePeakDb = -100, samplePeakDb = -100, momentaryMax = -100, shortTermMax = -100;
        // plain dBFS metering (unweighted)
        float rmsDb = -100, rmsSlowDb = -100;   // RMS over ~300 ms and ~3 s (a 0 dBFS sine reads -3.01 dBFS)
        float peakHoldDb = -100;                // sample peak, held 3 s, then falls 20 dB/s
        bool clipped = false;                   // a sample reached 0 dBFS since the last reset
        bool integratedValid = false;
    };

    void prepare (double sampleRate);
    void reset();
    void process (const float* const* ch, int numCh, int numSamples);
    Readings get() const;

    static constexpr double kFloor = -100.0;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void endOfSubBlock();
    double integratedFromHistogram (bool& valid) const;
    double lraFromHistogram() const;

    static constexpr int kHistBins = 3750;           // -70 .. +5 LUFS in 0.02 LU steps
    static int binFor (double lufs);
    static double energyFromLufs (double l);
    static double lufsFromEnergy (double e);

    double fs = 48000;
    int subLen = 4800, subPos = 0;
    std::array<std::array<Biquad, 2>, 2> kw {};          // [channel][stage]
    double subSum = 0;
    std::array<double, 30> ring {};                      // last 3 s of 100 ms energies
    int ringCount = 0, ringPos = 0;

    std::vector<double> intEnergy, stEnergy;             // per-bin energy sums
    std::vector<unsigned> intCount, stCount;
    unsigned intTotal = 0, stTotal = 0;

    // true peak
    static constexpr int kTaps = 48, kPhases = 4, kPerPhase = kTaps / kPhases;
    std::array<double, kTaps> tpCoef {};
    std::array<std::array<double, kPerPhase>, 2> tpHist {};
    int tpPos = 0;
    double tpMax = 0, spMax = 0;

    // dBFS meter state
    double rmsFast = 0, rmsSlow = 0, rmsFastCoef = 0, rmsSlowCoef = 0;
    double peakHold = 0, holdLeft = 0;
    bool clip = false;
    std::atomic<float> aRms { -100 }, aRmsSlow { -100 }, aPeakHold { -100 };
    std::atomic<bool> aClip { false };

    std::atomic<float> aM { -100 }, aS { -100 }, aI { -100 }, aLra { 0 }, aTp { -100 }, aSp { -100 }, aMmax { -100 }, aSmax { -100 };
    std::atomic<bool> aIvalid { false };
};
} // namespace arc
