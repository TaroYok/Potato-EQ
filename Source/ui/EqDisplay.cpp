#include "EqDisplay.h"
#include <algorithm>

using namespace juce;

static constexpr double kLoHz = 20.0, kHiHz = 20000.0;
static constexpr int kColStep = 2;

EqDisplay::EqDisplay (PotatoEQProcessor& p) : proc (p)
{
    preRing.assign (kFftSize, 0.0f); postRing.assign (kFftSize, 0.0f);
    fftData.assign (2 * kFftSize, 0.0f);
    window.resize (kFftSize);
    for (int i = 0; i < kFftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / kFftSize);
    setOpaque (true);
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
float EqDisplay::yForSpectrum (double db) const { auto a = plotArea(); return a.getY() + a.getHeight() * (float) jlimit (0.0, 1.0, (6.0 - db) / 96.0); }

void EqDisplay::resized()
{
    const int cols = jmax (1, (int) plotArea().getWidth() / kColStep + 1);
    preDisplay.assign ((size_t) cols, -120.0f);
    postDisplay.assign ((size_t) cols, -120.0f);
}

void EqDisplay::pullSpectrum (SampleFifo& fifo, std::vector<float>& ring, int& pos, std::vector<float>& display)
{
    float tmp[4096];
    int got;
    while ((got = fifo.pull (tmp, 4096)) > 0)
        for (int i = 0; i < got; ++i) { ring[(size_t) pos] = tmp[i]; pos = (pos + 1) % kFftSize; }

    for (int i = 0; i < kFftSize; ++i) fftData[(size_t) i] = ring[(size_t) ((pos + i) % kFftSize)] * window[(size_t) i];
    std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const double fs = proc.getFs(), binHz = fs / kFftSize;
    const auto a = plotArea();
    for (size_t c = 0; c < display.size(); ++c)
    {
        const float x0 = a.getX() + (float) c * kColStep;
        const double f0 = freqForX (x0), f1 = freqForX (x0 + kColStep);
        const int b0 = (int) std::floor (f0 / binHz), b1 = (int) std::ceil (f1 / binHz);
        float m = 0.0f;
        if (b1 - b0 <= 1)
        {   // fewer bins than pixels at the low end: interpolate
            const double fb = 0.5 * (f0 + f1) / binHz;
            const int k = jlimit (0, kFftSize / 2 - 1, (int) fb);
            const float t = (float) (fb - k);
            m = fftData[(size_t) k] * (1 - t) + fftData[(size_t) k + 1] * t;
        }
        else for (int k = jmax (0, b0); k <= jmin (kFftSize / 2, b1); ++k) m = jmax (m, fftData[(size_t) k]);

        const double fc = 0.5 * (f0 + f1);
        double db = Decibels::gainToDecibels (m * 4.0f / kFftSize, -150.0f) + 4.5 * std::log2 (fc / 1000.0);   // 4.5 dB/oct tilt, like most mastering analyzers
        auto& d = display[c];
        d = (float) (db > d ? d + (db - d) * 0.6 : d + (db - d) * 0.12);
    }
}

void EqDisplay::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue (params::id::analyzer)->load();
    if (mode == 1 || mode == 3) pullSpectrum (proc.preFifo, preRing, prePos, preDisplay);
    else { float t[4096]; while (proc.preFifo.pull (t, 4096) > 0) {} }
    if (mode == 2 || mode == 3) pullSpectrum (proc.postFifo, postRing, postPos, postDisplay);
    else { float t[4096]; while (proc.postFifo.pull (t, 4096) > 0) {} }
    for (int b = 0; b < arc::kNumBands; ++b) bands[(size_t) b] = proc.bandSettings (b);
    repaint();
}

