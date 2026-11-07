#include "EqDisplay.h"
#include <algorithm>

using namespace juce;

static constexpr double kLoHz = 20.0, kHiHz = 20000.0;
static constexpr int kColStep = 2;

EqDisplay::EqDisplay (PotatoEQProcessor& p) : proc (p)
{
    for (auto* s : { &pre, &post, &side }) s->ring.assign (kMaxSize, 0.0f);
    fftData.assign (2 * kMaxSize, 0.0f);
    setOpaque (false);   // rounded corners
    startTimerHz (30);
}

EqDisplay::~EqDisplay() { stopTimer(); }

Rectangle<float> EqDisplay::plotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 6.0f).withTrimmedRight (26.0f).withTrimmedBottom (14.0f); }
float EqDisplay::xForFreq (double f) const { auto a = plotArea(); return a.getX() + a.getWidth() * (float) (std::log (f / kLoHz) / std::log (kHiHz / kLoHz)); }
double EqDisplay::freqForX (float x) const { auto a = plotArea(); return kLoHz * std::pow (kHiHz / kLoHz, (double) ((x - a.getX()) / a.getWidth())); }
double EqDisplay::displayRange() const
{
    static const double r[] = { 3, 6, 12, 30 };
    return r[jlimit (0, 3, (int) proc.apvts.getRawParameterValue (params::id::range)->load())];
}
float EqDisplay::yForGain (double db) const { auto a = plotArea(); return a.getCentreY() - (float) (db / displayRange()) * (a.getHeight() * 0.46f); }
double EqDisplay::gainForY (float y) const { auto a = plotArea(); return (a.getCentreY() - y) / (a.getHeight() * 0.46f) * displayRange(); }
float EqDisplay::yForSpectrum (double db) const
{
    const double range = ana::rangeValues[jlimit (0, 2, setting (ana::range, 1))];
    auto a = plotArea();
    return a.getY() + a.getHeight() * (float) jlimit (0.0, 1.0, (6.0 - db) / (range + 6.0));
}

void EqDisplay::resized()
{
    const int cols = jmax (1, (int) plotArea().getWidth() / kColStep + 1);
    for (auto* s : { &pre, &post, &side }) { s->display.assign ((size_t) cols, -150.0f); s->peak.assign ((size_t) cols, -150.0f); }
}

void EqDisplay::clearPeakHold()
{
    for (auto* s : { &pre, &post }) std::fill (s->peak.begin(), s->peak.end(), -150.0f);
}

void EqDisplay::pullSpectrum (SampleFifo& fifo, Spectrum& s, bool analyse)
{
    float tmp[4096];
    int got;
    while ((got = fifo.pull (tmp, 4096)) > 0)
        for (int i = 0; i < got; ++i) { s.ring[(size_t) s.pos] = tmp[i]; s.pos = (s.pos + 1) % kMaxSize; }
    if (! analyse || s.display.empty()) return;

    const int order = fftOrder, size = 1 << order;
    if (ffts[(size_t) order] == nullptr) ffts[(size_t) order] = std::make_unique<dsp::FFT> (order);
    if ((int) window.size() != size)
    {
        window.resize ((size_t) size);
        for (int i = 0; i < size; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) size);
    }
    for (int i = 0; i < size; ++i) fftData[(size_t) i] = s.ring[(size_t) ((s.pos - size + i + kMaxSize) % kMaxSize)] * window[(size_t) i];
    std::fill (fftData.begin() + size, fftData.begin() + 2 * size, 0.0f);
    ffts[(size_t) order]->performFrequencyOnlyForwardTransform (fftData.data(), true);

    const double fs = proc.getFs(), binHz = fs / size;
    s.bins.assign (fftData.begin(), fftData.begin() + size / 2 + 1);
    s.binHz = binHz;
    const double tilt = ana::tiltValues[jlimit (0, 3, setting (ana::tilt, 2))];
    static const float release[] = { 0.05f, 0.12f, 0.3f }, attack[] = { 0.45f, 0.6f, 0.9f };
    const int sp = jlimit (0, 2, setting (ana::speed, 1));
    const bool hold = setting (ana::peakHold, 0) != 0;
    // Fractional-octave smoothing: average the *power* over a band around each column
    // (prefix sums make this O(1) per column). Averaging power instead of picking single
    // bins removes the spiky look at the low end, where there are fewer bins than pixels.
    static const double widthOct[] = { 1.0 / 48.0, 1.0 / 24.0, 1.0 / 12.0, 1.0 / 6.0, 1.0 / 3.0 };
    const double width = widthOct[jlimit (0, 4, setting (ana::smoothing, 2))];
    const int nb = size / 2 + 1;
    cum.resize ((size_t) nb + 1);
    cum[0] = 0.0;
    const double norm = 4.0 / size;
    for (int k = 0; k < nb; ++k) { const double amp = fftData[(size_t) k] * norm; cum[(size_t) k + 1] = cum[(size_t) k] + (k < 2 ? 0.0 : amp * amp); }
    auto energyUpTo = [&] (double x)   // integral of bin power from 0 to x (bin k covers [k-0.5, k+0.5])
    {
        const double p = jlimit (0.0, (double) nb, x + 0.5);
        const int i = jmin (nb - 1, (int) p);
        return cum[(size_t) i] + (p - i) * (cum[(size_t) i + 1] - cum[(size_t) i]);
    };
    const auto a = plotArea();
    for (size_t c = 0; c < s.display.size(); ++c)
    {
        const float x0 = a.getX() + (float) c * kColStep;
        const double f0 = freqForX (x0), f1 = freqForX (x0 + kColStep);
        const double fcol = std::sqrt (f0 * f1);
        double lo = fcol * std::pow (2.0, -width / 2) / binHz, hi = fcol * std::pow (2.0, width / 2) / binHz;
        if (hi - lo < 2.0) { const double mid = 0.5 * (lo + hi); lo = mid - 1.0; hi = mid + 1.0; }   // at least the window's main lobe
        lo = jmax (1.5, lo);
        const double power = hi > lo ? (energyUpTo (hi) - energyUpTo (lo)) / (hi - lo) : 0.0;
        const float m = (float) std::sqrt (jmax (0.0, power)) / (float) norm;   // back to the FFT magnitude scale used below

        const double db = Decibels::gainToDecibels (m * 4.0f / (float) size, -150.0f) + tilt * std::log2 (fcol / 1000.0);
        auto& d = s.display[c];
        d = (float) (db > d ? d + (db - d) * attack[sp] : d + (db - d) * release[sp]);
        s.peak[c] = hold ? jmax (s.peak[c], d) : -150.0f;
    }
}

