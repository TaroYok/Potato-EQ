# Potato EQ

A 24-band parametric EQ (VST3 / AU / standalone) with a built-in LUFS meter and an
**Auto EQ** (under the **Experimental tools** switch, top right. While that's off, the Auto EQ is hidden and does nothing) that listens to your audio and steers its tonal balance toward a
genre or instrument target curve.

![Potato EQ](docs/screenshot.png)

## Download

Go to **[Releases](../../releases/latest)** and download **`PotatoEQ-Windows.zip`** (or `PotatoEQ-macOS.zip`).
Unzip it and copy the **`Potato EQ.vst3`** folder into `C:\Program Files\Common Files\VST3`
(Mac: `~/Library/Audio/Plug-Ins/VST3`), then rescan plugins in your DAW. The installers are attached there too.

Note: the green **Code → Download ZIP** button gives you the *source code*, not the plugin.

Every push to `main` also builds the plugin; those builds are under **Actions → Build plugin → Artifacts**.

## Features

**EQ**
- 24 bands: Bell, Low/High Shelf, Low/High Cut, Notch, Band Pass, Tilt Shelf
- Cut slopes 6 – 96 dB/oct; shelves 6 or 12 dB/oct; resonant Q on cuts
- Per-band stereo placement: Stereo, Left, Right, Mid, Side
- **Dynamic EQ** per band (bell and shelves): threshold + range, ~2:1 action
- **Zero Latency** (smooth, modulation-safe SVF filters) or **Linear Phase** (FIR, ~170 ms latency at 48 kHz)
- Gain scale (shrink/exaggerate every band at once), output gain, auto-gain
- **Sidechain**: any dynamic band can listen to the plugin's sidechain input instead of its own track
  (duck the bass at 60 Hz when the kick hits, carve 2–4 kHz out of the beat only while the vocal sings)
- Drag nodes to move, scroll for Q, double-click to add or bypass, right-click for options, Alt-click to delete.
  Shift = fine adjust, Ctrl/Cmd = frequency only.

**Analyzer**
- Pre/post spectrum, resolution (2k–16k FFT), speed, tilt (0 / 3 / 4.5 / 6 dB/oct), 60–120 dB range
- **Spectrum grab**: hover a peak in the analyzer, then drag it up or down to create a band right on it
- Freeze and peak hold
- Sidechain spectrum overlay with **collision highlighting**: red shows where this track and the sidechained track are both strong
- Note names everywhere (hover readout, band readouts, "snap band to nearest note"), handy for tuning 808s and kicks

**Factory presets** (Presets button, or your DAW's preset list)
- Character: Radio, Telephone, Lo-fi / Old Vinyl, Muffled (next room), Megaphone
- Drums: Kick Punch, Kick Sub Thump, Snare Crack, Hi-hats Smooth (dynamic), Drum Bus Glue
- Bass: 808 Weight, Bass Tighten
- Vocals: Clarity (with de-esser band), De-mud, De-ess, Backing Vocals Tuck
- Instruments: Acoustic Guitar, Piano Bright, Synth Pad Make Room
- Mix / Master: Polish, Air, Mono Bass (M/S), Wider Top (M/S)
- Sweeps: Low-Pass, High-Pass, Band-Pass, Resonant Peak, Notch. Automate Band 1's frequency in your DAW

**Loudness (ITU-R BS.1770-4 / EBU R128)**
- Momentary, short-term, gated integrated LUFS, loudness range (LRA), 4×-oversampled true peak
- Targets: Spotify/YouTube −14, Apple −16, club −9, loud −8, hip-hop/EDM −11, broadcast −23, podcast −16
- Custom target (−30 to −5 LUFS)
- **Auto level to target** rides the gain so the track lands on the target (works in offline renders too)
- **Limiter**: brickwall look-ahead limiter (1.5 ms) with adjustable ceiling, so loud targets don't clip
- **Match target** is the one-shot version. Without the limiter it never pushes the true peak above −1 dBTP

**Auto EQ**
- Profiles for full mixes (Balanced, Hip-Hop/Trap, EDM, Dubstep/DnB, Pop, R&B, Rock, Lo-fi, Acoustic/Jazz)
  and sources (lead/backing vocals, kick, snare, drum bus, bass/808, guitars, keys, synth lead/pad)
- **Match a reference track**: load a finished song (WAV/AIFF/FLAC/OGG/MP3) and steer toward *its* tonal balance
  instead of a generic curve. The reference's spectrum is saved with your project
- **Learn**: listens to ~12 s of non-silent audio, then holds the correction (saved with your project)
- **Live**: keeps listening (about a 6 s memory) and glides the EQ as the song changes
- Compares your long-term spectrum with the profile's target, smooths the difference to 1/3 octave,
  limits it (default +3 / −4 dB, adjustable), never boosts empty regions,
  and adds the profile's high-pass for sources
- **Amount** 0–150 %, and **Turn into bands** converts the result into ordinary editable bands
- Tweaks: **Brightness** (tilts the target ±3 dB/oct), **Low end** (±6 dB below ~120 Hz),
  **Max boost / Max cut**, and **Detail** (Broad = overall balance only, Fine = narrower corrections)

The target curves are smooth tonal-balance references, based on the typical long-term
spectra of commercial releases. They are starting points to steer toward, not rules;
trust your ears and your reference tracks.

## Performance

Measured by the test harness on a 2.1 GHz server CPU (one core):

| Scenario | CPU |
|---|---|
| Zero latency, all 24 bands (96 dB/oct cuts, 8 dynamic, M/S), live auto EQ, auto level, limiter | ~4 % |
| Same, with 24 parameters automated every block | ~4.5 % |
| Linear phase, everything on | ~2.5 % |

Parameter changes are smoothed every 32 samples (frequency in log space), and the state-variable
filters stay stable under fast modulation, so automation sweeps are click-free. In linear-phase mode the FIR
is rebuilt in the background and cross-faded, so fast automation is smoother in zero-latency mode.

## Building yourself

Needs CMake 3.22+ and a C++20 compiler (Visual Studio 2022, Xcode 15+, or GCC 11+/Clang 14+).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release        # DSP accuracy tests
```

JUCE 9.0.5 is downloaded automatically. Pass `-DJUCE_DIR=/path/to/JUCE` to use a local copy.

## Layout

```
Source/dsp/Filters.h      filter design + exact response math (TPT state-variable filters)
Source/dsp/EqEngine.h     zero-latency engine: 24 bands, M/S, dynamic EQ, auto layer
Source/dsp/Loudness.*     BS.1770 meter (K-weighting, gating, LRA, true peak)
Source/dsp/AutoEq.*       target profiles, spectrum → correction fit, curve → bands
Source/PluginProcessor.*  parameters, linear-phase FIR builder, learning, state
Source/ui/*               display, band/loudness/auto panels, look and feel
tests/DspTests.cpp        filter, engine, EBU loudness and auto-EQ tests
tests/PluginHarness.cpp   end-to-end plugin check + editor screenshot
```

Plugin made with [JUCE](https://juce.com). JUCE is dual-licensed (AGPLv3 or commercial).
If you plan to sell this plugin closed-source, you need a JUCE commercial licence (free tier available for small revenue).
