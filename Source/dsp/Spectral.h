#pragma once
// Spectral dynamics (like Pro-Q 4's "Spectral" dynamic mode).
// A normal dynamic band moves its whole gain when the band gets loud. A spectral band instead works
// frequency by frequency inside its shape: only the frequencies that poke above the threshold are turned
// down (or up), the rest are left alone. That tames a harsh resonance in a vocal or guitar without dulling
// everything around it.
//
// How: short-time FFT (Hann windows, 75% overlap). For every frame and every spectral band we measure each
// frequency's level (smoothed over a width set by Density, optionally tilted +3 dB/oct), compare it with the
// threshold (manual, or automatic = the band's own typical level + a margin), turn the excess into a gain
// change (2:1, limited to the band's range), weight it by the band's shape, smooth it over time (attack /
// release) and apply it to the spectrum. The static part of the band (its gain knob) still runs in the
// normal EQ filters; only the dynamic part lives here. Latency = FFT size while any band is spectral.

#include "EqEngine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

namespace arc
{
// Small radix-2 complex FFT (no external dependency, so the DSP tests can run it).
class Fft
{
public:
    void init (int size)
    {
        n = size;
        rev.assign ((size_t) n, 0);
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; }
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i) tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * kPi * i / n));
    }
    int size() const { return n; }
    // in place; inverse is unnormalised (divide by n yourself)
    void run (std::complex<float>* x, bool inverse) const
    {
        for (int i = 0; i < n; ++i) if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    auto w = tw[(size_t) (j * step)];
                    if (inverse) w = std::conj (w);
                    const auto u = x[i + j], v = x[i + j + half] * w;
                    x[i + j] = u + v; x[i + j + half] = u - v;
                }
        }
    }
private:
    int n = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

class SpectralDynamics
{
public:
    static constexpr int kMaxN = 4096;
    static constexpr int kMaxBins = kMaxN / 2 + 1;
    static constexpr int kResolutions[3] = { 1024, 2048, 4096 };
    static constexpr float kAutoMarginDb = 5.0f;     // how far above the band's typical level a frequency must stick out (auto)

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < 3; ++i) ffts[(size_t) i].init (kResolutions[i]);
        for (auto& b : inBuf) b.assign (kMaxN, 0.0f);
        for (auto& b : outAcc) b.assign (kMaxN, 0.0f);
        for (auto& b : outReady) b.assign (kMaxN / 4, 0.0f);
        scBuf.assign (kMaxN, 0.0f);
        window.assign (kMaxN, 0.0f);
        X.assign (kMaxN, {}); SC.assign (kMaxN, {});
        SL.assign (kMaxBins, {}); SR.assign (kMaxBins, {});
        for (auto& p : power) p.assign (kMaxBins, 0.0);
        for (auto& c : cum) c.assign (kMaxBins + 1, 0.0);
        for (auto& g : gainDb) g.assign (kMaxBins, 0.0f);
        dens.assign (kMaxBins, 0.0f);
        for (auto& s : state) { s.g.assign (kMaxBins, 0.0f); s.weight.assign (kMaxBins, 0.0f); s.keyValid = false; s.baselineValid = false; }
        setResolution (resIndex, true);
    }

    // 0 = Low (1024), 1 = Medium (2048), 2 = High (4096)
    void setResolution (int index, bool force = false)
    {
        index = std::clamp (index, 0, 2);
        if (index == resIndex && ! force) return;
        resIndex = index;
        N = kResolutions[index]; hop = N / 4;
        for (int i = 0; i < N; ++i) window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));
        for (auto& s : state) s.keyValid = false;
        reset();
    }
    int resolution() const { return resIndex; }
    int latency() const { return N; }

    void reset()
    {
        for (auto& b : inBuf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : outAcc) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : outReady) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (scBuf.begin(), scBuf.end(), 0.0f);
        for (auto& s : state) { std::fill (s.g.begin(), s.g.end(), 0.0f); s.baselineValid = false; s.amount = 0; }
        pos = N - hop; readPos = 0;
        for (auto& a : shownDb) a.store (0.0f, std::memory_order_relaxed);
        for (auto& a : bandAmount) a.store (0.0f, std::memory_order_relaxed);
    }

    void setBands (const std::array<BandSettings, kNumBands>& b) { bands = b; }
    bool anySpectral() const { return std::any_of (bands.begin(), bands.end(), [] (const BandSettings& s) { return s.isSpectral(); }); }

    // io: the signal after the EQ filters (it is also what the spectral bands listen to, unless sidechained)
    void process (float* const* io, int numCh, int numSamples, const float* const* sc = nullptr, int scCh = 0)
    {
        if (numCh <= 0) return;
        numCh = std::min (numCh, 2);
        scPresent = sc != nullptr && scCh > 0;
        for (int i = 0; i < numSamples; ++i)
        {
            const float l = io[0][i], r = numCh > 1 ? io[1][i] : l;
            inBuf[0][(size_t) pos] = l; inBuf[1][(size_t) pos] = r;
            scBuf[(size_t) pos] = scPresent ? (scCh > 1 ? 0.5f * (sc[0][i] + sc[1][i]) : sc[0][i]) : 0.0f;
            io[0][i] = outReady[0][(size_t) readPos];
            if (numCh > 1) io[1][i] = outReady[1][(size_t) readPos];
            ++pos; ++readPos;
            if (pos == N)
            {
                frame();
                for (auto& b : inBuf) std::copy (b.begin() + hop, b.begin() + N, b.begin());
                std::copy (scBuf.begin() + hop, scBuf.begin() + N, scBuf.begin());
                pos = N - hop; readPos = 0;
            }
        }
    }

    // ---- for the display (any thread) ----
    double binHz() const { return fs / N; }
    int numBins() const { return N / 2 + 1; }
    // total gain change at a bin, dB (negative = reduction); sampled from the latest frame
    float shownGainDb (int bin) const { return shownDb[(size_t) std::clamp (bin, 0, kMaxBins - 1)].load (std::memory_order_relaxed); }
    // strongest gain change a band is currently making, dB
    float activity (int band) const { return bandAmount[(size_t) band].load (std::memory_order_relaxed); }

