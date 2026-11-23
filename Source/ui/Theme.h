#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Look: Frutiger Aero, modernised - see-through glass panels with curved gloss, chrome rims,
// glossy title bar, aqua/lime accents, glossy orbs for knobs and nodes. Default scheme: Dark Aero.
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
// Status colours avoid the red/green pair: on-target = sky blue, warning = yellow, over/clash = bright vermillion.
// These stay apart for deuteranopia, protanopia and tritanopia (Okabe-Ito based, lifted for dark backgrounds).
inline const Colour good      { 0xff6cc8ff };
inline const Colour warn      { 0xfff5e24a };
inline const Colour bad       { 0xffff6a2a };
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
    // Colourblind-friendly: Okabe-Ito hues brightened for dark backgrounds, ordered so neighbouring
    // bands alternate warm/cool and light/dark. Each node also shows its band number.
    static const juce::uint32 c[] = { 0xffffb000, 0xff56b4e9, 0xfff0e442, 0xff4d8fff, 0xffff7a3d, 0xff3fd6a8,
                                      0xffe58cc8, 0xffbfe6ff, 0xffc9a36b, 0xff9b8cff, 0xffffd8a8, 0xffdfe7ee };
    return Colour (c[band % 12]);
}

// ---- interface font (embedded open-licence fonts, chosen in Display > Font)
inline juce::Typeface::Ptr& uiRegular() { static juce::Typeface::Ptr p; return p; }
inline juce::Typeface::Ptr& uiBold()    { static juce::Typeface::Ptr p; return p; }
inline int& openEditors()               { static int n = 0; return n; }
inline float& uiFontScale()             { static float k = 1.0f; return k; }   // evens out each font's built-in spacing
inline const juce::StringArray fontNames { "Classic (system)", "Nunito (soft, rounded)", "Inter (clean, very readable)",
                                           "DM Sans (soft geometric)", "Manrope (modern)", "Quicksand (softest)",
                                           "Inter Display (crisp, modern)" };
constexpr int kDefaultFont = 6;
void setUiFont (int index);   // Panels.cpp (needs the embedded font data)

inline juce::Font font (float h, bool bold = false)
{
    if (auto& tf = bold ? uiBold() : uiRegular())
        return juce::Font (juce::FontOptions (tf).withHeight (h * uiFontScale()));
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}
// same size and weight, in the chosen interface font
inline juce::Font withFace (const juce::Font& f) { return font (f.getHeight(), f.isBold()); }

// Classic Aero gloss: a bright reflection over the top part of a shape whose lower edge curves
// down like light on a glass bubble, plus a crisp 1 px highlight just inside the top edge.
inline void drawAeroSheen (juce::Graphics& g, juce::Rectangle<float> r, float rad, float strength = 1.0f)
{
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addRoundedRectangle (r, rad);
    g.reduceClipRegion (clip);
    const float h = juce::jmin (r.getHeight() * 0.52f, 46.0f);
    juce::Path arc;
    arc.addEllipse (r.getX() - r.getWidth() * 0.35f, r.getY() - h * 1.9f, r.getWidth() * 1.7f, h * 2.9f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.34f * strength), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.05f * strength), 0, r.getY() + h, false));
    g.fillPath (arc);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.0f), r.getX(), 0, juce::Colours::white.withAlpha (0.55f * strength), r.getCentreX(), 0, false));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + rad * 0.6f, r.getRight() - rad * 0.6f);
}

// Rounded clear-glass slab: a thin tint you can see the scene through, a thick bevelled edge that
// catches light (bright top-left, glowing bottom), a diagonal glare and a crisp curved Aero gloss.
// `smoke` adds a clear dark tint (not frost) for sheets that float over the busy analyzer.
inline void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float rad = radius, Colour tint = panel, float smoke = 0.0f)
{
    if (smoke > 0.0f) { g.setColour (juce::Colour (0xff05020c).withAlpha (smoke)); g.fillRoundedRectangle (r, rad); }
    g.setGradientFill (juce::ColourGradient (tint.withAlpha (0.26f), 0, r.getY(), tint.darker (0.6f).withAlpha (0.46f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    {   // diagonal glare across the slab
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (r, rad);
        g.reduceClipRegion (clip);
        const float d = juce::jmin (r.getWidth(), r.getHeight()) * 0.9f + 60.0f;
        juce::Path glare;
        glare.startNewSubPath (r.getX() + r.getWidth() * 0.10f, r.getY());
        glare.lineTo (r.getX() + r.getWidth() * 0.10f + d * 0.55f, r.getY());
        glare.lineTo (r.getX() + r.getWidth() * 0.10f - d * 0.45f, r.getY() + d);
        glare.lineTo (r.getX() + r.getWidth() * 0.10f - d, r.getY() + d);
        glare.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX() + d * 0.2f, r.getY() + d, false));
        g.fillPath (glare);
    }
    auto glow = r.reduced (2.0f).removeFromBottom (juce::jmin (60.0f, r.getHeight() * 0.4f));
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.0f), 0, glow.getY(), accent.withAlpha (0.26f), 0, glow.getBottom(), false));
    g.fillRoundedRectangle (glow, rad - 2.0f);
    drawAeroSheen (g, r.reduced (2.5f), rad - 2.5f, 1.0f);
    // thick bevelled edge: an outer dark line, a wide bright band inside it, then a hairline
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (r.expanded (0.5f), rad + 0.5f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.95f), r.getX(), r.getY(), accent.brighter (0.4f).withAlpha (0.75f), r.getRight(), r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (1.0f), rad - 1.0f, 2.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.05f), r.getRight(), r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (3.6f), rad - 3.6f, 1.6f);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.drawRoundedRectangle (r.reduced (5.0f), rad - 5.0f, 0.8f);
    // caustic: light focused along the bottom inner edge
    g.setGradientFill (juce::ColourGradient (accent.brighter (0.6f).withAlpha (0.0f), r.getX(), 0, accent.brighter (0.6f).withAlpha (0.8f), r.getCentreX(), 0, true));
    g.drawHorizontalLine ((int) (r.getBottom() - 3.0f), r.getX() + rad, r.getRight() - rad);
}

