#pragma once
// Cross-track collision detection: every Potato EQ instance loaded in the same DAW process publishes its
// (post-EQ) spectrum here, so any instance can show the others and highlight where they collide.
// The registry is a process-wide static, shared by all instances of the plugin module.
// (A plugin the DAW runs "bridged" / sandboxed in its own process can't see the others.)
// Only the worker threads and the message thread touch it, never the audio thread.

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "dsp/Collision.h"
#include <memory>
#include <optional>
#include <vector>

namespace share
{
constexpr int kBands = arc::collide::kBands;

struct Snapshot
{
    int id = 0;
    juce::String name;
    juce::Colour colour;
    arc::collide::Spectrum db {};
    double ageSeconds = 1e9;   // time since the last update (large = stopped / not playing)
    bool live() const { return ageSeconds < 0.5; }
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
        e->db.fill (-150.0f);
        entries.push_back (std::move (e));
        return nextId;
    }
    void remove (int id)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        entries.erase (std::remove_if (entries.begin(), entries.end(), [id] (auto& e) { return e->id == id; }), entries.end());
    }
    // track name / colour as reported by the DAW (if it does)
    void setHostInfo (int id, const juce::String& name, std::optional<juce::uint32> colour)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        if (auto* e = find (id)) { e->hostName = name; if (colour) { e->colour = juce::Colour (*colour); e->hasColour = true; } }
    }
    // name the user typed (wins over the DAW's name)
    void setCustomName (int id, const juce::String& name)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        if (auto* e = find (id)) e->customName = name.trim();
    }
    void publish (int id, const arc::collide::Spectrum& db)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        if (auto* e = find (id)) { e->db = db; e->stamp = juce::Time::getMillisecondCounterHiRes(); }
    }
    std::vector<Snapshot> all() const
    {
        std::vector<Snapshot> out;
        const double now = juce::Time::getMillisecondCounterHiRes();
        const juce::SpinLock::ScopedLockType l (lock);
        for (auto& e : entries)
            out.push_back ({ e->id, nameOf (*e), e->hasColour ? e->colour : palette (e->id), e->db,
                             e->stamp > 0 ? (now - e->stamp) / 1000.0 : 1e9 });
        return out;
    }
    std::optional<Snapshot> one (int id) const
    {
        for (auto& s : all()) if (s.id == id) return s;
        return std::nullopt;
    }

private:
    struct Entry
    {
        int id = 0;
        juce::String hostName, customName;
        juce::Colour colour; bool hasColour = false;
        arc::collide::Spectrum db {};
        double stamp = 0;
    };
    Entry* find (int id) { for (auto& e : entries) if (e->id == id) return e.get(); return nullptr; }
    static juce::String nameOf (const Entry& e)
    {
        if (e.customName.isNotEmpty()) return e.customName;
        if (e.hostName.isNotEmpty()) return e.hostName;
        return "Potato EQ " + juce::String (e.id);
    }
    static juce::Colour palette (int id)
    {
        static const juce::uint32 c[] = { 0xff4dd4ff, 0xffffb347, 0xffb18cff, 0xff7cf29a, 0xffff7aa8, 0xfff2e85c, 0xff5ce1e6, 0xffff8c69 };
        return juce::Colour (c[(size_t) (id - 1) % 8]);
    }
    mutable juce::SpinLock lock;
    std::vector<std::unique_ptr<Entry>> entries;
    int nextId = 0;
};

// Turns raw FFT magnitudes into the shared band levels (dB; power summed over each 1/12-octave band,
// interpolated where a band is narrower than an FFT bin).
inline void binsToBands (const float* mag, int fftSize, double fs, arc::collide::Spectrum& out)
{
    const double binHz = fs / fftSize;
    const double scale = 16.0 / (1.5 * (double) fftSize * fftSize);   // Hann: a full-scale sine reads ~0 dB
    for (int i = 0; i < kBands; ++i)
    {
        const double lo = arc::collide::bandLo (i) / binHz, hi = arc::collide::bandHi (i) / binHz;
        const int b0 = (int) std::ceil (lo), b1 = (int) std::floor (hi);
        double p = 0;
        if (b1 < b0)
        {   // band narrower than a bin: take the interpolated bin power, scaled to the band's width
            const double fb = arc::collide::bandFreq (i) / binHz;
            const int k = juce::jlimit (1, fftSize / 2 - 1, (int) fb);
            const double t = fb - k;
            const double m = mag[k] * (1 - t) + mag[k + 1] * t;
            p = m * m * (hi - lo);
        }
        else for (int k = juce::jmax (1, b0); k <= juce::jmin (fftSize / 2, b1); ++k) p += (double) mag[k] * mag[k];
        out[(size_t) i] = (float) (10.0 * std::log10 (p * scale + 1e-15));
    }
}
} // namespace share
