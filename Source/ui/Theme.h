#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Look: Frutiger Aero / iPod touch era, modernised - glossy black glass, chrome rims,
// slate-blue navigation bar, iPhone OS blue accents, glossy orbs for knobs and nodes.
namespace theme
{
using juce::Colour;
inline Colour bgTop     { 0xff2a2d33 };
inline Colour bgBottom  { 0xff040405 };
inline Colour bg        { 0xff111214 };
inline Colour plotBg    { 0xff0a0c10 };
inline Colour panel     { 0xff2b2f37 };
inline Colour panelHi   { 0xff3d434e };
inline Colour grid      { 0xff262b33 };
inline Colour gridText  { 0xff8e96a3 };
inline Colour text      { 0xffffffff };
inline Colour textDim   { 0xffa9b1bd };
inline Colour accent    { 0xff2f8bff };   // iPhone OS blue
inline Colour accent2   { 0xff4cd964 };   // iOS green
inline const Colour potato    { 0xfff2c14e };   // logo gold
inline const Colour autoCol   { 0xffffa94d };
inline const Colour specCol   { 0xff4ff0d8 };   // spectral dynamics (aqua)
inline const Colour good      { 0xff7cf29a };
inline const Colour warn      { 0xffffd166 };
inline const Colour bad       { 0xffff6b6b };
inline Colour chrome    { 0xffd5dae1 };   // brushed silver
inline Colour navTop    { 0xff9aaecb };   // iPhone OS navigation bar
inline Colour navBottom { 0xff4d6487 };
inline Colour navText   { 0xffffffff };
inline const Colour preSpec   { 0xff7fb3e6 };
inline const Colour postSpec  { 0xffbfe9ff };
inline const Colour scSpec    { 0xffd29bff };
constexpr float radius = 12.0f;

inline Colour bandColour (int band)
{
    static const juce::uint32 c[] = { 0xffff7b7b, 0xffffb35c, 0xffffe066, 0xffa6e86b, 0xff4ce0b3, 0xff4fd2ef,
                                      0xff63b3ff, 0xff8c9dff, 0xffb48cff, 0xffe58cf2, 0xffff8fbd, 0xffdfe7ee };
    return Colour (c[band % 12]);
}

// ---- interface font (embedded open-licence fonts, chosen in Display > Font)
inline juce::Typeface::Ptr& uiRegular() { static juce::Typeface::Ptr p; return p; }
inline juce::Typeface::Ptr& uiBold()    { static juce::Typeface::Ptr p; return p; }
inline int& openEditors()               { static int n = 0; return n; }
inline float& uiFontScale()             { static float k = 1.0f; return k; }   // evens out each font's built-in spacing
inline const juce::StringArray fontNames { "Classic (system)", "Nunito (soft, rounded)", "Inter (clean, very readable)",
                                           "DM Sans (soft geometric)", "Manrope (modern)", "Quicksand (softest)" };
void setUiFont (int index);   // Panels.cpp (needs the embedded font data)

inline juce::Font font (float h, bool bold = false)
{
    if (auto& tf = bold ? uiBold() : uiRegular())
        return juce::Font (juce::FontOptions (tf).withHeight (h * uiFontScale()));
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}
// same size and weight, in the chosen interface font
inline juce::Font withFace (const juce::Font& f) { return font (f.getHeight(), f.isBold()); }

// Rounded glass panel: translucent body, soft top sheen, thin bright rim.
inline void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float rad = radius, Colour tint = panel)
{
    g.setGradientFill (juce::ColourGradient (tint.brighter (0.18f).withAlpha (0.92f), 0, r.getY(), tint.darker (0.35f).withAlpha (0.92f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    auto sheen = r.reduced (2.0f).removeFromTop (juce::jmin (34.0f, r.getHeight() * 0.45f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f), 0, sheen.getY(), juce::Colours::white.withAlpha (0.0f), 0, sheen.getBottom(), false));
    g.fillRoundedRectangle (sheen, rad - 2.0f);
    // chrome rim: brighter on top, darker at the bottom
    g.setGradientFill (juce::ColourGradient (chrome.withAlpha (0.55f), 0, r.getY(), chrome.withAlpha (0.12f), 0, r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.2f);
}

// Glossy orb (knob caps, band nodes, toggle bubbles).
inline void drawOrb (juce::Graphics& g, juce::Point<float> c, float r, Colour col, bool bright = false)
{
    const auto body = juce::Rectangle<float> (2 * r, 2 * r).withCentre (c);
    g.setGradientFill (juce::ColourGradient (col.brighter (bright ? 0.7f : 0.45f), c.x, c.y - r * 0.6f, col.darker (0.55f), c.x, c.y + r, true));
    g.fillEllipse (body);
    auto hl = juce::Rectangle<float> (r * 1.3f, r * 0.8f).withCentre ({ c.x, c.y - r * 0.42f });
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.75f), hl.getCentreX(), hl.getY(), juce::Colours::white.withAlpha (0.0f), hl.getCentreX(), hl.getBottom(), false));
    g.fillEllipse (hl);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (body, 1.0f);
}