Point<float> EqDisplay::nodePosition (int band, const arc::BandSettings& b) const
{
    const double fs = proc.getFs();
    const double scale = proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0;
    double y;
    if (arc::typeHasGain (b.type)) y = b.type == arc::BandType::TiltShelf ? 0.0 : b.gainDb * scale;
    else y = jlimit (-displayRange(), displayRange(), arc::designBand (b, 0, fs).magnitudeDb (b.freq, fs));
    if (b.type == arc::BandType::LowShelf || b.type == arc::BandType::HighShelf) y = b.gainDb * scale;
    (void) band;
    return { xForFreq (b.freq), jlimit (plotArea().getY() + 4, plotArea().getBottom() - 4, yForGain (y)) };
}

void EqDisplay::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    const auto a = plotArea();
    const double fs = proc.getFs();
    const double range = displayRange();
    const double scale = proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0;

    // grid
    g.setFont (theme::font (10.5f));
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0,
                      1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0, 10000.0, 20000.0 })
    {
        const bool major = f == 20 || f == 50 || f == 100 || f == 200 || f == 500 || f == 1000 || f == 2000 || f == 5000 || f == 10000 || f == 20000;
        const float x = xForFreq (f);
        g.setColour (theme::grid.withAlpha (major ? 1.0f : 0.45f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        if (major)
        {
            g.setColour (theme::gridText);
            g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), Rectangle<float> (x - 20, a.getBottom() + 1, 40, 12), Justification::centred);
        }
    }
    const double step = range <= 3 ? 1 : range <= 6 ? 2 : range <= 12 ? 3 : 6;
    for (double d = -range; d <= range + 0.01; d += step)
    {
        const float y = yForGain (d);
        g.setColour (d == 0 ? theme::grid.brighter (0.25f) : theme::grid);
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (theme::gridText);
        g.drawText ((d > 0 ? "+" : "") + String ((int) d), Rectangle<float> (a.getRight() + 2, y - 6, 24, 12), Justification::centredLeft);
    }

    // spectrum
    const int mode = (int) proc.apvts.getRawParameterValue (params::id::analyzer)->load();
    auto drawSpec = [&] (const std::vector<float>& d, Colour c, bool fill)
    {
        Path p;
        p.startNewSubPath (a.getX(), a.getBottom());
        for (size_t i = 0; i < d.size(); ++i) p.lineTo (a.getX() + (float) i * kColStep, yForSpectrum (d[i]));
        p.lineTo (a.getRight(), a.getBottom());
        p.closeSubPath();
        if (fill) { g.setGradientFill (ColourGradient (c.withAlpha (0.35f), 0, a.getY(), c.withAlpha (0.05f), 0, a.getBottom(), false)); g.fillPath (p); }
        g.setColour (c.withAlpha (fill ? 0.5f : 0.85f));
        g.strokePath (p, PathStrokeType (1.0f));
    };
    if (mode == 1 || mode == 3) drawSpec (preDisplay, theme::preSpec, true);
    if (mode == 2 || mode == 3) drawSpec (postDisplay, theme::postSpec, mode == 2);

    // curves
    const int cols = (int) (a.getWidth() / 3) + 2;
    std::vector<double> total ((size_t) cols, 0.0), autoDb ((size_t) cols, 0.0);
    const auto snap = proc.autoSnapshot();
    const bool autoOn = (int) proc.apvts.getRawParameterValue (params::id::autoMode)->load() != params::autoOff && snap.result.curve.valid;
    const double amount = proc.apvts.getRawParameterValue (params::id::autoAmount)->load() / 100.0;
    const bool linear = (int) proc.apvts.getRawParameterValue (params::id::phaseMode)->load() == 1;

    for (int b = 0; b < arc::kNumBands; ++b)
    {
        const auto& s = bands[(size_t) b];
        if (! s.active()) continue;
        const double dyn = linear ? 0.0 : proc.dynamicDelta (b);
        const auto set = arc::designBand (s, s.gainDb * scale + dyn, fs);
        Path p;
        const float zero = yForGain (0);
        p.startNewSubPath (a.getX(), zero);
        for (int i = 0; i < cols; ++i)
        {
            const float x = a.getX() + (float) i * 3;
            const double db = set.magnitudeDb (freqForX (x), fs);
            total[(size_t) i] += db;
            p.lineTo (x, jlimit (a.getY() - 2, a.getBottom() + 2, yForGain (db)));
        }
        p.lineTo (a.getRight(), zero);
        const auto c = theme::bandColour (b);
        g.setColour (c.withAlpha (b == selected ? 0.22f : 0.10f));
        g.fillPath (p);
        g.setColour (c.withAlpha (b == selected ? 0.9f : 0.35f));
        g.strokePath (p, PathStrokeType (1.0f));
    }
    if (autoOn)
    {
        Path p;
        for (int i = 0; i < cols; ++i)
        {
            const float x = a.getX() + (float) i * 3;
            autoDb[(size_t) i] = arc::autoCurveDb (snap.result.curve, amount, freqForX (x), fs);
            total[(size_t) i] += autoDb[(size_t) i];
            const float y = jlimit (a.getY() - 2, a.getBottom() + 2, yForGain (autoDb[(size_t) i]));
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
        g.setColour (Colours::white.withAlpha (0.12f));
        g.strokePath (p, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white.withAlpha (0.95f));
        g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // nodes
    for (int b = 0; b < arc::kNumBands; ++b)
    {
        const auto& s = bands[(size_t) b];
        if (! s.used) continue;
        const auto pos = nodePosition (b, s);
        const auto c = theme::bandColour (b);
        const float r = (b == selected || b == hovered) ? 8.5f : 7.0f;
        if (s.dynamic && arc::typeSupportsDynamic (s.type) && ! linear)
        {   // ring shows the dynamic range target
            g.setColour (c.withAlpha (0.35f));
            const float yr = yForGain (s.gainDb * scale + s.rangeDb);
            g.drawLine (pos.x, pos.y, pos.x, jlimit (a.getY(), a.getBottom(), yr), 2.0f);
            g.drawEllipse (Rectangle<float> (r * 2.4f, r * 2.4f).withCentre ({ pos.x, yr }), 1.2f);
        }
        g.setColour (s.bypass ? theme::panelHi : c);
        g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (pos));
        g.setColour (b == selected ? Colours::white : Colours::black.withAlpha (0.6f));
        g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (pos), b == selected ? 2.0f : 1.0f);
        g.setColour (s.bypass ? theme::textDim : Colours::black);
        g.setFont (theme::font (10.0f, true));
        g.drawText (String (b + 1), Rectangle<float> (r * 2, r * 2).withCentre (pos), Justification::centred);
    }

    // hover readout
    const int show = dragging >= 0 ? dragging : hovered;
    if (show >= 0 && bands[(size_t) show].used)
    {
        const auto& s = bands[(size_t) show];
        const auto pos = nodePosition (show, s);
        String t = theme::freqText (s.freq);
        if (arc::typeHasGain (s.type)) t << "   " << (s.gainDb > 0 ? "+" : "") << String (s.gainDb, 1) << " dB";
        t << "   Q " << String (s.q, 2);
        Rectangle<float> box (pos.x + 12, pos.y - 30, 170, 20);
        if (box.getRight() > a.getRight()) box.setX (pos.x - 182);
        if (box.getY() < a.getY()) box.setY (pos.y + 12);
        g.setColour (theme::panel.withAlpha (0.92f)); g.fillRoundedRectangle (box, 4);
        g.setColour (theme::text); g.setFont (theme::font (12.0f));
        g.drawText (t, box, Justification::centred);
    }

    if (bands.size() > 0 && std::none_of (bands.begin(), bands.end(), [] (auto& s) { return s.used; }))
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (14.0f));
        g.drawText ("Double-click anywhere to add a band.  Drag to move, scroll to change Q, right-click for options, Alt-click to delete.",
                    a.withTrimmedTop (a.getHeight() * 0.62f).withHeight (24), Justification::centred);
    }
    if (linear)
    {
        g.setColour (theme::textDim); g.setFont (theme::font (11.0f));
        g.drawText ("Linear phase  (latency " + String (1000.0 * proc.getLatencySamples() / jmax (1.0, fs), 0) + " ms, dynamic bands act static)",
                    a.withHeight (16).translated (6, 2), Justification::left);
    }
}