private:
    enum Variant { vStereo, vMid, vSide, vLeft, vRight, vSc, kVariants };

    struct BandState
    {
        std::vector<float> g;        // current gain change per bin (dB, before the shape weight)
        std::vector<float> weight;   // 0..1: how much of the band's shape covers each bin
        int k0 = 0, k1 = -1;         // bins the band covers
        BandType type = BandType::Bell; double freq = 0, q = 0; int slope = 0; bool keyValid = false;
        float baseline = 0; bool baselineValid = false;
        float amount = 0;
    };

    static int targetOf (Placement p)
    {
        switch (p)
        {
            case Placement::Mid:   return vMid;
            case Placement::Side:  return vSide;
            case Placement::Left:  return vLeft;
            case Placement::Right: return vRight;
            default:               return vStereo;
        }
    }

    void updateWeight (BandState& st, const BandSettings& b)
    {
        if (st.keyValid && st.type == b.type && st.slope == b.slope
            && std::abs (st.freq - b.freq) <= b.freq * 1e-4 && std::abs (st.q - b.q) <= b.q * 1e-4) return;
        st.type = b.type; st.freq = b.freq; st.q = b.q; st.slope = b.slope; st.keyValid = true;
        auto shapeBand = b; shapeBand.gainDb = 12.0;
        const auto set = designBand (shapeBand, 12.0, fs);
        const int nb = N / 2 + 1;
        st.k0 = nb; st.k1 = -1;
        for (int k = 1; k < nb; ++k)
        {
            const double f = k * fs / N;
            const float w = (float) std::clamp (set.magnitudeDb (f, fs) / 12.0, 0.0, 1.0);
            st.weight[(size_t) k] = w > 0.01f ? w : 0.0f;
            if (w > 0.01f) { st.k0 = std::min (st.k0, k); st.k1 = std::max (st.k1, k); }
        }
        st.weight[0] = 0.0f;
    }

    void frame()
    {
        const int nb = N / 2 + 1;
        const auto& fft = ffts[(size_t) resIndex];
        // ---- analysis: both channels in one complex FFT
        for (int i = 0; i < N; ++i) X[(size_t) i] = { inBuf[0][(size_t) i] * window[(size_t) i], inBuf[1][(size_t) i] * window[(size_t) i] };
        fft.run (X.data(), false);
        for (int k = 0; k < nb; ++k)
        {
            const auto a = X[(size_t) k], b = std::conj (X[(size_t) ((N - k) % N)]);
            SL[(size_t) k] = 0.5f * (a + b);
            SR[(size_t) k] = std::complex<float> (0.0f, -0.5f) * (a - b);
        }

        // ---- which detectors are needed
        std::array<bool, kVariants> need {};
        bool anyBand = false;
        for (auto& b : bands)
            if (b.isSpectral())
            {
                anyBand = true;
                need[(size_t) ((b.sidechain && scPresent) ? (int) vSc : detectorOf (b.place))] = true;
            }
        for (auto& g : gainDb) std::fill (g.begin(), g.begin() + nb, 0.0f);
        if (anyBand)
        {
            if (need[vSc])
            {
                for (int i = 0; i < N; ++i) SC[(size_t) i] = { scBuf[(size_t) i] * window[(size_t) i], 0.0f };
                fft.run (SC.data(), false);
            }
            const double scale = 16.0 / (1.5 * (double) N * N);   // a full-scale sine reads about 0 dB
            for (int v = 0; v < kVariants; ++v)
            {
                if (! need[(size_t) v]) continue;
                auto& p = power[(size_t) v];
                for (int k = 0; k < nb; ++k)
                {
                    const auto l = SL[(size_t) k], r = SR[(size_t) k];
                    double x = 0;
                    switch (v)
                    {
                        case vStereo: x = 0.5 * (std::norm (l) + std::norm (r)); break;
                        case vMid:    x = std::norm (0.5f * (l + r)); break;
                        case vSide:   x = std::norm (0.5f * (l - r)); break;
                        case vLeft:   x = std::norm (l); break;
                        case vSc:     x = std::norm (SC[(size_t) k]); break;
                        default:      x = std::norm (r); break;   // right
                    }
                    p[(size_t) k] = k == 0 ? 0.0 : x * scale;
                }
                auto& c = cum[(size_t) v];
                c[0] = 0.0;
                for (int k = 0; k < nb; ++k) c[(size_t) k + 1] = c[(size_t) k] + p[(size_t) k];
            }

            const double framePeriod = hop / fs;
            for (int bi = 0; bi < kNumBands; ++bi)
            {
                const auto& b = bands[(size_t) bi];
                auto& st = state[(size_t) bi];
                if (! b.isSpectral()) { st.amount = 0; st.baselineValid = false; continue; }
                updateWeight (st, b);
                if (st.k1 < st.k0) { st.amount = 0; continue; }
                const int det = (b.sidechain && scPresent) ? (int) vSc : detectorOf (b.place);
                const auto& c = cum[(size_t) det];
                auto sumTo = [&] (double x)   // integral of power from bin 0 to x (bin k covers [k-0.5, k+0.5])
                {
                    const double p = std::clamp (x + 0.5, 0.0, (double) nb);
                    const int i = std::min (nb - 1, (int) p);
                    return c[(size_t) i] + (p - i) * (c[(size_t) i + 1] - c[(size_t) i]);
                };
                const double widthOct = std::pow (24.0, -std::clamp (b.specDensity, 0.0, 1.0));   // 1 oct .. 1/24 oct
                const double half = std::pow (2.0, widthOct / 2.0);
                // level of each bin: power summed over the density window (manual threshold compares this,
                // so it reads like a band level), or average power density (auto compares the shape)
                double wsum = 0, bsum = 0;
                for (int k = st.k0; k <= st.k1; ++k)
                {
                    double lo = k / half, hi = k * half;
                    if (hi - lo < 3.0) { lo = k - 1.5; hi = k + 1.5; }
                    const double sum = std::max (0.0, sumTo (hi) - sumTo (lo));
                    const double f = k * fs / N;
                    const double tilt = b.specTilt ? 3.0 * std::log2 (f / 1000.0) : 0.0;
                    dens[(size_t) k] = b.specAuto ? (float) (10.0 * std::log10 (sum / (hi - lo) + 1e-30) + tilt)
                                                  : (float) (10.0 * std::log10 (sum + 1e-30) + tilt);
                    const double w = st.weight[(size_t) k];
                    if (b.specAuto && w > 0.1 && dens[(size_t) k] > -200.0f) { wsum += w; bsum += w * dens[(size_t) k]; }
                }
                float threshold = (float) b.thresholdDb;
                if (b.specAuto)
                {
                    if (wsum > 0)
                    {
                        const float now = (float) (bsum / wsum);
                        const float a = (float) std::exp (-framePeriod / 0.3);   // the typical level follows over ~0.3 s
                        st.baseline = st.baselineValid ? a * st.baseline + (1 - a) * now : now;
                        st.baselineValid = true;
                    }
                    threshold = st.baseline + kAutoMarginDb;
                }
                const double atkMs = b.specAuto ? 5.0 : b.specAttackMs, relMs = b.specAuto ? 80.0 : b.specReleaseMs;
                const float atk = (float) std::exp (-framePeriod / std::max (1e-4, atkMs * 0.001));
                const float rel = (float) std::exp (-framePeriod / std::max (1e-4, relMs * 0.001));
                const float range = (float) b.rangeDb;
                auto& out = gainDb[(size_t) targetOf (b.place)];
                float strongest = 0;
                for (int k = st.k0; k <= st.k1; ++k)
                {
                    const float over = dens[(size_t) k] - threshold;
                    const float t = (over <= 0 || dens[(size_t) k] < -100.0f) ? 0.0f
                                  : (range < 0 ? -std::min (-range, over * 0.5f) : std::min (range, over * 0.5f));
                    float& g = st.g[(size_t) k];
                    const float coef = std::abs (t) > std::abs (g) ? atk : rel;
                    g = coef * g + (1 - coef) * t;
                    const float applied = g * st.weight[(size_t) k];
                    out[(size_t) k] += applied;
                    if (std::abs (applied) > std::abs (strongest)) strongest = applied;
                }
                st.amount = strongest;
            }
        }
        for (int bi = 0; bi < kNumBands; ++bi) bandAmount[(size_t) bi].store (anyBand ? state[(size_t) bi].amount : 0.0f, std::memory_order_relaxed);

        // ---- apply
        if (anyBand)
        {
            const bool ms = std::any_of (gainDb[vMid].begin(), gainDb[vMid].begin() + nb, [] (float g) { return g != 0.0f; })
                         || std::any_of (gainDb[vSide].begin(), gainDb[vSide].begin() + nb, [] (float g) { return g != 0.0f; });
            for (int k = 0; k < nb; ++k)
            {
                auto l = SL[(size_t) k], r = SR[(size_t) k];
                const float gs = dbToGain (gainDb[vStereo][(size_t) k]);
                if (ms)
                {
                    auto m = 0.5f * (l + r) * gs * dbToGain (gainDb[vMid][(size_t) k]);
                    auto s = 0.5f * (l - r) * gs * dbToGain (gainDb[vSide][(size_t) k]);
                    l = m + s; r = m - s;
                }
                else { l *= gs; r *= gs; }
                l *= dbToGain (gainDb[vLeft][(size_t) k]);
                r *= dbToGain (gainDb[vRight][(size_t) k]);
                SL[(size_t) k] = l; SR[(size_t) k] = r;
                shownDb[(size_t) k].store (gainDb[vStereo][(size_t) k] + gainDb[vMid][(size_t) k] + gainDb[vSide][(size_t) k]
                                           + 0.5f * (gainDb[vLeft][(size_t) k] + gainDb[vRight][(size_t) k]), std::memory_order_relaxed);
            }
        }
        else
            for (int k = 0; k < nb; ++k) shownDb[(size_t) k].store (0.0f, std::memory_order_relaxed);

        // ---- synthesis: rebuild the packed spectrum (L + jR), inverse FFT, window, overlap-add
        for (int k = 0; k < nb; ++k) X[(size_t) k] = SL[(size_t) k] + std::complex<float> (0.0f, 1.0f) * SR[(size_t) k];
        for (int k = nb; k < N; ++k) X[(size_t) k] = std::conj (SL[(size_t) (N - k)]) + std::complex<float> (0.0f, 1.0f) * std::conj (SR[(size_t) (N - k)]);
        fft.run (X.data(), true);
        const float norm = 1.0f / (N * 1.5f);   // inverse FFT scale and the Hann^2 overlap sum at 75% overlap
        for (int i = 0; i < N; ++i)
        {
            const float w = window[(size_t) i] * norm;
            outAcc[0][(size_t) i] += X[(size_t) i].real() * w;
            outAcc[1][(size_t) i] += X[(size_t) i].imag() * w;
        }
        for (int c = 0; c < 2; ++c)
        {
            std::copy (outAcc[(size_t) c].begin(), outAcc[(size_t) c].begin() + hop, outReady[(size_t) c].begin());
            std::copy (outAcc[(size_t) c].begin() + hop, outAcc[(size_t) c].begin() + N, outAcc[(size_t) c].begin());
            std::fill (outAcc[(size_t) c].begin() + (N - hop), outAcc[(size_t) c].begin() + N, 0.0f);
        }
    }

    static int detectorOf (Placement p) { return targetOf (p); }
    static float dbToGain (float db) { return db == 0.0f ? 1.0f : std::pow (10.0f, db / 20.0f); }

    double fs = 48000.0;
    int resIndex = 1, N = 2048, hop = 512, pos = 0, readPos = 0;
    bool scPresent = false;
    std::array<Fft, 3> ffts;
    std::array<std::vector<float>, 2> inBuf, outAcc, outReady;
    std::vector<float> scBuf, window, dens;
    std::vector<std::complex<float>> X, SC, SL, SR;
    std::array<std::vector<double>, kVariants> power, cum;
    std::array<std::vector<float>, kVariants> gainDb;
    std::array<BandSettings, kNumBands> bands {};
    std::array<BandState, kNumBands> state;
    std::array<std::atomic<float>, kMaxBins> shownDb {};
    std::array<std::atomic<float>, kNumBands> bandAmount {};
};
} // namespace arc
