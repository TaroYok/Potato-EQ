#pragma once
// Factory presets. Each one sets up bands (all other bands are cleared).
// "Sweep" presets prepare a single filter and select it, ready to automate its frequency.

#include "dsp/Filters.h"
#include <vector>

namespace presets
{
using arc::BandType;
using arc::Placement;

struct Band
{
    BandType type; double freq, gain, q; int slope = 12;
    Placement place = Placement::Stereo;
    bool dynamic = false; double thr = -30, range = -6;
};

struct Preset
{
    const char* category;
    const char* name;
    const char* hint;        // shown after loading
    std::vector<Band> bands;
    int selectBand = -1;     // band to select in the UI afterwards
};

inline const std::vector<Preset>& all()
{
    using T = BandType;
    static const std::vector<Preset> p {
        { "General", "Default (flat)", "", {} },

        { "Character", "Radio", "Small-speaker radio sound.",
          { { T::LowCut, 350, 0, 0.9, 24 }, { T::HighCut, 4500, 0, 0.9, 24 }, { T::Bell, 1600, 4, 1.0 } } },
        { "Character", "Telephone", "Narrow phone-line band.",
          { { T::LowCut, 500, 0, 1.1, 36 }, { T::HighCut, 3000, 0, 1.1, 36 }, { T::Bell, 1200, 5, 1.4 } } },
        { "Character", "Lo-fi / Old Vinyl", "Dark, boxy and warm.",
          { { T::LowCut, 120, 0, 0.7, 12 }, { T::HighCut, 6000, 0, 0.8, 18 }, { T::Bell, 900, 2.5, 0.7 }, { T::Bell, 250, 1.5, 1.0 } } },
        { "Character", "Muffled (next room)", "Like hearing the club through a wall.",
          { { T::HighCut, 500, 0, 0.7, 24 }, { T::LowShelf, 90, 3, 0.7 } } },
        { "Character", "Megaphone", "Harsh, honky and narrow.",
          { { T::LowCut, 700, 0, 1.0, 36 }, { T::HighCut, 4000, 0, 1.0, 24 }, { T::Bell, 1800, 8, 2.0 } } },

        { "Drums", "Kick: Punch", "Weight at 60 Hz, less box, more click.",
          { { T::LowCut, 30, 0, 0.7, 24 }, { T::Bell, 60, 3, 1.2 }, { T::Bell, 350, -4, 1.5 }, { T::Bell, 4000, 3, 1.2 } } },
        { "Drums", "Kick: Sub Thump", "Deep, round kick for trap / hip-hop.",
          { { T::LowCut, 25, 0, 0.7, 24 }, { T::Bell, 50, 4, 1.0 }, { T::Bell, 250, -3, 1.0 }, { T::HighCut, 9000, 0, 0.7, 12 } } },
        { "Drums", "Snare: Crack", "Body plus top-end snap.",
          { { T::LowCut, 70, 0, 0.7, 18 }, { T::Bell, 200, 2, 1.2 }, { T::Bell, 900, -2.5, 1.5 }, { T::Bell, 5000, 3, 1.0 } } },
        { "Drums", "Hi-hats: Smooth", "Removes low junk, tames harsh peaks only when they spike.",
          { { T::LowCut, 400, 0, 0.7, 24 }, { T::Bell, 8000, 0, 2.0, 12, Placement::Stereo, true, -30, -5 } } },
        { "Drums", "Drum Bus: Glue", "Gentle bus tone-shaping.",
          { { T::LowCut, 28, 0, 0.7, 18 }, { T::Bell, 80, 1.5, 0.9 }, { T::Bell, 400, -1.5, 1.0 }, { T::HighShelf, 10000, 2, 0.7 } } },

        { "Bass", "808: Weight", "Clean sub, controlled low-mids.",
          { { T::LowCut, 25, 0, 0.7, 24 }, { T::Bell, 50, 2.5, 1.0 }, { T::Bell, 250, -3, 1.2 }, { T::HighCut, 8000, 0, 0.7, 12 } } },
        { "Bass", "Bass: Tighten", "Less mud, more note definition.",
          { { T::LowCut, 35, 0, 0.7, 24 }, { T::Bell, 200, -2, 1.2 }, { T::Bell, 800, 2, 1.5 }, { T::HighCut, 6000, 0, 0.7, 12 } } },

        { "Vocals", "Vocal: Clarity", "Presence and air, with a built-in de-esser band.",
          { { T::LowCut, 90, 0, 0.7, 18 }, { T::Bell, 250, -2.5, 1.2 }, { T::Bell, 3000, 2, 1.0 }, { T::HighShelf, 10000, 2.5, 0.7 },
            { T::Bell, 6500, 0, 3.0, 12, Placement::Stereo, true, -32, -6 } } },
        { "Vocals", "Vocal: De-mud", "Clears boxiness in the low-mids.",
          { { T::LowCut, 100, 0, 0.7, 18 }, { T::Bell, 350, -4, 1.4 }, { T::Bell, 800, -1.5, 2.0 } } },
        { "Vocals", "Vocal: De-ess", "Dynamic cut that only acts on loud S and T sounds.",
          { { T::LowCut, 80, 0, 0.7, 12 }, { T::Bell, 7000, 0, 2.5, 12, Placement::Stereo, true, -35, -8 } } },
        { "Vocals", "Backing Vocals: Tuck", "Thin and airy so they sit behind the lead.",
          { { T::LowCut, 180, 0, 0.7, 18 }, { T::Bell, 2500, -2, 1.0 }, { T::HighShelf, 9000, 2, 0.7 } } },

        { "Instruments", "Acoustic Guitar", "Less boom, more sparkle.",
          { { T::LowCut, 80, 0, 0.7, 18 }, { T::Bell, 200, -3, 1.2 }, { T::Bell, 5000, 2, 1.0 }, { T::HighShelf, 12000, 1.5, 0.7 } } },
        { "Instruments", "Piano: Bright", "Clear and present in a mix.",
          { { T::LowCut, 40, 0, 0.7, 12 }, { T::Bell, 300, -2, 1.0 }, { T::Bell, 3500, 2, 0.9 } } },
        { "Instruments", "Synth Pad: Make Room", "Scoops the pad out of the vocal range.",
          { { T::LowCut, 150, 0, 0.7, 18 }, { T::Bell, 2500, -3, 0.8 }, { T::HighShelf, 10000, 1.5, 0.7 } } },

        { "Mix / Master", "Master: Polish", "Subtle mastering tilt: tight lows, a little air.",
          { { T::LowCut, 25, 0, 0.7, 18 }, { T::LowShelf, 80, 1, 0.7 }, { T::Bell, 300, -1, 0.9 }, { T::HighShelf, 12000, 1.5, 0.7 } } },
        { "Mix / Master", "Master: Air", "Just adds shimmer on top.",
          { { T::HighShelf, 12000, 2.5, 0.7 } } },
        { "Mix / Master", "Mono Bass (M/S)", "Removes stereo below 120 Hz so the low end hits in mono.",
          { { T::LowCut, 120, 0, 0.7, 24, Placement::Side } } },
        { "Mix / Master", "Wider Top (M/S)", "Lifts the highs on the sides only.",
          { { T::LowCut, 120, 0, 0.7, 24, Placement::Side }, { T::HighShelf, 6000, 3, 0.7, 12, Placement::Side } } },

        { "Sweeps", "Low-Pass Sweep", "Automate Band 1 frequency downward for a filter-down transition.",
          { { T::HighCut, 20000, 0, 1.4, 24 } }, 0 },
        { "Sweeps", "High-Pass Sweep", "Automate Band 1 frequency upward for a build-up / riser.",
          { { T::LowCut, 20, 0, 1.4, 24 } }, 0 },
        { "Sweeps", "Band-Pass Sweep", "Automate Band 1 frequency for a wah / radio-tuning effect.",
          { { T::BandPass, 1000, 0, 2.5, 12 } }, 0 },
        { "Sweeps", "Resonant Peak Sweep", "Narrow boost to sweep through the spectrum.",
          { { T::Bell, 500, 10, 8.0 } }, 0 },
        { "Sweeps", "Phaser-style Notch Sweep", "Automate Band 1 frequency for a hollow notch sweep.",
          { { T::Notch, 1000, 0, 1.5, 12 } }, 0 },
    };
    return p;
}
} // namespace presets
