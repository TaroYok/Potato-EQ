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
} // namespace theme
