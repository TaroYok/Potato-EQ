# Potato EQ: notes for Claude

JUCE 9 audio plugin (VST3 / AU / Standalone): 24-band EQ, BS.1770 loudness meter, auto level, limiter,
experimental Auto EQ. Owner: TaroYok. Licence: AGPLv3 (JUCE used under its AGPL option).
README.md describes every feature; read it before changing behaviour.

## Build and test on Windows (this laptop)
Visual Studio 2022 (C++ workload) is installed; use a "Developer PowerShell for VS 2022" or let CMake find it.
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PotatoEQ_VST3 potato_tests
ctest --test-dir build -C Release --output-on-failure
```
First configure downloads JUCE (pinned commit in CMakeLists.txt). Built plugin:
`build/PotatoEQ_artefacts/Release/VST3/Potato EQ.vst3`.

## Installing for testing in FL Studio
- FL Studio 2026 must be closed first (check `Get-Process FL64`); the DLL is locked while FL runs.
- Copy the whole `Potato EQ.vst3` folder into `C:\Program Files\Common Files\VST3\` (needs admin: the user
  approves the UAC prompt). Replace the old folder, then compare file hashes to confirm.
- Then the user rescans or just reopens FL.

## Working rules the owner set
- Don't push untested work to `main`: use a branch, run the tests, merge when they pass.
- Batch several tweaks per build when possible.
- Never permanently delete the owner's files: move them to a `_TO DELETE` folder instead.
- Ask before downloading or installing anything on the laptop.
- Commits use the GitHub no-reply email (127334506+TaroYok@users.noreply.github.com), never a personal one.
- CI (.github/workflows/build.yml) builds Windows/macOS/Linux on every push to main and publishes a Release.

## Code map
- `Source/dsp/`: filters (TPT SVF), EqEngine (bands, M/S, dynamic, sidechain), Loudness (BS.1770), Limiter, AutoEq.
- `Source/PluginProcessor.*`: parameters, auto level (ride / learn-then-lock), linear phase FIR, presets, state.
- `Source/ui/`: Theme (colour schemes, fonts), EqDisplay (analyzer), Panels (band, loudness, meters, auto EQ).
- `tests/DspTests.cpp` (ctest), `tests/PluginHarness.cpp` (end-to-end + screenshots; Linux CI).
- Known flake: harness checks for "learned a curve after ~12 s" can fail on a slow CI runner; a re-run passes.

## Open ideas
- Collision detection work in progress on branch `wip/collision-detection`.
- Make the timing-based harness checks more tolerant of slow machines.
