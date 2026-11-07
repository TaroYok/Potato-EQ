#pragma once
// Cross-track collision detection: every Potato EQ instance loaded in the same DAW process
// publishes its (post-EQ) spectrum here, and any instance can read the others.
// The registry is a process-wide static, shared by all instances of the plugin module.
// (A plugin that the DAW runs "bridged" / sandboxed in its own process can't see the others.)

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace share
{
constexpr int kBands = 120;                                       // 1/12 octave, 20 Hz .. 20 kHz
inline double bandFreq (int i) { return 20.0 * std::pow (1000.0, i / (double) (kBands - 1)); }

struct Snapshot
{
    int id = 0;
    juce::String name;
    juce::Colour colour;
    std::array<float, kBands> db {};
    double ageSeconds = 1e9;   // time since the last update (large = silent / stopped)
};

class Registry
{
public:
    static Registry& get() { static Registry r; return r; }

    int add()
    {
        const juce::SpinLock::ScopedLockType l (lock);
        auto e = std::make_unique<Entry>();
        e->id = ++nextId;
        e->name = "Potato EQ " + juce::String (e->id);
        e->db.fill (-150.0f);
        entries.push_back (std::move (e));
        return nextId;
    }
    void remove (int id)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        entries.erase (std::remove_if (entries.begin(), entries.end(), [id] (auto& e) { return e->id == id; }), entries.end());
    }
    void setName (int id, const juce::String& name, std::optional<juce::uint32> colour)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        for (auto& e : entries)
            if (e->id == id) { e->name = name; if (colour) e->colour = juce::Colour (*colour); }
    }
    void publish (int id, const std::array<float, kBands>& db)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        for (auto& e : entries)
            if (e->id == id) { e->db = db; e->stamp = juce::Time::getMillisecondCounterHiRes(); }
    }
    // everything except the caller
    std::vector<Snapshot> others (int selfId) const
    {
        std::vector<Snapshot> out;
        const double now = juce::Time::getMillisecondCounterHiRes();
        const juce::SpinLock::ScopedLockType l (lock);
        for (auto& e : entries)
            if (e->id != selfId)
                out.push_back ({ e->id, e->name, e->colour, e->db, e->stamp > 0 ? (now - e->stamp) / 1000.0 : 1e9 });
        return out;
    }

private:
    struct Entry
    {
        int id = 0;
        juce::String name;
        juce::Colour colour { 0xff4dd4ff };
        std::array<float, kBands> db {};
        double stamp = 0;
    };
    mutable juce::SpinLock lock;
    std::vector<std::unique_ptr<Entry>> entries;
    int nextId = 0;
};

// Turns raw FFT magnitudes into the shared band levels (dB, peak per band, with interpolation where bins are sparse).
inline void binsToBands (const float* mag, int fftSize, double fs, std::array<float, kBands>& out)
{
    const double binHz = fs / fftSize;
    for (int i = 0; i < kBands; ++i)
    {
        const double f0 = bandFreq (i) * std::pow (2.0, -1.0 / 24.0), f1 = bandFreq (i) * std::pow (2.0, 1.0 / 24.0);
        const int b0 = (int) std::ceil (f0 / binHz), b1 = (int) std::floor (f1 / binHz);
        float m = 0;
        if (b1 < b0)
        {
            const double fb = bandFreq (i) / binHz;
            const int k = juce::jlimit (0, fftSize / 2 - 1, (int) fb);
            const float t = (float) (fb - k);
            m = mag[k] * (1 - t) + mag[k + 1] * t;
        }
        else for (int k = juce::jmax (0, b0); k <= juce::jmin (fftSize / 2, b1); ++k) m = juce::jmax (m, mag[k]);
        out[(size_t) i] = juce::Decibels::gainToDecibels (m * 4.0f / (float) fftSize, -150.0f);
    }
}
} // namespace share