void EqDisplay::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue (params::id::analyzer)->load();
    const bool frozen = setting (ana::freeze, 0) != 0 || grabbing;
    const int wantOrder = 11 + jlimit (0, 3, setting (ana::resolution, 2));
    if (wantOrder != fftOrder) fftOrder = wantOrder;
    pullSpectrum (proc.preFifo, pre, ! frozen && (mode == 1 || mode == 3));
    pullSpectrum (proc.postFifo, post, ! frozen && (mode == 2 || mode == 3));
    pullSpectrum (proc.scFifo, side, ! frozen && mode != 0 && proc.sidechainActive() && setting (ana::showSc, 1) != 0);
    if (setting (ana::view, 0) == 1 && ! frozen)
    {
        const Spectrum* src = (mode == 2 || mode == 3) ? &post : (mode == 1 ? &pre : nullptr);
        if (src != nullptr) updateWaterfall (*src);
    }
    for (int b = 0; b < arc::kNumBands; ++b) bands[(size_t) b] = proc.bandSettings (b);
    if (mouseInside && dragging < 0 && hovered < 0) findPeakNear (mousePos);
    repaint();
}

const EqDisplay::Spectrum* EqDisplay::grabSource() const
{
    const int mode = (int) proc.apvts.getRawParameterValue (params::id::analyzer)->load();
    if (mode == 1) return &pre;
    if (mode == 2 || mode == 3) return &post;
    return nullptr;
}

// Finds a clear spectral peak close to the mouse (for spectrum grab).
void EqDisplay::findPeakNear (Point<float> pt)
{
    hoverPeak = {};
    const auto* s = grabSource();
    if (s == nullptr || s->display.empty()) return;
    const auto a = plotArea();
    const int n = (int) s->display.size();
    const int centre = (int) ((pt.x - a.getX()) / kColStep);
    int best = -1; float bestDb = -1e9f;
    for (int c = jmax (3, centre - 8); c <= jmin (n - 4, centre + 8); ++c)
    {
        const float v = s->display[(size_t) c];
        bool isMax = true; float lo = v;
        for (int d = -3; d <= 3 && isMax; ++d) if (d != 0 && s->display[(size_t) (c + d)] > v) isMax = false;
        if (! isMax) continue;
        for (int d = -12; d <= 12; ++d) { const int j = c + d; if (j >= 0 && j < n) lo = jmin (lo, s->display[(size_t) j]); }
        if (v - lo < 5.0f || v < -110.0f) continue;   // needs some prominence to count as a peak
        if (v > bestDb) { bestDb = v; best = c; }
    }
    if (best < 0) return;
    float x = a.getX() + (float) best * kColStep + kColStep * 0.5f;
    // refine on the raw FFT bins: strongest bin near the column, then parabolic interpolation (in dB)
    if (s->bins.size() > 8)
    {
        const double approx = freqForX (x);
        const int k0 = (int) std::lround (approx / s->binHz);
        const int span = jmax (2, (int) std::ceil (approx * 0.06 / s->binHz));
        int k = -1; float kv = 0;
        for (int j = jmax (1, k0 - span); j <= jmin ((int) s->bins.size() - 2, k0 + span); ++j)
            if (s->bins[(size_t) j] > kv) { kv = s->bins[(size_t) j]; k = j; }
        if (k > 0)
        {
            const float l = Decibels::gainToDecibels (s->bins[(size_t) k - 1], -200.0f), c0 = Decibels::gainToDecibels (kv, -200.0f),
                        rr = Decibels::gainToDecibels (s->bins[(size_t) k + 1], -200.0f);
            const float den = l - 2 * c0 + rr;
            const float off = std::abs (den) > 1e-6f ? jlimit (-0.5f, 0.5f, 0.5f * (l - rr) / den) : 0.0f;
            const double f = ((double) k + off) * s->binHz;
            if (f > kLoHz && f < kHiHz) x = xForFreq (f);
        }
    }
    const float y = yForSpectrum (bestDb);
    if (std::abs (y - pt.y) > 45.0f) return;
    hoverPeak.valid = true;
    hoverPeak.freq = freqForX (x);
    hoverPeak.db = bestDb;
    hoverPeak.pt = { x, y };
}