int EqDisplay::hitNode (Point<float> pt) const
{
    int best = -1; float bestD = 12.0f;
    for (int b = 0; b < arc::kNumBands; ++b)
    {
        if (! bands[(size_t) b].used) continue;
        const float d = nodePosition (b, bands[(size_t) b]).getDistanceFrom (pt);
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

void EqDisplay::addBandAt (Point<float> pt)
{
    const int b = proc.bandToFill();
    if (b < 0) return;
    const double f = freqForX (pt.x);
    arc::BandType t = arc::BandType::Bell;
    if (f < 30) t = arc::BandType::LowCut;
    else if (f > 16000) t = arc::BandType::HighCut;
    const double scale = jmax (0.01f, proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0f);
    proc.setParam (params::bandId (b, "type"), (float) t);
    proc.setParam (params::bandId (b, "freq"), (float) f);
    proc.setParam (params::bandId (b, "gain"), t == arc::BandType::Bell ? (float) jlimit (-30.0, 30.0, gainForY (pt.y) / scale) : 0.0f);
    proc.setParam (params::bandId (b, "q"), t == arc::BandType::Bell ? 1.0f : 0.7071f);
    proc.setParam (params::bandId (b, "slope"), t == arc::BandType::Bell ? 1.0f : 3.0f);
    proc.setParam (params::bandId (b, "place"), 0.0f);
    proc.setParam (params::bandId (b, "dyn"), 0.0f);
    proc.setParam (params::bandId (b, "bypass"), 0.0f);
    proc.setParam (params::bandId (b, "used"), 1.0f);
    bands[(size_t) b] = proc.bandSettings (b);
    setSelected (b);
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
    m.addSectionHeader ("Band " + String (b + 1));
    m.addSubMenu ("Shape", types);
    m.addSubMenu ("Slope", slopes, arc::typeHasSlope (s.type));
    m.addSubMenu ("Stereo", places);
    m.addItem (1, "Dynamic EQ", arc::typeSupportsDynamic (s.type), s.dynamic);
    m.addItem (2, "Bypass", true, s.bypass);
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
    });
}

