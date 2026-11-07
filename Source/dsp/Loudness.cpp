#include "Loudness.h"
#include <algorithm>
#include <cmath>

namespace arc
{
static constexpr double kPiL = 3.14159265358979323846;

void LoudnessMeter::prepare (double sampleRate)
{
    fs = sampleRate;
    subLen = std::max (1, (int) std::lround (fs * 0.1));

    // K-weighting for any sample rate (same derivation as libebur128)
    for (auto& c : kw)
    {
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPiL * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            auto& b = c[0];
            b.b0 = (Vh + Vb * K / Q + K * K) / a0;
            b.b1 = 2.0 * (K * K - Vh) / a0;
            b.b2 = (Vh - Vb * K / Q + K * K) / a0;
            b.a1 = 2.0 * (K * K - 1.0) / a0;
            b.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPiL * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            auto& b = c[1];
            b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0;
            b.a1 = 2.0 * (K * K - 1.0) / a0;
            b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    // 4x interpolator: Kaiser-windowed sinc, 48 taps (12 per phase)
    const double beta = 7.0, centre = (kTaps - 1) / 2.0;
    auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
    for (int n = 0; n < kTaps; ++n)
    {
        const double x = (n - centre) / kPhases;
        const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (kPiL * x) / (kPiL * x);
        const double r = (n - centre) / centre;
        tpCoef[(size_t) n] = sinc * bessel0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / bessel0 (beta);
    }
    for (int p = 0; p < kPhases; ++p)   // normalise every phase to unity DC gain
    {
        double s = 0; for (int j = 0; j < kPerPhase; ++j) s += tpCoef[(size_t) (p + kPhases * j)];
        for (int j = 0; j < kPerPhase; ++j) tpCoef[(size_t) (p + kPhases * j)] /= s;
    }

    intEnergy.assign (kHistBins, 0.0); stEnergy.assign (kHistBins, 0.0);
    intCount.assign (kHistBins, 0u);   stCount.assign (kHistBins, 0u);
    reset();
}

void LoudnessMeter::reset()
{
    for (auto& c : kw) for (auto& b : c) b.z1 = b.z2 = 0;
    subPos = 0; subSum = 0; ring.fill (0); ringCount = ringPos = 0;
    std::fill (intEnergy.begin(), intEnergy.end(), 0.0); std::fill (stEnergy.begin(), stEnergy.end(), 0.0);
    std::fill (intCount.begin(), intCount.end(), 0u);    std::fill (stCount.begin(), stCount.end(), 0u);
    intTotal = stTotal = 0;
    for (auto& h : tpHist) h.fill (0);
    tpPos = 0; tpMax = spMax = 0;
    rmsFast = rmsSlow = 0; peakHold = 0; holdLeft = 0; clip = false;
    aRms = aRmsSlow = aPeakHold = -100.0f; aClip = false;
    aM = aS = aI = aTp = aSp = aMmax = aSmax = -100.0f; aLra = 0.0f; aIvalid = false;
}

int LoudnessMeter::binFor (double lufs) { return std::clamp ((int) ((lufs + 70.0) / 0.02), 0, kHistBins - 1); }
double LoudnessMeter::energyFromLufs (double l) { return std::pow (10.0, (l + 0.691) / 10.0); }
double LoudnessMeter::lufsFromEnergy (double e) { return e > 0 ? -0.691 + 10.0 * std::log10 (e) : kFloor; }

void LoudnessMeter::process (const float* const* ch, int numCh, int numSamples)
{
    numCh = std::min (numCh, 2);
    rmsFastCoef = std::exp (-1.0 / (fs * 0.3));
    rmsSlowCoef = std::exp (-1.0 / (fs * 3.0));
    double blockPeak = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        double sum = 0, raw = 0;
        for (int c = 0; c < numCh; ++c)
        {
            const double x = ch[c][i];
            const double y = kw[(size_t) c][1].process (kw[(size_t) c][0].process (x));
            sum += y * y;   // channel weight 1.0 for L and R

            // true peak
            auto& h = tpHist[(size_t) c];
            h[(size_t) tpPos] = x;
            spMax = std::max (spMax, std::abs (x));
            blockPeak = std::max (blockPeak, std::abs (x));
            raw += x * x;
            for (int p = 0; p < kPhases; ++p)
            {
                double acc = 0;
                for (int j = 0; j < kPerPhase; ++j)
                    acc += tpCoef[(size_t) (p + kPhases * j)] * h[(size_t) ((tpPos - j + kPerPhase) % kPerPhase)];
                tpMax = std::max (tpMax, std::abs (acc));
            }
        }
        raw /= numCh;   // mean square across channels
        rmsFast = rmsFastCoef * rmsFast + (1.0 - rmsFastCoef) * raw;
        rmsSlow = rmsSlowCoef * rmsSlow + (1.0 - rmsSlowCoef) * raw;
        if (numCh == 1) sum *= 2.0;   // mono played on two speakers (EBU convention)
        tpPos = (tpPos + 1) % kPerPhase;
        subSum += sum;
        if (++subPos >= subLen) endOfSubBlock();
    }
    aTp.store ((float) (tpMax > 0 ? 20 * std::log10 (tpMax) : kFloor));
    aSp.store ((float) (spMax > 0 ? 20 * std::log10 (spMax) : kFloor));

    // peak hold: 3 s, then fall at 20 dB/s
    const double dt = numSamples / fs;
    if (blockPeak >= peakHold) { peakHold = blockPeak; holdLeft = 3.0; }
    else if (holdLeft > 0) holdLeft -= dt;
    else peakHold *= std::pow (10.0, -20.0 * dt / 20.0);
    if (blockPeak >= 0.99999) clip = true;
    auto toDb = [] (double v) { return (float) (v > 1e-10 ? 20 * std::log10 (v) : kFloor); };
    aPeakHold.store (toDb (peakHold));
    aRms.store ((float) (rmsFast > 1e-20 ? 10 * std::log10 (rmsFast) : kFloor));
    aRmsSlow.store ((float) (rmsSlow > 1e-20 ? 10 * std::log10 (rmsSlow) : kFloor));
    aClip.store (clip);
}

void LoudnessMeter::endOfSubBlock()
{
    ring[(size_t) ringPos] = subSum / subLen;
    ringPos = (ringPos + 1) % (int) ring.size();
    ringCount = std::min (ringCount + 1, (int) ring.size());
    subSum = 0; subPos = 0;

    auto meanOfLast = [this] (int k)
    {
        double s = 0;
        for (int j = 1; j <= k; ++j) s += ring[(size_t) ((ringPos - j + (int) ring.size()) % (int) ring.size())];
        return s / k;
    };

    if (ringCount >= 4)
    {
        const double eM = meanOfLast (4);
        const double lM = lufsFromEnergy (eM);
        aM.store ((float) lM);
        if (lM > aMmax.load()) aMmax.store ((float) lM);
        if (lM >= -70.0)   // absolute gate; 400 ms blocks with 75 % overlap
        {
            const int b = binFor (lM);
            intEnergy[(size_t) b] += eM; ++intCount[(size_t) b]; ++intTotal;
        }
        bool valid = false;
        const double I = integratedFromHistogram (valid);
        aI.store ((float) I); aIvalid.store (valid);
    }
    if (ringCount >= 1)
    {
        const double eS = meanOfLast (ringCount);
        const double lS = lufsFromEnergy (eS);
        aS.store ((float) lS);
        if (ringCount >= (int) ring.size())
        {
            if (lS > aSmax.load()) aSmax.store ((float) lS);
            if (lS >= -70.0)
            {
                const int b = binFor (lS);
                stEnergy[(size_t) b] += eS; ++stCount[(size_t) b]; ++stTotal;
                aLra.store ((float) lraFromHistogram());
            }
        }
    }
}

double LoudnessMeter::integratedFromHistogram (bool& valid) const
{
    valid = false;
    if (intTotal == 0) return kFloor;
    double e = 0; for (int b = 0; b < kHistBins; ++b) e += intEnergy[(size_t) b];
    const double gate = lufsFromEnergy (e / intTotal) - 10.0;
    double e2 = 0; unsigned n2 = 0;
    for (int b = binFor (gate); b < kHistBins; ++b) { e2 += intEnergy[(size_t) b]; n2 += intCount[(size_t) b]; }
    if (n2 == 0) return kFloor;
    valid = true;
    return lufsFromEnergy (e2 / n2);
}

double LoudnessMeter::lraFromHistogram() const
{
    if (stTotal == 0) return 0.0;
    double e = 0; for (int b = 0; b < kHistBins; ++b) e += stEnergy[(size_t) b];
    const int gateBin = binFor (lufsFromEnergy (e / stTotal) - 20.0);
    unsigned n = 0; for (int b = gateBin; b < kHistBins; ++b) n += stCount[(size_t) b];
    if (n < 2) return 0.0;
    auto percentile = [&] (double p)
    {
        const double target = p * (n - 1);
        unsigned cum = 0;
        for (int b = gateBin; b < kHistBins; ++b)
        {
            cum += stCount[(size_t) b];
            if (cum > target) return -70.0 + (b + 0.5) * 0.02;
        }
        return 5.0;
    };
    return percentile (0.95) - percentile (0.10);
}

LoudnessMeter::Readings LoudnessMeter::get() const
{
    Readings r;
    r.momentary = aM; r.shortTerm = aS; r.integrated = aI; r.lra = aLra;
    r.truePeakDb = aTp; r.samplePeakDb = aSp; r.momentaryMax = aMmax; r.shortTermMax = aSmax;
    r.rmsDb = aRms; r.rmsSlowDb = aRmsSlow; r.peakHoldDb = aPeakHold; r.clipped = aClip;
    r.integratedValid = aIvalid;
    return r;
}
} // namespace arc