Point<float> EqDisplay::nodePosition (const arc::BandSettings& b) const
{
    const double fs = proc.getFs();
    const double scale = proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0;
    double y;
    if (b.type == arc::BandType::TiltShelf) y = 0.0;
    else if (arc::typeHasGain (b.type)) y = b.gainDb * scale;
    else y = jlimit (-displayRange(), displayRange(), arc::designBand (b, 0, fs).magnitudeDb (b.freq, fs));
    return { xForFreq (b.freq), jlimit (plotArea().getY() + 4, plotArea().getBottom() - 4, yForGain (y)) };
}

static bool sameBand (const arc::BandSettings& a, const arc::BandSettings& b)
{
    return a.type == b.type && a.freq == b.freq && a.q == b.q && a.slope == b.slope;
}

const std::vector<float>& EqDisplay::bandCurve (int band, const arc::BandSettings& s, double eff, int cols)
{
    auto& c = curveCache[(size_t) band];
    const double fs = proc.getFs();
    const auto a = plotArea();
    if ((int) c.db.size() != cols || c.fs != fs || c.width != a.getWidth() || ! sameBand (c.key, s) || std::abs (c.eff - eff) > 1e-4)
    {
        c.db.resize ((size_t) cols);
        const auto set = arc::designBand (s, eff, fs);
        for (int i = 0; i < cols; ++i) c.db[(size_t) i] = (float) set.magnitudeDb (freqForX (a.getX() + (float) i * 3), fs);
        c.key = s; c.eff = eff; c.fs = fs; c.width = a.getWidth();
    }
    return c.db;
}