// Glass refraction for the cached backdrop: shadow under a slab, then the scene seen through it,
// magnified and shifted the way a thick lens would.
void drawGlassSlabInScene (juce::Graphics&, juce::Rectangle<float> slab, juce::Rectangle<float> scene, float rad);   // Panels.cpp

// The scene behind everything (Display > Background): 0 = sky and nature, 1 = colour void.
inline int& backdropMode() { static int m = 0; return m; }
void paintScene (juce::Graphics&, juce::Rectangle<float> area, int mode, bool simple = false);   // Panels.cpp

// Glossy orb (knob caps, band nodes, toggle bubbles).
inline void drawOrb (juce::Graphics& g, juce::Point<float> c, float r, Colour col, bool bright = false)
{
    const auto body = juce::Rectangle<float> (2 * r, 2 * r).withCentre (c);
    g.setGradientFill (juce::ColourGradient (col.brighter (bright ? 0.7f : 0.45f), c.x, c.y - r * 0.6f, col.darker (0.55f), c.x, c.y + r, true));
    g.fillEllipse (body);
    auto hl = juce::Rectangle<float> (r * 1.3f, r * 0.8f).withCentre ({ c.x, c.y - r * 0.42f });
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.9f), hl.getCentreX(), hl.getY(), juce::Colours::white.withAlpha (0.0f), hl.getCentreX(), hl.getBottom(), false));
    g.fillEllipse (hl);
    // light caught in the bottom of the bubble
    auto lo = juce::Rectangle<float> (r * 1.1f, r * 0.45f).withCentre ({ c.x, c.y + r * 0.6f });
    g.setGradientFill (juce::ColourGradient (col.brighter (0.9f).withAlpha (0.0f), lo.getCentreX(), lo.getY(), col.brighter (0.9f).withAlpha (0.45f), lo.getCentreX(), lo.getBottom(), false));
    g.fillEllipse (lo);
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
    int palette;         // matching analyzer gradient
    int backdrop = -1;   // themes also pick the scene behind the glass (-1: leave it)
};
inline const std::array<Scheme, 6>& schemes()
{
    static const std::array<Scheme, 6> s { {
        // Dark Aero (default): purple-black glass with the gloss and aqua/lime pop of sky-and-nature Frutiger Aero
        { "Dark Aero",      0xff2a1446, 0xff07030d, 0xff0c0716, 0xff2a1d44, 0xff3d2b62, 0xff261a3a, 0xff9d8fb8, 0xffffffff, 0xffbdb2d6,
                            0xff3fd8ff, 0xffb6f24a, 0xffe6dcff, 0xff9a7bff, 0xff3b1d7a, 0xffffffff, 7 },
        { "iPod touch",     0xff2a2d33, 0xff040405, 0xff0a0c10, 0xff2b2f37, 0xff3d434e, 0xff262b33, 0xff8e96a3, 0xffffffff, 0xffa9b1bd,
                            0xff2f8bff, 0xff4cd964, 0xffd5dae1, 0xff9aaecb, 0xff4d6487, 0xffffffff, 0 },
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

// Themes: a colour scheme, scene and analyzer colours that belong together. Their indices start at
// kFirstTheme so projects saved with the older numbering (custom = 6) keep their look.
constexpr int kCustomScheme = 6, kFirstTheme = 7, kDefaultScheme = 7;   // default: Sky Aero
inline const std::vector<Scheme>& themeSchemes()
{
    static const std::vector<Scheme> t {
        { "Sky Aero",       0xff0a5bc4, 0xff06243f, 0xff071a33, 0xff12365e, 0xff1f5a8f, 0xff1f4a73, 0xffa9cbe8, 0xffffffff, 0xffcfe6f7,
                            0xff3fd8ff, 0xffb6f24a, 0xffe8f8ff, 0xff7fd0ff, 0xff1565b8, 0xffffffff, 1, 0 },
        { "Ocean",          0xff0a4a6e, 0xff021522, 0xff03192a, 0xff0c3a50, 0xff15586f, 0xff134560, 0xff98c8d8, 0xffeefcff, 0xffb7dde6,
                            0xff35f0d8, 0xffffb05a, 0xffd8fbff, 0xff4fd8e8, 0xff0b5d80, 0xffffffff, 2, 2 },
        { "Sunset Beach",   0xff5a1e5e, 0xff1a0820, 0xff1c0a22, 0xff4a1f4a, 0xff6a2f62, 0xff3d1c3d, 0xffd6a8c4, 0xfffff6ee, 0xffefc9d6,
                            0xffffa94a, 0xff5fd0ff, 0xffffe6d0, 0xffff9a6a, 0xffa8326e, 0xffffffff, 3, 3 },
        { "Aurora Night",   0xff0d1a3a, 0xff020510, 0xff040a1c, 0xff12224a, 0xff1d3468, 0xff16264a, 0xff8ea6d0, 0xfff2fbff, 0xffb4c8e6,
                            0xff4dffb0, 0xffc08cff, 0xffd8f0ff, 0xff5a7bd8, 0xff1a2a6e, 0xffffffff, 2, 4 },
        { "Meadow",         0xff2f7a2a, 0xff0b2a0c, 0xff0a1f12, 0xff1c4a2c, 0xff2c6a3e, 0xff1d4a2a, 0xffb2d8b0, 0xffffffff, 0xffd5ecd0,
                            0xffc8f54a, 0xff56c8ff, 0xfff0ffe0, 0xff8fdc5a, 0xff2a7f3a, 0xffffffff, 2, 5 },
        { "Colour Void",    0xff1d0b33, 0xff05020c, 0xff07040f, 0xff261640, 0xff3a2460, 0xff22163a, 0xffa898c8, 0xffffffff, 0xffd0c4ee,
                            0xff1fd2ff, 0xffff6ad5, 0xffeee0ff, 0xffb06bff, 0xff4a1f8a, 0xffffffff, 7, 1 } };
    return t;
}
inline int numSchemeSlots() { return kFirstTheme + (int) themeSchemes().size(); }
inline const Scheme* schemeAt (int i)
{
    if (i >= 0 && i < (int) schemes().size()) return &schemes()[(size_t) i];
    if (i >= kFirstTheme && i < numSchemeSlots()) return &themeSchemes()[(size_t) (i - kFirstTheme)];
    return nullptr;   // kCustomScheme (or unknown)
}

// Custom scheme: the user picks these; everything else is derived from them.
inline const juce::StringArray customSlotNames { "Background", "Panels", "Analyzer background", "Accent", "Second accent",
                                                 "Title bar", "Text", "Title text", "Rims / chrome" };
constexpr int kCustomSlots = 9;

inline std::array<Colour, kCustomSlots> slotsFromScheme (int index)
{
    const auto* sp = schemeAt (index);
    const auto& c = sp != nullptr ? *sp : themeSchemes()[0];
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
    const auto* sp = schemeAt (index);
    const auto& c = sp != nullptr ? *sp : themeSchemes()[0];
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
                                  scheme ("uiScheme"), units ("uiUnits"), uiFont ("uiFont"), backdrop ("uiBackdrop"), modLoudness ("modLoudness"),
                                  cmpWith ("cmpWith"), cmpSens ("cmpSens");
    inline juce::Identifier customSlot (int i) { return juce::Identifier ("uiC" + juce::String (i)); }
    inline const juce::StringArray backdropNames { "Sky and nature", "Colour void", "Ocean", "Sunset beach", "Aurora night", "Meadow" };
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
    constexpr int kDefaultPalette = 1;   // Aero Sky, matches the default Sky Aero theme
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
        { "Aero Violet",  0xffe6fbff, 0xff9b5cff, 0xff1a0633 } } };
    return p;
}

inline Palette paletteFrom (const juce::ValueTree& s)
{
    const int idx = ana::get (s, ana::palette, ana::kDefaultPalette);
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
    backdropMode() = juce::jlimit (0, ana::backdropNames.size() - 1, ana::get (st, ana::backdrop, 0));
    const int idx = ana::get (st, ana::scheme, kDefaultScheme);
    if (schemeAt (idx) != nullptr) { applyScheme (idx); return; }
    auto k = slotsFromScheme (0);
    for (int i = 0; i < kCustomSlots; ++i)
        if (st.hasProperty (ana::customSlot (i))) k[(size_t) i] = Colour ((juce::uint32) (juce::int64) st.getProperty (ana::customSlot (i)));
    applyCustom (k);
}
} // namespace theme
namespace ana = theme::ana;