void EqDisplay::mouseMove (const MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hovered) { hovered = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor);
}
void EqDisplay::mouseExit (const MouseEvent&) { hovered = -1; repaint(); }

void EqDisplay::mouseDown (const MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h < 0) return;
    setSelected (h);
    if (e.mods.isAltDown() || e.mods.isMiddleButtonDown()) { deleteBand (h); return; }
    if (e.mods.isPopupMenu()) { showBandMenu (h); return; }
    dragging = h;
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
    const auto pos = dragStart + (e.position - dragStart) * fine;
    const bool lockY = e.mods.isCommandDown();   // Ctrl/Cmd: frequency only
    setParamPlain (dragging, "freq", (float) jlimit (10.0, 30000.0, freqForX (pos.x)));
    if (arc::typeHasGain (s.type) && ! lockY)
    {
        const double scale = jmax (0.01f, proc.apvts.getRawParameterValue (params::id::gainScale)->load() / 100.0f);
        double g = gainForY (pos.y) / scale;
        if (s.type == arc::BandType::TiltShelf) g = dragStartGain - (pos.y - dragStart.y) * 0.1;
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
}

void EqDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h >= 0) proc.setParam (params::bandId (h, "bypass"), bands[(size_t) h].bypass ? 0.0f : 1.0f);
    else if (plotArea().contains (e.position)) addBandAt (e.position);
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