inline juce::String freqText (double f)
{
    return f >= 1000 ? juce::String (f / 1000.0, f >= 10000 ? 1 : 2) + " kHz" : juce::String (f, f < 100 ? 1 : 0) + " Hz";
}

// "A1 +12c" style note name (A4 = 440 Hz)
inline juce::String noteName (double f)
{
    if (f <= 0) return {};
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
    const int n = (int) std::lround (midi);
    const int cents = (int) std::lround ((midi - n) * 100.0);
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    if (cents != 0) s << " " << (cents > 0 ? "+" : "") << cents << "c";
    return s;
}

// ---- whole-interface colour schemes (switched from the Display menu, saved with the project)
struct Scheme
{
    const char* name;
    juce::uint32 bgTop, bgBottom, plotBg, panel, panelHi, grid, gridText, text, textDim, accent, accent2, chrome, navTop, navBottom, navText;
    int palette;   // matching analyzer gradient
};
inline const std::array<Scheme, 6>& schemes()
{
    static const std::array<Scheme, 6> s { {
        { "iPod touch",     0xff2a2d33, 0xff040405, 0xff0a0c10, 0xff2b2f37, 0xff3d434e, 0xff262b33, 0xff8e96a3, 0xffffffff, 0xffa9b1bd,
                            0xff2f8bff, 0xff4cd964, 0xffd5dae1, 0xff9aaecb, 0xff4d6487, 0xffffffff, 0 },
        { "Frutiger Aero",  0xff0f3352, 0xff06121f, 0xff071624, 0xff16344f, 0xff1f4a6e, 0xff1d3a55, 0xff7f9bb3, 0xffeaf6ff, 0xff9db8cc,
                            0xff5ad1ff, 0xff9be564, 0xffbfe6ff, 0xff6fd3ff, 0xff1f7fb8, 0xffffffff, 1 },
        { "Silver Classic", 0xff3a3d42, 0xff15161a, 0xff0d0f12, 0xff3a3f47, 0xff4d545f, 0xff2c3138, 0xff9aa2ad, 0xffffffff, 0xffb8bfc9,
                            0xff4a90e2, 0xff7ed957, 0xffe9ecf0, 0xffeef0f3, 0xffaab2bd, 0xff1d2128, 5 },
        { "Midnight",       0xff1d1830, 0xff07050d, 0xff0b0814, 0xff2a2340, 0xff3b3158, 0xff241d36, 0xff9089a8, 0xfff5f0ff, 0xffb3abc9,
                            0xffb26bff, 0xffff6bcb, 0xffd8cff0, 0xff8a7ad1, 0xff3f2f86, 0xffffffff, 7 },
        { "Potato Gold",    0xff2f261b, 0xff0a0705, 0xff100c08, 0xff3a2f22, 0xff4d3f2e, 0xff2d251b, 0xffa89a85, 0xfffff8ec, 0xffc9b9a0,
                            0xfff2b544, 0xffff8a4c, 0xffeedfc4, 0xffe6c27a, 0xff9a6a1f, 0xff2a1a05, 6 },
        { "Mint",           0xff16302c, 0xff040b0a, 0xff081210, 0xff1d3a35, 0xff2a5049, 0xff1a332e, 0xff86a8a0, 0xfff0fffb, 0xffa6c7bf,
                            0xff3fe0b0, 0xffb6f36b, 0xffcff5ea, 0xff7fe3c6, 0xff1f8a72, 0xff06231c, 2 } } };
    return s;
}

