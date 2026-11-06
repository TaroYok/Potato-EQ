#pragma once
// Look-ahead brickwall peak limiter (stereo-linked). Minimum filter over the look-ahead
// window, release smoothing, then a moving average of the same length, so the gain is
// already down when the peak arrives: no overshoot, no clicks. Real-time safe.

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace arc
{
class Limiter
{
public:
    void prepare (double fs)
    {
        look = std::max (8, (int) std::lround (fs * 0.0015));          // 1.5 ms
        rel = std::exp (-1.0 / (fs * 0.080));                            // 80 ms release
        delay.assign (2, std::vector<float> ((size_t) look, 0.0f));
        minBuf.assign ((size_t) look, 1.0);
        avgBuf.assign ((size_t) look, 1.0);
        reset();
    }
    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (minBuf.begin(), minBuf.end(), 1.0);
        std::fill (avgBuf.begin(), avgBuf.end(), 1.0);
        avgSum = (double) look; env = 1.0; pos = 0; minGain = 1.0f;
    }
    int latency() const { return look - 1; }   // gain reaches its minimum exactly when the peak leaves the delay
    float gainReductionDb() const { return -20.0f * std::log10 (std::max (1e-6f, minGain)); }
    void clearMeter() { minGain = 1.0f; }

    void process (float* const* ch, int numCh, int n, double ceilingDb)
    {
        const double ceil = std::pow (10.0, ceilingDb / 20.0);
        for (int i = 0; i < n; ++i)
        {
            double peak = 0;
            for (int c = 0; c < numCh; ++c) peak = std::max (peak, (double) std::abs (ch[c][i]));
            const double need = peak > ceil ? ceil / peak : 1.0;

            // running minimum over the look-ahead window (window is short: linear scan is fine)
            minBuf[(size_t) pos] = need;
            double m = 1.0;
            for (double v : minBuf) m = std::min (m, v);
            env = m < env ? m : env * rel + m * (1.0 - rel);

            avgSum += env - avgBuf[(size_t) pos];
            avgBuf[(size_t) pos] = env;
            const double g = std::min (1.0, avgSum / look);

            for (int c = 0; c < numCh; ++c)
            {
                auto& d = delay[(size_t) c];
                d[(size_t) pos] = ch[c][i];
                const float out = d[(size_t) ((pos + 1) % look)];   // written look-1 samples ago
                ch[c][i] = (float) (out * g);
            }
            minGain = std::min (minGain, (float) g);
            pos = (pos + 1) % look;
        }
    }

private:
    int look = 64, pos = 0;
    double rel = 0.999, env = 1.0, avgSum = 64;
    std::vector<std::vector<float>> delay;
    std::vector<double> minBuf, avgBuf;
    float minGain = 1.0f;
};
} // namespace arc
