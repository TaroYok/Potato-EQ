# Plan: LUFS / dB toggle (for tomorrow)

## What exists today
- Loudness panel shows **LUFS only**: integrated (big number), short-term, momentary, LRA, true peak (dBTP),
  plus a 30 s momentary-LUFS history graph and the target / match / auto-level / limiter controls.
- `arc::LoudnessMeter` already computes sample peak and 4x true peak (`samplePeakDb`, `truePeakDb`), but no RMS,
  no peak hold/decay, and there's no input-side (pre-EQ) meter in the UI. `preMeter` exists but is only used by auto level.
- The limiter reports gain reduction (text only).

## Ideas

### A. Unit switch on the loudness panel  (smallest, ~1-2 h)
A pill toggle `LUFS | dB` in the panel header.
- **LUFS mode**: today's view.
- **dB mode**: big number = **RMS (dBFS, 300 ms)**, rows = Peak (dBFS, with 3 s hold), True peak (dBTP),
  RMS max, **Crest factor** (peak − RMS, in dB: how "squashed" the track is), headroom to 0 dBFS.
- History graph follows the unit (momentary LUFS ↔ RMS dB). Target line only in LUFS mode.
- Needs: RMS integrator + peak hold in `LoudnessMeter` (cheap: one sum of squares per sample).

### B. Vertical bar meters with a unit switch  (medium, ~3-4 h)  ★ recommended with A
Slim glossy In / Out bar meters (and a GR bar when the limiter is on) at the right edge of the EQ display,
iPod-style rounded capsules.
- **dB mode**: peak bar + RMS bar overlay, peak-hold tick, clip light (click to reset), scale 0 … −60 dBFS.
- **LUFS mode**: momentary + short-term bars on a LUFS scale (0 … −40) with the target as a marker.
- Click the scale to flip units (shares the same setting as A, so the whole UI flips together).
- Needs: input meter (meter `preFifo` side or a second `LoudnessMeter` on the input), GR history.

### C. "Primary unit" global setting  (small add-on to A+B)
One setting in Display… → Units: `LUFS` / `dBFS` / `Both`. *Both* shows LUFS big + dB small underneath.
Also affects the Output-gain readout tooltip and Match-target messages.

### D. Analyzer scale in dB vs. loudness-weighted  (optional, experimental)
Toggle the spectrum's vertical meaning: plain dB per band (today) vs. **K-weighted** (what LUFS "hears":
lows down, ~2–4 kHz up). Useful for seeing why a track measures loud. Put under Experimental tools.

## Recommendation
Do **A + B** with one shared unit setting (that's C for free), skip D unless time is left.

## Implementation steps
1. **DSP** (`Source/dsp/Loudness.*`): add RMS (300 ms exponential, also 3 s), peak with hold (3 s) and decay
   (20 dB/s), crest factor, clip flag. Expose in `Readings`. Keep it allocation-free.
2. **Input meter**: run a `LoudnessMeter` on the input in `processBlock` (before EQ). Cost is tiny.
3. **Setting**: `ana::units` (0 LUFS, 1 dB, 2 Both) on the plugin state (saved per project).
4. **UI A**: pill toggle in `LoudnessPanel` header; second set of rows; history graph switches source.
5. **UI B**: new `LevelMeters` component (In / Out / GR), drawn with the theme's glass/orb style,
   30 fps, ballistics done on the UI side; place inside the display's right margin.
6. **Tests** (`DspTests.cpp`): sine at −20 dBFS peak → RMS −23.01 dBFS; square wave crest 0 dB;
   peak-hold timing; clip flag at 0 dBFS. Harness: toggle units, screenshot both modes.
7. Ship: merge → CI → download + install on the laptop (same routine as tonight).

## Also on tomorrow's list
- Install tonight's build (Frutiger Aero / iPod look) if it wasn't installed yet.
- FL Studio connector (flstudio-mcp, code reviewed + download verified tonight):
  install Python 3.12 + loopMIDI → create ports `FLStudioMCP RX` / `FLStudioMCP TX` → run installer (no audio extras)
  → FL MIDI settings → start daemon → add to Claude desktop config → restart Claude.
- Optional: finish cross-track collision detection (branch `wip/collision-detection`).