void EqDisplay::paint (Graphics& g)
{
    // rounded glass plot
    const auto outer = getLocalBounds().toFloat();
    Path clip; clip.addRoundedRectangle (outer, theme::radius);
    g.reduceClipRegion (clip);
    g.setGradientFill (ColourGradient (theme::plotBg.brighter (0.25f), 0, outer.getY(), theme::plotBg.darker (0.3f), 0, outer.getBottom(), false));
    g.fillRect (outer);
    const auto a = plotArea();
    const double fs = proc.getFs();
    const double range = displayRange();
    const double scale = proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0;

    // grid
    g.setFont (theme::font (10.5f));
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0,
                      1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0, 10000.0, 20000.0 })
    {
        const int fi = (int) f;
        const bool major = fi == 20 || fi == 50 || fi == 100 || fi == 200 || fi == 500 || fi == 1000 || fi == 2000 || fi == 5000 || fi == 10000 || fi == 20000;
        const float x = xForFreq (f);
        g.setColour (theme::grid.withAlpha (major ? 1.0f : 0.45f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        if (major)
        {
            g.setColour (theme::gridText);
            g.drawText (fi >= 1000 ? String (fi / 1000) + "k" : String (fi), Rectangle<float> (x - 20, a.getBottom() + 1, 40, 12), Justification::centred);
        }
    }
    const double step = range <= 3 ? 1 : range <= 6 ? 2 : range <= 12 ? 3 : 6;
    for (int k = -(int) std::round (range / step); k <= (int) std::round (range / step); ++k)
    {
        const double d = k * step;
        const float y = yForGain (d);
        g.setColour (k == 0 ? theme::grid.brighter (0.25f) : theme::grid);
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (theme::gridText);
        g.drawText ((d > 0 ? "+" : "") + String ((int) d), Rectangle<float> (a.getRight() + 2, y - 6, 24, 12), Justification::centredLeft);
    }

    // note grid: every C, labelled just above the frequency scale
    if (setting (ana::noteGrid, 1) != 0)
    {
        g.setFont (theme::font (9.5f));
        for (int oct = 0; oct <= 9; ++oct)
        {
            const double f = 16.351597831287414 * std::pow (2.0, oct);   // C0 * 2^oct
            if (f < kLoHz || f > kHiHz) continue;
            const float x = xForFreq (f);
            g.setColour (theme::accent.withAlpha (0.10f));
            g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            g.setColour (theme::accent.withAlpha (0.55f));
            g.drawText ("C" + String (oct), Rectangle<float> (x + 2, a.getBottom() - 13, 24, 12), Justification::centredLeft);
        }
    }

    // spectrum
    const int mode = (int) proc.apvts.getRawParameterValue (params::id::analyzer)->load();
    const bool hold = setting (ana::peakHold, 0) != 0;
    auto specPath = [&] (const std::vector<float>& d, bool closed)
    {
        Path p;
        if (closed) p.startNewSubPath (a.getX(), a.getBottom());
        // quadratic curves through the midpoints between columns: smooth, no corners
        Point<float> prev;
        for (size_t i = 0; i < d.size(); ++i)
        {
            const Point<float> pt (a.getX() + (float) i * kColStep, yForSpectrum (d[i]));
            if (i == 0) { if (closed) p.lineTo (pt); else p.startNewSubPath (pt); }
            else p.quadraticTo (prev, (prev + pt) * 0.5f);
            prev = pt;
        }
        if (! d.empty()) p.lineTo (prev);
        if (closed) { p.lineTo (a.getRight(), a.getBottom()); p.closeSubPath(); }
        return p;
    };
    auto drawSpec = [&] (const Spectrum& s, Colour c, bool fill)
    {
        auto p = specPath (s.display, true);
        if (fill) { g.setGradientFill (ColourGradient (c.withAlpha (0.35f), 0, a.getY(), c.withAlpha (0.05f), 0, a.getBottom(), false)); g.fillPath (p); }
        g.setColour (c.withAlpha (fill ? 0.5f : 0.85f));
        g.strokePath (p, PathStrokeType (1.0f));
        if (hold)
        {
            g.setColour (c.brighter (0.6f).withAlpha (0.7f));
            g.strokePath (specPath (s.peak, false), PathStrokeType (1.0f));
        }
    };
    const auto pal = theme::paletteFrom (proc.apvts.state);
    const int view = setting (ana::view, 0), style = setting (ana::fill, 1);
    const Spectrum* main = (mode == 2 || mode == 3) ? &post : (mode == 1 ? &pre : nullptr);
    if (main != nullptr)
    {
        if (view == 1 && waterfall.isValid())
            g.drawImage (waterfall, a, RectanglePlacement::stretchToFit);
        else
        {
            if (mode == 3) { g.setColour (Colours::white.withAlpha (0.28f)); g.strokePath (specPath (pre.display, false), PathStrokeType (1.0f)); }
            drawStyledSpectrum (g, *main, pal, style);
            if (hold) { g.setColour (pal.light.withAlpha (0.75f)); g.strokePath (specPath (main->peak, false), PathStrokeType (1.0f)); }
        }
    }
    (void) drawSpec;
    if (mode != 0 && proc.sidechainActive() && setting (ana::showSc, 1) != 0 && ! side.display.empty())
    {
        g.setColour (theme::scSpec.withAlpha (0.85f));
        g.strokePath (specPath (side.display, false), PathStrokeType (1.2f));
        // collisions: frequencies where this track and the sidechain track are both strong
        const auto* own = grabSource();
        if (own != nullptr && setting (ana::collide, 1) != 0 && own->display.size() == side.display.size())
        {
            const float topA = *std::max_element (own->display.begin(), own->display.end());
            const float topB = *std::max_element (side.display.begin(), side.display.end());
            for (size_t c = 0; c < side.display.size(); ++c)
            {
                const float ea = own->display[c] - (topA - 24.0f), eb = side.display[c] - (topB - 24.0f);
                if (ea <= 0 || eb <= 0) continue;
                const float k = jlimit (0.0f, 1.0f, jmin (ea, eb) / 12.0f);
                const float x = a.getX() + (float) c * kColStep;
                const float y = yForSpectrum (jmin (own->display[c], side.display[c]));
                g.setColour (theme::bad.withAlpha (0.12f + 0.3f * k));
                g.fillRect (Rectangle<float> (x, y, (float) kColStep, a.getBottom() - y));
            }
        }
        g.setColour (theme::scSpec); g.setFont (theme::font (11.0f));
        g.drawText ("sidechain", a.reduced (8, 4).removeFromTop (14), Justification::topRight);
    }

    // curves
    const int cols = (int) (a.getWidth() / 3) + 2;
    std::vector<double> total ((size_t) cols, 0.0);
    const auto snap = proc.autoSnapshot();
    const bool autoOn = proc.effectiveAutoMode() != params::autoOff && snap.result.curve.valid;
    const double amount = proc.apvts.getRawParameterValue (params::id::autoAmount)->load() / 100.0;
    const bool linear = (int) proc.apvts.getRawParameterValue (params::id::phaseMode)->load() == 1;

    for (int b = 0; b < arc::kNumBands; ++b)
    {
        const auto& s = bands[(size_t) b];
        if (! s.active()) continue;
        const double dyn = linear ? 0.0 : proc.dynamicDelta (b);
        const auto& curve = bandCurve (b, s, s.gainDb * scale + dyn, cols);
        for (int i = 0; i < cols; ++i) total[(size_t) i] += curve[(size_t) i];
        // calm display: only the band you're working on shows its own shape
        if (! (b == selected || b == hovered || b == dragging)) continue;
        Path p;
        const float zero = yForGain (0);
        p.startNewSubPath (a.getX(), zero);
        for (int i = 0; i < cols; ++i)
        {
            const float x = a.getX() + (float) i * 3;
            const double db = curve[(size_t) i];
            p.lineTo (x, jlimit (a.getY() - 2, a.getBottom() + 2, yForGain (db)));
        }
        p.lineTo (a.getRight(), zero);
        const auto c = theme::bandColour (b);
        g.setColour (c.withAlpha (b == dragging || b == hovered ? 0.24f : 0.16f));
        g.fillPath (p);
        g.setColour (c.withAlpha (0.85f));
        g.strokePath (p, PathStrokeType (1.0f));
    }
    if (autoOn)
    {
        std::array<double, arc::kAutoBells> key {};
        for (int k = 0; k < arc::kAutoBells; ++k) key[(size_t) k] = snap.result.curve.gainsDb[(size_t) k] * amount + snap.result.curve.hpfHz * 1e-3;
        if (key != autoKey || (int) autoCache.db.size() != cols || autoCache.width != a.getWidth() || autoCache.fs != fs)
        {
            autoKey = key; autoCache.db.resize ((size_t) cols); autoCache.width = a.getWidth(); autoCache.fs = fs;
            for (int i = 0; i < cols; ++i) autoCache.db[(size_t) i] = (float) arc::autoCurveDb (snap.result.curve, amount, freqForX (a.getX() + (float) i * 3), fs);
        }
        Path p;
        for (int i = 0; i < cols; ++i)
        {
            const float x = a.getX() + (float) i * 3;
            const double db = autoCache.db[(size_t) i];
            total[(size_t) i] += db;
            const float y = jlimit (a.getY() - 2, a.getBottom() + 2, yForGain (db));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        Path dashed;
        const float dashes[] = { 6.0f, 4.0f };
        PathStrokeType (1.6f).createDashedStroke (dashed, p, dashes, 2);
        g.setColour (theme::autoCol.withAlpha (0.9f));
        g.fillPath (dashed);
    }
    {
        Path p;
        for (int i = 0; i < cols; ++i)
        {
            const float x = a.getX() + (float) i * 3;
            const float y = jlimit (a.getY() - 2, a.getBottom() + 2, yForGain (total[(size_t) i]));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (theme::accent.withAlpha (0.18f));
        g.strokePath (p, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white.withAlpha (0.95f));
        g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // nodes
    for (int b = 0; b < arc::kNumBands; ++b)
    {
        const auto& s = bands[(size_t) b];
        if (! s.used) continue;
        const auto pos = nodePosition (s);
        const auto c = theme::bandColour (b);
        const float r = (b == selected || b == hovered) ? 8.5f : 7.0f;
        if (s.dynamic && arc::typeSupportsDynamic (s.type) && ! linear)
        {   // line + ring show where the dynamic range can take the band
            g.setColour (c.withAlpha (0.35f));
            const float yr = yForGain (s.gainDb * scale + s.rangeDb);
            g.drawLine (pos.x, pos.y, pos.x, jlimit (a.getY(), a.getBottom(), yr), 2.0f);
            g.drawEllipse (Rectangle<float> (r * 2.4f, r * 2.4f).withCentre ({ pos.x, yr }), 1.2f);
        }
        if (b == selected)
        {   // soft halo around the selected band
            g.setGradientFill (ColourGradient (c.withAlpha (0.45f), pos.x, pos.y, c.withAlpha (0.0f), pos.x + r * 2.6f, pos.y, true));
            g.fillEllipse (Rectangle<float> (r * 5.2f, r * 5.2f).withCentre (pos));
        }
        theme::drawOrb (g, pos, r, s.bypass ? theme::panelHi : c, b == selected || b == hovered);
        if (b == selected) { g.setColour (Colours::white.withAlpha (0.9f)); g.drawEllipse (Rectangle<float> (r * 2 + 3, r * 2 + 3).withCentre (pos), 1.5f); }
        g.setColour (s.bypass ? theme::textDim : Colours::black.withAlpha (0.8f));
        g.setFont (theme::font (10.0f, true));
        g.drawText (String (b + 1), Rectangle<float> (r * 2, r * 2).withCentre (pos), Justification::centred);
    }

    auto tag = [&] (Point<float> at, const String& t, Colour border)
    {
        const float w = (float) GlyphArrangement::getStringWidthInt (theme::font (12.0f), t) + 16.0f;
        Rectangle<float> box (at.x + 12, at.y - 30, w, 20);
        if (box.getRight() > a.getRight()) box.setX (at.x - 12 - w);
        if (box.getY() < a.getY()) box.setY (at.y + 12);
        g.setColour (theme::panel.withAlpha (0.94f)); g.fillRoundedRectangle (box, 4);
        g.setColour (border.withAlpha (0.6f)); g.drawRoundedRectangle (box, 4, 1);
        g.setColour (theme::text); g.setFont (theme::font (12.0f));
        g.drawText (t, box, Justification::centred);
    };

    // band readout
    const int show = dragging >= 0 ? dragging : hovered;
    if (show >= 0 && bands[(size_t) show].used)
    {
        const auto& s = bands[(size_t) show];
        String t = theme::freqText (s.freq) + "  (" + theme::noteName (s.freq) + ")";
        if (arc::typeHasGain (s.type)) t << "   " << (s.gainDb > 0 ? "+" : "") << String (s.gainDb, 1) << " dB";
        t << "   Q " << String (s.q, 2);
        tag (nodePosition (s), t, theme::bandColour (show));
    }
    else if (hoverPeak.valid && mouseInside)
    {   // spectrum grab target
        g.setColour (theme::accent);
        g.drawEllipse (Rectangle<float> (14, 14).withCentre (hoverPeak.pt), 1.6f);
        g.drawVerticalLine ((int) hoverPeak.pt.x, hoverPeak.pt.y + 7, a.getBottom());
        tag (hoverPeak.pt, theme::noteName (hoverPeak.freq) + "   " + theme::freqText (hoverPeak.freq) + "   drag to EQ", theme::accent);
    }
    else if (mouseInside && a.contains (mousePos) && dragging < 0)
    {   // cursor readout along the bottom
        const double f = freqForX (mousePos.x);
        g.setColour (theme::text.withAlpha (0.15f));
        g.drawVerticalLine ((int) mousePos.x, a.getY(), a.getBottom());
        const String t = theme::freqText (f) + "   " + theme::noteName (f) + "   " + (gainForY (mousePos.y) >= 0 ? "+" : "") + String (gainForY (mousePos.y), 1) + " dB";
        g.setColour (theme::textDim); g.setFont (theme::font (11.0f));
        g.drawText (t, a.reduced (6, 4).removeFromBottom (14), Justification::bottomRight);
    }

    if (std::none_of (bands.begin(), bands.end(), [] (auto& s) { return s.used; }))
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (14.0f));
        g.drawText ("Double-click to add a band, or drag a peak in the analyzer.  Scroll = Q, right-click = options, Alt-click = delete.",
                    a.withTrimmedTop (a.getHeight() * 0.62f).withHeight (24), Justification::centred);
    }
    if (message.isNotEmpty() && Time::getMillisecondCounter() < messageUntil)
    {
        g.setFont (theme::font (13.0f));
        const float w = (float) GlyphArrangement::getStringWidthInt (theme::font (13.0f), message) + 28.0f;
        auto box = Rectangle<float> (w, 26).withCentre ({ a.getCentreX(), a.getY() + 30 });
        g.setColour (theme::panel.withAlpha (0.95f)); g.fillRoundedRectangle (box, 6);
        g.setColour (theme::accent.withAlpha (0.7f)); g.drawRoundedRectangle (box, 6, 1);
        g.setColour (theme::text); g.drawText (message, box, Justification::centred);
    }
    String status;
    if (linear) status << "Linear phase  (latency " << String (1000.0 * proc.getLatencySamples() / jmax (1.0, fs), 0) << " ms, dynamic bands act static)    ";
    if (setting (ana::freeze, 0) != 0) status << "ANALYZER FROZEN";
    if (status.isNotEmpty())
    {
        g.setColour (theme::textDim); g.setFont (theme::font (11.0f));
        g.drawText (status, a.withHeight (16).translated (6, 2), Justification::left);
    }
}

// Spectrum fill styles. Colour always follows loudness: darkest = quiet, lightest = loud.
void EqDisplay::drawStyledSpectrum (Graphics& g, const Spectrum& s, const theme::Palette& pal, int style)
{
    if (s.display.empty()) return;
    const auto a = plotArea();
    // colour scale follows the loudest part of the current spectrum: top 36 dB spread from darkest to lightest
    const float top = *std::max_element (s.display.begin(), s.display.end());
    auto levelAt = [top] (float db) { return jlimit (0.0f, 1.0f, (db - (top - 36.0f)) / 36.0f); };

    Path area;
    area.startNewSubPath (a.getX(), a.getBottom());
    Point<float> prev;
    for (size_t i = 0; i < s.display.size(); ++i)
    {
        const Point<float> pt (a.getX() + (float) i * kColStep, yForSpectrum (s.display[i]));
        if (i == 0) area.lineTo (pt); else area.quadraticTo (prev, (prev + pt) * 0.5f);
        prev = pt;
    }
    area.lineTo (prev); area.lineTo (a.getRight(), a.getBottom()); area.closeSubPath();

    if (style == 3)
    {   // slices: Riemann-sum bars, each coloured by its own level
        const float w = 3.5f, gap = 2.0f;
        for (float x = a.getX(); x < a.getRight(); x += w + gap)
        {
            const int c0 = (int) ((x - a.getX()) / kColStep), c1 = jmin ((int) s.display.size() - 1, (int) ((x + w - a.getX()) / kColStep));
            float db = -200.0f;
            for (int c = jmax (0, c0); c <= c1; ++c) db = jmax (db, s.display[(size_t) c]);
            const float y = yForSpectrum (db), t = levelAt (db);
            if (y >= a.getBottom() - 1) continue;
            const Rectangle<float> bar (x, y, w, a.getBottom() - y);
            g.setGradientFill (ColourGradient (pal.at (t).withAlpha (0.95f), 0, y, pal.at (t * 0.35f).withAlpha (0.35f), 0, a.getBottom(), false));
            g.fillRoundedRectangle (bar, 1.75f);
            g.setColour (Colours::white.withAlpha (0.3f * t));
            g.fillRoundedRectangle (bar.withHeight (jmin (2.0f, bar.getHeight())), 1.0f);
        }
        return;
    }
    if (style == 1)
    {   // colour by level: hue follows each frequency's loudness, fading toward the floor
        ColourGradient grad (pal.at (levelAt (s.display.front())), a.getX(), 0, pal.at (levelAt (s.display.back())), a.getRight(), 0, false);
        for (size_t i = 0; i < s.display.size(); i += 4)
            grad.addColour (jlimit (0.0, 1.0, (double) i * kColStep / a.getWidth()), pal.at (levelAt (s.display[i])).withAlpha (0.92f));
        g.setGradientFill (grad);
        g.fillPath (area);
        g.setGradientFill (ColourGradient (theme::plotBg.withAlpha (0.0f), 0, a.getY(), theme::plotBg.withAlpha (0.85f), 0, a.getBottom(), false));
        g.fillPath (area);
    }
    else if (style == 2)
    {   // heat map: colour depends on height, so only loud peaks reach the lightest colour
        ColourGradient grad (pal.light, 0, a.getY(), pal.dark.withAlpha (0.25f), 0, a.getBottom(), false);
        grad.addColour (0.35, pal.at (0.7f));
        grad.addColour (0.65, pal.at (0.35f).withAlpha (0.8f));
        g.setGradientFill (grad);
        g.fillPath (area);
    }
    Path line;
    for (size_t i = 0; i < s.display.size(); ++i)
    {
        const Point<float> pt (a.getX() + (float) i * kColStep, yForSpectrum (s.display[i]));
        if (i == 0) line.startNewSubPath (pt); else line.lineTo (pt);
    }
    g.setColour (pal.light.withAlpha (style == 0 ? 0.9f : 0.55f));
    g.strokePath (line, PathStrokeType (style == 0 ? 1.4f : 1.0f));
}

// Spectrogram: one row per frame, scrolling down; brightness/colour = loudness.
void EqDisplay::updateWaterfall (const Spectrum& s)
{
    const auto a = plotArea();
    const int w = jmax (1, (int) s.display.size()), h = jmax (16, (int) a.getHeight() / 2);
    if (! waterfall.isValid() || waterfall.getWidth() != w || waterfall.getHeight() != h)
        waterfall = Image (Image::ARGB, w, h, true);
    waterfall.moveImageSection (0, 1, 0, 0, w, h - 1);
    const auto pal = theme::paletteFrom (proc.apvts.state);
    const float frameTop = *std::max_element (s.display.begin(), s.display.end());
    waterfallTop = jmax (frameTop, waterfallTop - 0.25f);   // slowly falling reference so quiet passages dim
    for (int x = 0; x < w; ++x)
    {
        const float t = jlimit (0.0f, 1.0f, (s.display[(size_t) x] - (waterfallTop - 48.0f)) / 48.0f);
        waterfall.setPixelAt (x, 0, pal.at (t).withAlpha (jlimit (0.0f, 1.0f, t * 1.4f)));
    }
}

int EqDisplay::hitNode (Point<float> pt) const
{
    int best = -1; float bestD = 12.0f;
    for (int b = 0; b < arc::kNumBands; ++b)
    {
        if (! bands[(size_t) b].used) continue;
        const float d = nodePosition (bands[(size_t) b]).getDistanceFrom (pt);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EqDisplay::setSelected (int b)
{
    if (b == selected) return;
    selected = b;
    if (onSelectionChanged) onSelectionChanged (b);
    repaint();
}

void EqDisplay::setParamPlain (int band, const char* what, float v)
{
    if (auto* p = proc.apvts.getParameter (params::bandId (band, what))) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

int EqDisplay::addBand (arc::BandType t, double f, double gainDb, double q)
{
    const int b = proc.bandToFill();
    if (b < 0) return -1;
    proc.setParam (params::bandId (b, "type"), (float) t);
    proc.setParam (params::bandId (b, "freq"), (float) f);
    proc.setParam (params::bandId (b, "gain"), (float) jlimit (-30.0, 30.0, gainDb));
    proc.setParam (params::bandId (b, "q"), (float) q);
    proc.setParam (params::bandId (b, "slope"), t == arc::BandType::Bell ? 1.0f : 3.0f);
    proc.setParam (params::bandId (b, "place"), 0.0f);
    proc.setParam (params::bandId (b, "dyn"), 0.0f);
    proc.setParam (params::bandId (b, "bypass"), 0.0f);
    proc.setParam (params::bandId (b, "used"), 1.0f);
    bands[(size_t) b] = proc.bandSettings (b);
    setSelected (b);
    return b;
}

void EqDisplay::deleteBand (int b)
{
    proc.setParam (params::bandId (b, "used"), 0.0f);
    proc.setParam (params::bandId (b, "gain"), 0.0f);
    proc.setParam (params::bandId (b, "dyn"), 0.0f);
    if (selected == b) setSelected (-1);
}

void EqDisplay::showBandMenu (int b)
{
    const auto s = proc.bandSettings (b);
    PopupMenu m, types, slopes, places;
    for (int i = 0; i < (int) arc::BandType::Count; ++i) types.addItem (100 + i, arc::bandTypeName ((arc::BandType) i), true, (int) s.type == i);
    for (int i = 0; i < (int) arc::kSlopes.size(); ++i) slopes.addItem (200 + i, String (arc::kSlopes[(size_t) i]) + " dB/oct", arc::typeHasSlope (s.type), s.slope == arc::kSlopes[(size_t) i]);
    for (int i = 0; i < (int) arc::Placement::Count; ++i) places.addItem (300 + i, arc::placementName ((arc::Placement) i), true, (int) s.place == i);
    m.addSectionHeader ("Band " + String (b + 1) + "  -  " + theme::noteName (s.freq));
    m.addSubMenu ("Shape", types);
    m.addSubMenu ("Slope", slopes, arc::typeHasSlope (s.type));
    m.addSubMenu ("Stereo", places);
    m.addItem (1, "Dynamic EQ", arc::typeSupportsDynamic (s.type), s.dynamic);
    m.addItem (2, "Bypass", true, s.bypass);
    m.addItem (4, "Snap to nearest note (" + theme::noteName (440.0 * std::pow (2.0, std::round (12.0 * std::log2 (s.freq / 440.0)) / 12.0)) + ")");
    m.addSeparator();
    m.addItem (3, "Delete");
    SafePointer<EqDisplay> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, b, s] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r >= 100 && r < 200) pr.setParam (params::bandId (b, "type"), (float) (r - 100));
        else if (r >= 200 && r < 300) pr.setParam (params::bandId (b, "slope"), (float) (r - 200));
        else if (r >= 300 && r < 400) pr.setParam (params::bandId (b, "place"), (float) (r - 300));
        else if (r == 1) pr.setParam (params::bandId (b, "dyn"), s.dynamic ? 0.0f : 1.0f);
        else if (r == 2) pr.setParam (params::bandId (b, "bypass"), s.bypass ? 0.0f : 1.0f);
        else if (r == 3) safe->deleteBand (b);
        else if (r == 4) pr.setParam (params::bandId (b, "freq"), (float) (440.0 * std::pow (2.0, std::round (12.0 * std::log2 (s.freq / 440.0)) / 12.0)));
    });
}

void EqDisplay::mouseMove (const MouseEvent& e)
{
    mousePos = e.position; mouseInside = true;
    const int h = hitNode (e.position);
    if (h != hovered) hovered = h;
    if (h < 0) findPeakNear (e.position); else hoverPeak = {};
    setMouseCursor (h >= 0 || hoverPeak.valid ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor);
    repaint();
}
void EqDisplay::mouseExit (const MouseEvent&) { hovered = -1; mouseInside = false; hoverPeak = {}; repaint(); }

void EqDisplay::mouseDown (const MouseEvent& e)
{
    int h = hitNode (e.position);
    if (h < 0 && ! hoverPeak.valid && e.mods.isLeftButtonDown() && e.getNumberOfClicks() == 1) setSelected (-1);   // click empty space: close the band panel
    if (h < 0 && hoverPeak.valid && e.mods.isLeftButtonDown() && ! e.mods.isAltDown())
    {   // spectrum grab: new narrow bell on the peak, drag up/down to set its gain
        h = addBand (arc::BandType::Bell, hoverPeak.freq, 0.0, 4.0);
        if (h < 0) return;
        grabbing = true;
    }
    if (h < 0) return;
    setSelected (h);
    if (e.mods.isAltDown() || e.mods.isMiddleButtonDown()) { deleteBand (h); return; }
    if (e.mods.isPopupMenu()) { showBandMenu (h); return; }
    dragging = h;
    hoverPeak = {};
    dragStart = e.position;
    dragStartFreq = bands[(size_t) h].freq;
    dragStartGain = bands[(size_t) h].gainDb;
    for (auto* w : { "freq", "gain" }) if (auto* p = proc.apvts.getParameter (params::bandId (h, w))) p->beginChangeGesture();
}

void EqDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const auto& s = bands[(size_t) dragging];
    const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
    const auto delta = (e.position - dragStart) * fine;
    // relative moves: the node never jumps to the cursor
    const bool freqOnly = e.mods.isCommandDown() && ! grabbing;
    if (! grabbing || std::abs (delta.x) > 6.0f)
        setParamPlain (dragging, "freq", (float) jlimit (10.0, 30000.0, freqForX (xForFreq (dragStartFreq) + delta.x)));
    if (arc::typeHasGain (s.type) && ! freqOnly)
    {
        const double scale = jmax (0.01f, proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0f);
        const double dbPerPx = displayRange() / (plotArea().getHeight() * 0.46);
        double g = dragStartGain - delta.y * dbPerPx / scale;
        if (s.type == arc::BandType::TiltShelf) g = dragStartGain - delta.y * 0.1;
        setParamPlain (dragging, "gain", (float) jlimit (-30.0, 30.0, g));
    }
    bands[(size_t) dragging] = proc.bandSettings (dragging);
    repaint();
}

