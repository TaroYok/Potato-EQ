#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/AutoEq.h"

namespace params
{
inline juce::String bandId (int band, const char* what) { return "b" + juce::String (band + 1) + "_" + what; }

namespace id
{
    inline const juce::String outGain = "outGain", gainScale = "gainScale", autoGain = "autoGain", phaseMode = "phaseMode",
                              analyzer = "analyzer", range = "displayRange", autoProfile = "autoProfile", autoMode = "autoMode",
                              autoAmount = "autoAmount", lufsTarget = "lufsTarget",
                              autoMaxBoost = "autoMaxBoost", autoMaxCut = "autoMaxCut", autoTilt = "autoTilt", autoBass = "autoBass",
                              autoSmooth = "autoSmooth", autoLevel = "autoLevel", limiter = "limiter", ceiling = "ceiling", lufsCustom = "lufsCustom";
}

enum AutoMode { autoOff = 0, autoLearn, autoLive };

struct LufsTarget { const char* name; float lufs; };   // lufs > 0 means "use the Custom value"
inline const std::array<LufsTarget, 8>& lufsTargets()
{
    static const std::array<LufsTarget, 8> t { {
        { "Spotify / YouTube  -14", -14.0f }, { "Apple Music  -16", -16.0f }, { "SoundCloud / Club  -9", -9.0f },
        { "Loud master  -8", -8.0f }, { "Hip-hop / EDM  -11", -11.0f }, { "Broadcast EBU  -23", -23.0f }, { "Podcast  -16", -16.0f },
        { "Custom", 1.0f } } };
    return t;
}

inline juce::NormalisableRange<float> logRange (float lo, float hi, float interval = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi,
        [] (float a, float b, float x) { return a * std::pow (b / a, x); },
        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
        [interval] (float a, float b, float v) { v = juce::jlimit (a, b, v); return interval > 0 ? std::round (v / interval) * interval : v; });
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pid = [] (const String& s) { return ParameterID { s, 1 }; };
    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });

    l.add (std::make_unique<AudioParameterFloat> (pid (id::outGain), "Output Gain", NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (pid (id::gainScale), "Gain Scale", NormalisableRange<float> (0.f, 200.f, 0.1f), 100.f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterBool> (pid (id::autoGain), "Auto Gain", false));
    l.add (std::make_unique<AudioParameterChoice> (pid (id::phaseMode), "Processing", StringArray { "Zero Latency", "Linear Phase" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (pid (id::analyzer), "Analyzer", StringArray { "Off", "Pre", "Post", "Pre + Post" }, 3));
    l.add (std::make_unique<AudioParameterChoice> (pid (id::range), "Display Range", StringArray { "3 dB", "6 dB", "12 dB", "30 dB" }, 2));

    StringArray profNames;
    for (auto& p : arc::profiles()) profNames.add (p.name);
    l.add (std::make_unique<AudioParameterChoice> (pid (id::autoProfile), "Auto EQ Profile", profNames, 0));
    l.add (std::make_unique<AudioParameterChoice> (pid (id::autoMode), "Auto EQ Mode", StringArray { "Off", "Learn", "Live" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (pid (id::autoAmount), "Auto EQ Amount", NormalisableRange<float> (0.f, 150.f, 0.1f), 100.f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    StringArray targets;
    for (auto& t : lufsTargets()) targets.add (t.name);
    l.add (std::make_unique<AudioParameterChoice> (pid (id::lufsTarget), "Loudness Target", targets, 0));

    l.add (std::make_unique<AudioParameterFloat> (pid (id::autoMaxBoost), "Auto EQ Max Boost", NormalisableRange<float> (0.f, 12.f, 0.1f), 5.f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (pid (id::autoMaxCut), "Auto EQ Max Cut", NormalisableRange<float> (0.f, 12.f, 0.1f), 8.f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (pid (id::autoTilt), "Auto EQ Brightness", NormalisableRange<float> (-3.f, 3.f, 0.05f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB/oct"; })));
    l.add (std::make_unique<AudioParameterFloat> (pid (id::autoBass), "Auto EQ Low End", NormalisableRange<float> (-6.f, 6.f, 0.1f), 0.f, dbAttr));
    l.add (std::make_unique<AudioParameterChoice> (pid (id::autoSmooth), "Auto EQ Detail", StringArray { "Broad", "Medium", "Fine" }, 1));
    l.add (std::make_unique<AudioParameterBool> (pid (id::autoLevel), "Auto Level", false));
    l.add (std::make_unique<AudioParameterBool> (pid (id::limiter), "Limiter", false));
    l.add (std::make_unique<AudioParameterFloat> (pid (id::ceiling), "Limiter Ceiling", NormalisableRange<float> (-6.f, 0.f, 0.1f), -1.f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterFloat> (pid (id::lufsCustom), "Custom Loudness Target", NormalisableRange<float> (-30.f, -5.f, 0.1f), -14.f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " LUFS"; })));

    StringArray types, slopes, places;
    for (int i = 0; i < (int) arc::BandType::Count; ++i) types.add (arc::bandTypeName ((arc::BandType) i));
    for (int s : arc::kSlopes) slopes.add (String (s) + " dB/oct");
    for (int i = 0; i < (int) arc::Placement::Count; ++i) places.add (arc::placementName ((arc::Placement) i));

    for (int b = 0; b < arc::kNumBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (b + 1), "Band " + String (b + 1), "|");
        group->addChild (std::make_unique<AudioParameterBool> (pid (bandId (b, "used")), n + "Used", false));
        group->addChild (std::make_unique<AudioParameterBool> (pid (bandId (b, "bypass")), n + "Bypass", false));
        group->addChild (std::make_unique<AudioParameterChoice> (pid (bandId (b, "type")), n + "Type", types, 0));
        group->addChild (std::make_unique<AudioParameterFloat> (pid (bandId (b, "freq")), n + "Freq", logRange (10.f, 30000.f), 1000.f,
            AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
                { return v >= 1000 ? String (v / 1000.f, 2) + " kHz" : String (v, v < 100 ? 1 : 0) + " Hz"; })));
        group->addChild (std::make_unique<AudioParameterFloat> (pid (bandId (b, "gain")), n + "Gain", NormalisableRange<float> (-30.f, 30.f, 0.01f), 0.f, dbAttr));
        group->addChild (std::make_unique<AudioParameterFloat> (pid (bandId (b, "q")), n + "Q", logRange (0.025f, 40.f), 1.f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10 ? 2 : 1); })));
        group->addChild (std::make_unique<AudioParameterChoice> (pid (bandId (b, "slope")), n + "Slope", slopes, 3));
        group->addChild (std::make_unique<AudioParameterChoice> (pid (bandId (b, "place")), n + "Stereo", places, 0));
        group->addChild (std::make_unique<AudioParameterBool> (pid (bandId (b, "dyn")), n + "Dynamic", false));
        group->addChild (std::make_unique<AudioParameterFloat> (pid (bandId (b, "thr")), n + "Threshold", NormalisableRange<float> (-80.f, 0.f, 0.1f), -30.f, dbAttr));
        group->addChild (std::make_unique<AudioParameterFloat> (pid (bandId (b, "range")), n + "Range", NormalisableRange<float> (-24.f, 24.f, 0.1f), -6.f, dbAttr));
        group->addChild (std::make_unique<AudioParameterBool> (pid (bandId (b, "sc")), n + "Sidechain", false));
        l.add (std::move (group));
    }
    return l;
}

// Fast read access to one band's parameters (cached raw pointers).
struct BandParams
{
    std::atomic<float>* used {}; std::atomic<float>* bypass {}; std::atomic<float>* type {}; std::atomic<float>* freq {};
    std::atomic<float>* gain {}; std::atomic<float>* q {}; std::atomic<float>* slope {}; std::atomic<float>* place {};
    std::atomic<float>* dyn {}; std::atomic<float>* thr {}; std::atomic<float>* range {}; std::atomic<float>* sc {};

    void attach (juce::AudioProcessorValueTreeState& s, int b)
    {
        used = s.getRawParameterValue (bandId (b, "used")); bypass = s.getRawParameterValue (bandId (b, "bypass"));
        type = s.getRawParameterValue (bandId (b, "type")); freq = s.getRawParameterValue (bandId (b, "freq"));
        gain = s.getRawParameterValue (bandId (b, "gain")); q = s.getRawParameterValue (bandId (b, "q"));
        slope = s.getRawParameterValue (bandId (b, "slope")); place = s.getRawParameterValue (bandId (b, "place"));
        dyn = s.getRawParameterValue (bandId (b, "dyn")); thr = s.getRawParameterValue (bandId (b, "thr"));
        range = s.getRawParameterValue (bandId (b, "range")); sc = s.getRawParameterValue (bandId (b, "sc"));
    }

    arc::BandSettings read() const
    {
        arc::BandSettings b;
        b.used = used->load() > 0.5f; b.bypass = bypass->load() > 0.5f;
        b.type = (arc::BandType) juce::jlimit (0, (int) arc::BandType::Count - 1, (int) type->load());
        b.freq = freq->load(); b.gainDb = gain->load(); b.q = q->load();
        b.slope = arc::kSlopes[(size_t) juce::jlimit (0, (int) arc::kSlopes.size() - 1, (int) slope->load())];
        b.place = (arc::Placement) juce::jlimit (0, (int) arc::Placement::Count - 1, (int) place->load());
        b.dynamic = dyn->load() > 0.5f; b.thresholdDb = thr->load(); b.rangeDb = range->load(); b.sidechain = sc->load() > 0.5f;
        return b;
    }
};

inline int slopeIndex (int slope)
{
    for (int i = 0; i < (int) arc::kSlopes.size(); ++i) if (arc::kSlopes[(size_t) i] >= slope) return i;
    return (int) arc::kSlopes.size() - 1;
}
} // namespace params