// Custom scheme: the user picks these; everything else is derived from them.
inline const juce::StringArray customSlotNames { "Background", "Panels", "Analyzer background", "Accent", "Second accent",
                                                 "Title bar", "Text", "Title text", "Rims / chrome" };
constexpr int kCustomSlots = 9;

inline std::array<Colour, kCustomSlots> slotsFromScheme (int index)
{
    const auto& c = schemes()[(size_t) juce::jlimit (0, (int) schemes().size() - 1, index)];
    return { Colour (c.bgTop), Colour (c.panel), Colour (c.plotBg), Colour (c.accent), Colour (c.accent2),
             Colour (c.navTop).interpolatedWith (Colour (c.navBottom), 0.5f), Colour (c.text), Colour (c.navText), Colour (c.chrome) };
}

inline void applyCustom (const std::array<Colour, kCustomSlots>& k)
{
    bgTop = k[0]; bgBottom = k[0].darker (0.85f); bg = k[0].darker (0.5f);
    panel = k[1]; panelHi = k[1].brighter (0.35f);
    plotBg = k[2]; grid = k[2].interpolatedWith (k[6], 0.12f); gridText = k[6].interpolatedWith (k[2], 0.45f);
    accent = k[3]; accent2 = k[4];
    navTop = k[5].brighter (0.35f); navBottom = k[5].darker (0.3f);
    text = k[6]; textDim = k[6].interpolatedWith (k[1], 0.35f);
    navText = k[7]; chrome = k[8];
}

inline void applyScheme (int index)
{
    const auto& c = schemes()[(size_t) juce::jlimit (0, (int) schemes().size() - 1, index)];
    bgTop = Colour (c.bgTop); bgBottom = Colour (c.bgBottom); plotBg = Colour (c.plotBg); bg = Colour (c.bgBottom).brighter (0.1f);
    panel = Colour (c.panel); panelHi = Colour (c.panelHi); grid = Colour (c.grid); gridText = Colour (c.gridText);
    text = Colour (c.text); textDim = Colour (c.textDim); accent = Colour (c.accent); accent2 = Colour (c.accent2);
    chrome = Colour (c.chrome); navTop = Colour (c.navTop); navBottom = Colour (c.navBottom); navText = Colour (c.navText);
}

// Analyzer display settings live as properties on the plugin state (saved with the project).
namespace ana
{
    inline const juce::Identifier freeze ("anaFreeze"), peakHold ("anaPeakHold"), resolution ("anaRes"),
                                  speed ("anaSpeed"), tilt ("anaTilt"), range ("anaRange"),
                                  showSc ("anaShowSc"), collide ("anaCollide"), smoothing ("anaSmooth"), noteGrid ("anaNoteGrid"),
                                  view ("anaView"), fill ("anaFill"), palette ("anaPalette"), customLight ("anaLight"), customDark ("anaDark"),
                                  scheme ("uiScheme"), units ("uiUnits"), uiFont ("uiFont"),
                                  cmpWith ("cmpWith"), cmpSens ("cmpSens");
    inline juce::Identifier customSlot (int i) { return juce::Identifier ("uiC" + juce::String (i)); }
    inline const juce::StringArray unitNames { "LUFS", "dB (dBFS)", "Both" };
    inline const juce::StringArray smoothingNames { "Off (1/48 oct)", "Light (1/24 oct)", "Medium (1/12 oct)", "Heavy (1/6 oct)", "Very heavy (1/3 oct)" };
    inline const juce::StringArray resolutionNames { "Low", "Medium", "High", "Maximum" };   // FFT 2k/4k/8k/16k
    inline const juce::StringArray speedNames { "Slow", "Medium", "Fast" };
    inline const juce::StringArray tiltNames { "0 dB/oct", "3 dB/oct", "4.5 dB/oct", "6 dB/oct" };
    inline const juce::StringArray rangeNames { "60 dB", "90 dB", "120 dB" };
    inline const juce::StringArray viewNames { "Spectrum", "Spectrogram (waterfall)" };
    inline const juce::StringArray fillNames { "Outline only", "Colour by level", "Heat map (by height)", "Slices (bars)" };
    inline const double tiltValues[] = { 0.0, 3.0, 4.5, 6.0 };
    inline const double rangeValues[] = { 60.0, 90.0, 120.0 };
    inline int get (const juce::ValueTree& s, const juce::Identifier& id, int def) { return (int) s.getProperty (id, def); }
}

