#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
using juce::Colour;
inline const Colour bg        { 0xff15171c };
inline const Colour panel     { 0xff1d2027 };
inline const Colour panelHi   { 0xff262a33 };
inline const Colour grid      { 0xff2b2f38 };
inline const Colour gridText  { 0xff6d7482 };
inline const Colour text      { 0xffd9dde5 };
inline const Colour textDim   { 0xff8d94a3 };
inline const Colour accent    { 0xffe8b04b };   // potato gold
inline const Colour autoCol   { 0xffff7a45 };
inline const Colour good      { 0xff59d18a };
inline const Colour warn      { 0xffffc857 };
inline const Colour bad       { 0xffff5a5f };
inline const Colour preSpec   { 0xff3a6ea5 };
inline const Colour postSpec  { 0xff7fb3e6 };
inline const Colour scSpec    { 0xffc77dff };

inline Colour bandColour (int band)
{
    static const juce::uint32 c[] = { 0xffff6b6b, 0xffffa94d, 0xffffd43b, 0xff94d82d, 0xff38d9a9, 0xff3bc9db,
                                      0xff4dabf7, 0xff748ffc, 0xff9775fa, 0xffda77f2, 0xfff783ac, 0xffced4da };
    return Colour (c[band % 12]);
}

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
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

// Analyzer display settings live as properties on the plugin state (saved with the project).
namespace ana
{
    inline const juce::Identifier freeze ("anaFreeze"), peakHold ("anaPeakHold"), resolution ("anaRes"),
                                  speed ("anaSpeed"), tilt ("anaTilt"), range ("anaRange"),
                                  showSc ("anaShowSc"), collide ("anaCollide"), smoothing ("anaSmooth"), noteGrid ("anaNoteGrid");
    inline const juce::StringArray smoothingNames { "Off (1/48 oct)", "Light (1/24 oct)", "Medium (1/12 oct)", "Heavy (1/6 oct)", "Very heavy (1/3 oct)" };
    inline const juce::StringArray resolutionNames { "Low", "Medium", "High", "Maximum" };   // FFT 2k/4k/8k/16k
    inline const juce::StringArray speedNames { "Slow", "Medium", "Fast" };
    inline const juce::StringArray tiltNames { "0 dB/oct", "3 dB/oct", "4.5 dB/oct", "6 dB/oct" };
    inline const juce::StringArray rangeNames { "60 dB", "90 dB", "120 dB" };
    inline const double tiltValues[] = { 0.0, 3.0, 4.5, 6.0 };
    inline const double rangeValues[] = { 60.0, 90.0, 120.0 };
    inline int get (const juce::ValueTree& s, const juce::Identifier& id, int def) { return (int) s.getProperty (id, def); }
}
} // namespace theme
namespace ana = theme::ana;