void EqDisplay::mouseUp (const MouseEvent&)
{
    if (dragging >= 0)
        for (auto* w : { "freq", "gain" }) if (auto* p = proc.apvts.getParameter (params::bandId (dragging, w))) p->endChangeGesture();
    dragging = -1;
    grabbing = false;
}

void EqDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h >= 0) { proc.setParam (params::bandId (h, "bypass"), bands[(size_t) h].bypass ? 0.0f : 1.0f); return; }
    if (! plotArea().contains (e.position)) return;
    const double f = freqForX (e.position.x);
    const double scale = jmax (0.01f, proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0f);
    if (f < 30) addBand (arc::BandType::LowCut, f, 0, 0.7071);
    else if (f > 16000) addBand (arc::BandType::HighCut, f, 0, 0.7071);
    else addBand (arc::BandType::Bell, f, gainForY (e.position.y) / scale, 1.0);
}

void EqDisplay::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    int b = hitNode (e.position);
    if (b < 0) b = selected;
    if (b < 0 || ! bands[(size_t) b].used) return;
    const float q = (float) jlimit (0.025, 40.0, bands[(size_t) b].q * std::exp (w.deltaY * (e.mods.isShiftDown() ? 0.4 : 1.5)));
    proc.setParam (params::bandId (b, "q"), q);
    bands[(size_t) b] = proc.bandSettings (b);
}