// Colour gradient used for the analyzer: darkest = quiet, lightest = loud.
struct Palette
{
    Colour dark, mid, light;
    Colour at (float t) const   // t: 0 = quiet .. 1 = loud
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return t < 0.5f ? dark.interpolatedWith (mid, t * 2.0f) : mid.interpolatedWith (light, (t - 0.5f) * 2.0f);
    }
};

struct PalettePreset { const char* name; juce::uint32 light, mid, dark; };
inline const std::array<PalettePreset, 8>& palettePresets()
{
    static const std::array<PalettePreset, 8> p { {
        { "iPod Blue",    0xffe6f2ff, 0xff2f8bff, 0xff081a3a },
        { "Aero Sky",     0xffeafcff, 0xff3fb8f5, 0xff0a3a6b },
        { "Aqua Lime",    0xfff6ffd8, 0xff49d6a8, 0xff0b4d5c },
        { "Sunset",       0xfffff1c1, 0xffff7a59, 0xff4a1259 },
        { "Ember",        0xfffff7d6, 0xffff7b00, 0xff3a0a00 },
        { "Ice",          0xffffffff, 0xff9fc9ff, 0xff1b2a4a },
        { "Potato Gold",  0xfffff6d5, 0xffe8b04b, 0xff3b2600 },
        { "Neon Violet",  0xfff5e6ff, 0xffb26bff, 0xff22075a } } };
    return p;
}

inline Palette paletteFrom (const juce::ValueTree& s)
{
    const int idx = ana::get (s, ana::palette, 0);
    if (idx >= 0 && idx < (int) palettePresets().size())
    {
        const auto& p = palettePresets()[(size_t) idx];
        return { Colour (p.dark), Colour (p.mid), Colour (p.light) };
    }
    // custom: user picks lightest and darkest; the middle is a saturated blend of the two
    const Colour light ((juce::uint32) (juce::int64) s.getProperty (ana::customLight, (juce::int64) 0xffe6f2ff));
    const Colour dark ((juce::uint32) (juce::int64) s.getProperty (ana::customDark, (juce::int64) 0xff081a3a));
    const Colour m = dark.interpolatedWith (light, 0.5f);
    return { dark, m.withSaturation (juce::jmin (1.0f, m.getSaturation() * 1.4f + 0.1f)), light };
}
// Scheme from the plugin state: a preset, or the custom colours.
inline void applySchemeFromState (const juce::ValueTree& st)
{
    const int idx = ana::get (st, ana::scheme, 0);
    if (idx < (int) schemes().size()) { applyScheme (idx); return; }
    auto k = slotsFromScheme (0);
    for (int i = 0; i < kCustomSlots; ++i)
        if (st.hasProperty (ana::customSlot (i))) k[(size_t) i] = Colour ((juce::uint32) (juce::int64) st.getProperty (ana::customSlot (i)));
    applyCustom (k);
}
} // namespace theme
namespace ana = theme::ana;
