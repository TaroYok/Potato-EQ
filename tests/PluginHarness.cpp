// End-to-end check of the full plugin: processing in both modes, auto-EQ learning,
// state save/restore, and a rendered screenshot of the editor.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <cstdio>

using namespace juce;
static int failures = 0;
static void check (bool ok, const String& what, double got, double want)
{
    std::printf ("%s  %-60s got %9.3f  want %9.3f\n", ok ? "PASS" : "FAIL", what.toRawUTF8(), got, want);
    if (! ok) ++failures;
}

static void setParamFast (PotatoEQProcessor& p, const String& id, float v)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void pump (int ms) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

// Feeds a stereo signal through the processor in 512-sample blocks.
template <typename Gen>
static void run (PotatoEQProcessor& p, double seconds, Gen&& gen, std::vector<float>* outL = nullptr)
{
    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    const int total = (int) (seconds * 48000);
    for (int off = 0; off < total; off += 512)
    {
        for (int i = 0; i < 512; ++i) { const auto s = gen (off + i); buf.setSample (0, i, s.first); buf.setSample (1, i, s.second); }
        p.processBlock (buf, midi);
        if (outL) outL->insert (outL->end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
        if ((off / 512) % 40 == 0) Thread::sleep (1);   // give the worker thread air, like a real host would
    }
}

static double sineGainDb (PotatoEQProcessor& p, double f, int latency)
{
    std::vector<float> out;
    const double a = 0.1;
    run (p, 1.0 + latency / 48000.0, [&] (int n) { const float v = (float) (a * std::sin (2 * MathConstants<double>::pi * f * n / 48000)); return std::make_pair (v, v); }, &out);
    double s = 0; int cnt = 0;
    for (size_t i = out.size() / 2; i < out.size(); ++i) { s += (double) out[i] * out[i]; ++cnt; }
    return 10 * std::log10 (s / cnt) - 20 * std::log10 (a / std::sqrt (2.0));
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const String shotPath = argc > 1 ? String (argv[1]) : String ("potato-eq.png");

    auto p = std::make_unique<PotatoEQProcessor>();
    p->setPlayConfigDetails (2, 2, 48000, 512);
    p->prepareToPlay (48000, 512);

    // a few bands
    auto band = [&] (int b, arc::BandType t, float f, float g, float q, int slopeIdx = 1)
    {
        p->setParam (params::bandId (b, "type"), (float) t); p->setParam (params::bandId (b, "freq"), f);
        p->setParam (params::bandId (b, "gain"), g); p->setParam (params::bandId (b, "q"), q);
        p->setParam (params::bandId (b, "slope"), (float) slopeIdx); p->setParam (params::bandId (b, "used"), 1.0f);
    };
    band (0, arc::BandType::LowCut, 30, 0, 0.7071f, 3);
    band (1, arc::BandType::Bell, 250, -3, 1.2f);
    band (2, arc::BandType::Bell, 3000, 4, 0.9f);
    band (3, arc::BandType::HighShelf, 10000, 2.5f, 0.7f);

    std::array<arc::BandSettings, arc::kNumBands> bs;
    for (int i = 0; i < arc::kNumBands; ++i) bs[(size_t) i] = p->bandSettings (i);
    auto expected = [&] (double f) { double d = 0; for (auto& b : bs) if (b.active()) d += arc::designBand (b, b.gainDb, 48000).magnitudeDb (f, 48000); return d; };

    std::puts ("-- zero latency --");
    for (double f : { 60.0, 250.0, 3000.0, 12000.0 })
        check (std::abs (sineGainDb (*p, f, 0) - expected (f)) < 0.15, "IIR " + String (f) + " Hz", sineGainDb (*p, f, 0), expected (f));

    std::puts ("-- linear phase --");
    p->setParam (params::id::phaseMode, 1.0f);
    run (*p, 0.05, [] (int) { return std::make_pair (0.0f, 0.0f); });
    for (int i = 0; i < 40; ++i) { Thread::sleep (25); run (*p, 0.02, [] (int) { return std::make_pair (0.0f, 0.0f); }); }
    check (p->getLatencySamples() == 8192 + 71, "reported latency (FIR + limiter look-ahead)", p->getLatencySamples(), 8192 + 71);
    for (double f : { 60.0, 250.0, 3000.0, 12000.0 })
    {
        const double g = sineGainDb (*p, f, p->getLatencySamples());
        check (std::abs (g - expected (f)) < 0.4, "FIR " + String (f) + " Hz", g, expected (f));
    }
    p->setParam (params::id::phaseMode, 0.0f);

    std::puts ("-- auto EQ learn --");
    p->setParam (params::id::autoProfile, 1.0f);   // hip-hop
    p->setParam (params::id::autoMode, (float) params::autoLearn);
    Random rng (42);
    double b0 = 0, b1 = 0, b2 = 0;
    auto pinkish = [&] (int) { // pink-ish noise with a honky 500 Hz-ish bump
        const float w = rng.nextFloat() * 2 - 1;
        b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
        const float v = (float) ((b0 + b1 + b2 + w * 0.1848) * 0.08);
        return std::make_pair (v, v);
    };
    run (*p, 14.0, pinkish);
    for (int i = 0; i < 40 && ! p->autoSnapshot().result.valid; ++i) { Thread::sleep (50); run (*p, 0.05, pinkish); }
    auto snap = p->autoSnapshot();
    check (snap.result.valid, "learned a curve after ~12 s", snap.result.valid, 1);
    double maxAbs = 0; for (auto g : snap.result.curve.gainsDb) maxAbs = std::max (maxAbs, std::abs (g));
    check (maxAbs > 0.5 && maxAbs <= 12.0, "auto bells within limits (max |dB|)", maxAbs, 6);
    const double lowCorr = arc::autoCurveDb (snap.result.curve, 1.0, 60, 48000);
    check (lowCorr > 0.5, "pink noise vs hip-hop target -> boosts the low end", lowCorr, 3);

    std::puts ("-- loudness --");
    p->resetMeter();
    run (*p, 0.02, pinkish);
    run (*p, 6.0, pinkish);
    auto r = p->meter.get();
    check (r.integratedValid && r.integrated > -60 && r.integrated < 0, "meter reads a sensible integrated value", r.integrated, -20);
    const float before = p->apvts.getRawParameterValue (params::id::outGain)->load();
    auto m = p->matchLoudnessToTarget();
    const float after = p->apvts.getRawParameterValue (params::id::outGain)->load();
    check (m.ok && std::abs ((after - before) - m.appliedDb) < 0.05, "match-to-target moves output gain", after - before, m.appliedDb);
    run (*p, 0.02, pinkish); run (*p, 6.0, pinkish);
    r = p->meter.get();
    check (r.truePeakDb <= -0.7f, "true peak kept at/below -1 dBTP after matching", r.truePeakDb, -1);

    std::puts ("-- auto level + limiter --");
    {
        auto pl = std::make_unique<PotatoEQProcessor>();
        pl->setPlayConfigDetails (2, 2, 48000, 512);
        pl->prepareToPlay (48000, 512);
        pl->setParam (params::id::lufsTarget, 7.0f);      // Custom
        pl->setParam (params::id::lufsCustom, -10.0f);
        pl->setParam (params::id::limiter, 1.0f);
        pl->setParam (params::id::ceiling, -1.0f);
        pl->setParam (params::id::autoLevel, 1.0f);
        auto quiet = [&] (int n) { auto v = pinkish (n); return std::make_pair (v.first * 0.3f, v.second * 0.3f); };
        std::vector<float> out;
        run (*pl, 25.0, quiet, &out);
        pl->resetMeter(); run (*pl, 0.02, quiet); run (*pl, 6.0, quiet, &out);
        const auto rr = pl->meter.get();
        check (std::abs (rr.shortTerm + 10.0f) < 1.5f, "auto level lands a quiet track on -10 LUFS", rr.shortTerm, -10);
        float pk = 0; for (size_t i = out.size() - 48000 * 5; i < out.size(); ++i) pk = jmax (pk, std::abs (out[i]));
        check (Decibels::gainToDecibels (pk) <= -0.99f, "limiter holds the -1 dB ceiling (sample peak)", Decibels::gainToDecibels (pk), -1);
        check (pl->autoLevelDb() > 3.0f, "auto level gain went up", pl->autoLevelDb(), 10);
    }

    std::puts ("-- CPU load + automation stress --");
    {
        auto pc = std::make_unique<PotatoEQProcessor>();
        pc->setPlayConfigDetails (2, 2, 48000, 512);
        pc->prepareToPlay (48000, 512);
        // worst case: all 24 bands, steepest cuts, 8 dynamic bands, M/S bands, live auto EQ, limiter
        for (int b = 0; b < arc::kNumBands; ++b)
        {
            const auto t = b % 6 == 0 ? arc::BandType::LowCut : b % 6 == 1 ? arc::BandType::HighCut : b % 6 == 2 ? arc::BandType::LowShelf : arc::BandType::Bell;
            pc->setParam (params::bandId (b, "type"), (float) t);
            pc->setParam (params::bandId (b, "freq"), (float) (30.0 * std::pow (600.0, b / 23.0)));
            pc->setParam (params::bandId (b, "gain"), (b % 2 ? 3.0f : -3.0f));
            pc->setParam (params::bandId (b, "q"), 1.0f);
            pc->setParam (params::bandId (b, "slope"), 7.0f);   // 96 dB/oct
            pc->setParam (params::bandId (b, "place"), (float) (b % 5));
            pc->setParam (params::bandId (b, "dyn"), b % 3 == 0 ? 1.0f : 0.0f);
            pc->setParam (params::bandId (b, "used"), 1.0f);
        }
        pc->setParam (params::id::autoMode, (float) params::autoLive);
        pc->setParam (params::id::limiter, 1.0f);
        pc->setParam (params::id::autoLevel, 1.0f);
        const double secs = 20.0;
        auto t0 = Time::getMillisecondCounterHiRes();
        run (*pc, secs, pinkish);
        const double iirPct = 100.0 * (Time::getMillisecondCounterHiRes() - t0) / (secs * 1000.0);
        std::printf ("      zero latency, everything on: %.2f %% of one core in real time\n", iirPct);
        check (iirPct < 15.0, "zero-latency worst case CPU (% of one core)", iirPct, 15);

        // automation: sweep frequency, gain and Q of 8 bands every block, fast
        float maxOut = 0; bool finite = true; int block = 0;
        AudioBuffer<float> buf (2, 512); MidiBuffer midi;
        t0 = Time::getMillisecondCounterHiRes();
        for (int k = 0; k < (int) (secs * 48000 / 512); ++k, ++block)
        {
            const double ph = block * 512.0 / 48000.0;
            for (int b = 0; b < 8; ++b)
            {
                setParamFast (*pc, params::bandId (b, "freq"), (float) (20.0 * std::pow (1000.0, 0.5 + 0.5 * std::sin (ph * 6.0 + b))));
                setParamFast (*pc, params::bandId (b, "gain"), (float) (18.0 * std::sin (ph * 9.0 + b)));
                setParamFast (*pc, params::bandId (b, "q"), (float) (0.3 * std::pow (30.0, 0.5 + 0.5 * std::sin (ph * 4.0 + b))));
            }
            for (int i = 0; i < 512; ++i) { auto v = pinkish (i); buf.setSample (0, i, v.first); buf.setSample (1, i, v.second); }
            pc->processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i)
            { const float x = buf.getSample (c, i); if (! std::isfinite (x)) finite = false; maxOut = jmax (maxOut, std::abs (x)); }
        }
        const double autoPct = 100.0 * (Time::getMillisecondCounterHiRes() - t0) / (secs * 1000.0);
        std::printf ("      zero latency with 24 parameters automated every block: %.2f %% of one core\n", autoPct);
        check (finite && maxOut <= 1.0f, "fast automation: stable, no blow-ups (limiter ceiling holds)", maxOut, 1);

        pc->setParam (params::id::phaseMode, 1.0f);
        run (*pc, 1.0, pinkish);
        t0 = Time::getMillisecondCounterHiRes();
        run (*pc, secs, pinkish);
        const double firPct = 100.0 * (Time::getMillisecondCounterHiRes() - t0) / (secs * 1000.0);
        std::printf ("      linear phase, everything on: %.2f %% of one core\n", firPct);
        check (firPct < 15.0, "linear-phase worst case CPU (% of one core)", firPct, 15);
    }

    std::puts ("-- state --");
    MemoryBlock state;
    p->getStateInformation (state);
    auto q = std::make_unique<PotatoEQProcessor>();
    q->setPlayConfigDetails (2, 2, 48000, 512);
    q->setStateInformation (state.getData(), (int) state.getSize());
    q->prepareToPlay (48000, 512);
    check (q->bandSettings (2).used && std::abs (q->bandSettings (2).gainDb - 4) < 0.01, "bands restored", q->bandSettings (2).gainDb, 4);
    for (int i = 0; i < 40 && ! q->autoSnapshot().result.valid; ++i) Thread::sleep (50);
    check (q->autoSnapshot().result.valid, "learned spectrum restored with the project", q->autoSnapshot().result.valid, 1);

    std::puts ("-- commit auto to bands --");
    const int placed = p->commitAutoToBands();
    check (placed >= 2 && placed <= 8, "auto curve turned into editable bands", placed, 6);
    check ((int) p->apvts.getRawParameterValue (params::id::autoMode)->load() == params::autoOff, "auto EQ switched off after commit", 0, 0);

    std::puts ("-- factory presets --");
    {
        auto pp = std::make_unique<PotatoEQProcessor>();
        pp->setPlayConfigDetails (2, 2, 48000, 512);
        pp->prepareToPlay (48000, 512);
        bool allOk = true;
        for (int i = 0; i < pp->getNumPrograms(); ++i)
        {
            pp->setCurrentProgram (i);
            int used = 0; for (int b = 0; b < arc::kNumBands; ++b) used += pp->bandSettings (b).used ? 1 : 0;
            const double g = sineGainDb (*pp, 1000, 0);
            if (used != (int) presets::all()[(size_t) i].bands.size() || ! std::isfinite (g)) allOk = false;
        }
        check (allOk, String (pp->getNumPrograms()) + " presets load the right bands and process cleanly", allOk, 1);
        int radio = -1; for (int i = 0; i < pp->getNumPrograms(); ++i) if (pp->getProgramName (i).endsWith ("Radio")) radio = i;
        pp->setCurrentProgram (radio);
        const double lo = sineGainDb (*pp, 100, 0), mid = sineGainDb (*pp, 1600, 0);
        check (lo < -15 && mid > 3, "Radio: lows cut hard, mids boosted (100 Hz dB)", lo, -20);
        pp->setCurrentProgram (0);
        int used = 0; for (int b = 0; b < arc::kNumBands; ++b) used += pp->bandSettings (b).used ? 1 : 0;
        check (used == 0, "Default clears all bands", used, 0);
    }

    std::puts ("-- note names --");
    check (theme::noteName (55.0) == "A1", "55 Hz is " + theme::noteName (55.0), 1, 1);
    check (theme::noteName (261.63) == "C4", "261.63 Hz is " + theme::noteName (261.63), 1, 1);
    check (theme::noteName (45.0) == "F#1 -47c", "45 Hz is " + theme::noteName (45.0), 1, 1);

    std::puts ("-- editor --");
    {
        p->apvts.state.setProperty (ana::peakHold, 1, nullptr);
        p->apvts.state.setProperty (ana::resolution, 3, nullptr);   // 16k FFT
        p->setParam (params::id::autoMode, (float) params::autoLearn);
        std::unique_ptr<AudioProcessorEditor> ed (p->createEditor());
        ed->setSize (1360, 860);
        for (int i = 0; i < 30; ++i) { run (*p, 0.05, pinkish); pump (30); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        {
            const auto t0 = Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 20; ++i) ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            const double ms = (Time::getMillisecondCounterHiRes() - t0) / 20.0;
            std::printf ("      full editor repaint: %.1f ms\n", ms);
            for (auto* c : ed->getChildren())
                if (auto* d = dynamic_cast<EqDisplay*> (c))
                {
                    const auto t1 = Time::getMillisecondCounterHiRes();
                    for (int i = 0; i < 20; ++i) d->createComponentSnapshot (d->getLocalBounds(), true, 1.0f);
                    std::printf ("      EQ display repaint (30x per second): %.1f ms\n", (Time::getMillisecondCounterHiRes() - t1) / 20.0);
                }
        }
        File f (shotPath);
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
        check (f.existsAsFile(), "editor rendered to " + f.getFileName(), 1, 1);
        // spectrum grab: a 440 Hz tone in the analyzer; clicking its peak and dragging up should create a band there
        EqDisplay* disp = nullptr;
        for (auto* c : ed->getChildren()) if (auto* d = dynamic_cast<EqDisplay*> (c)) disp = d;
        p->setParam (params::id::autoMode, (float) params::autoOff);
        for (int b = 0; b < arc::kNumBands; ++b) p->setParam (params::bandId (b, "used"), 0.0f);
        p->apvts.state.setProperty (ana::peakHold, 0, nullptr);
        long toneN = 0;
        auto tone = [&] (int) { const long n = toneN++; const float v = (float) (0.25 * std::sin (2 * MathConstants<double>::pi * 440.0 * (double) n / 48000)) + pinkish (n).first * 0.05f; return std::make_pair (v, v); };
        for (int i = 0; i < 20; ++i) { run (*p, 0.05, tone); pump (30); }
        auto src = Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (Point<float> pt, Point<float> down, bool dragged)
        {
            return MouseEvent (src, pt, ModifierKeys (ModifierKeys::leftButtonModifier), 1.0f, 0, 0, 0, 0, disp, disp,
                               Time::getCurrentTime(), down, Time::getCurrentTime(), 1, dragged);
        };
        int grabbed = -1;
        for (int y = 30; y < disp->getHeight() - 30 && grabbed < 0; y += 8)
        {
            for (float x = 380; x < 560 && grabbed < 0; x += 4)
            {
                const Point<float> pt (x, (float) y);
                disp->mouseMove (ev (pt, pt, false));
                disp->mouseDown (ev (pt, pt, false));
                for (int b = 0; b < arc::kNumBands; ++b) if (p->bandSettings (b).used) grabbed = b;
                if (grabbed >= 0)
                {
                    disp->mouseDrag (ev (pt.translated (0, -60), pt, true));
                    disp->mouseUp (ev (pt.translated (0, -60), pt, true));
                }
            }
        }
        check (grabbed >= 0, "spectrum grab created a band from the peak", grabbed >= 0, 1);
        if (grabbed >= 0)
        {
            const auto gb = p->bandSettings (grabbed);
            check (std::abs (12.0 * std::log2 (gb.freq / 440.0)) < 1.0, "grabbed band sits on the 440 Hz tone (" + theme::noteName (gb.freq) + ")", gb.freq, 440);
            check (gb.gainDb > 2.0, "dragging up boosted it", gb.gainDb, 6);
        }
        p->editorBeingDeleted (ed.get());
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures, failures == 1 ? "" : "s");
    p->releaseResources();
    return failures ? 1 : 0;
}
