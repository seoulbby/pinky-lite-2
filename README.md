# Pinky Lite 2 💗

**Wobble · Shimmer · Glass · Glitch — by [seoulbby](https://github.com/seoulbby)**

A pink, animated "melody warper" for **FL Studio** (VST3, Windows/macOS) and standalone.
No distortion: instead four ways to bend and sparkle a melody, plus the original Pinky Lite crush + filter.

![light](preview-light.png)
![night](preview-night.png)

## Modules
- **Wobble** – dreamy pitch drift (tape wow + flutter) with stereo spread
- **Shimmer** – pitch-shifted reverb: +12 = octave up, -12 = octave down, 0 = plain reverb
- **Glass** – resonators tuned to a *key and scale* (6 scales): notes in your key ring out like glass bells
- **Glitch** – tempo-synced Stutter / Reverse / Tape-stop / Gate / Random with chance and smoothing
  (turn on *Repeatable glitches* in Settings for loops that glitch the same way every time)
- **Crush** (bits + downsample), **Filter** (LP/HP/BP), **Tone** tilt, **Width**, **Mix**, **Input/Output**
- **LFO** (tempo-synced, 6 shapes) that moves Wobble, Shimmer, Glass and the filter cutoff
- 12 factory presets, live visualisers, level meters
- **Settings page** (gear icon): Light/Night theme, 4 accent colours, window size, animation level,
  knob feel, tooltips, repeatable glitches, safety limiter

Tips: double-click a knob to reset it; mouse-wheel works on knobs, presets and selectors.

## Build on GitHub (no local setup)
1. Put all project files in a repository (`.github/workflows/build.yml` must exist).
2. **Actions → Build Pinky Lite 2** runs on every push; download `Pinky-Lite-2-Windows` from the run's *Artifacts*.
3. Copy `Pinky Lite 2.vst3` to `C:\Program Files\Common Files\VST3\`, then in FL Studio:
   **Options → Manage plugins → Find installed plugins**.

## Build locally
CMake >= 3.22, C++17 compiler (Visual Studio 2022 / Xcode).
```bash
cmake -S . -B build -DPINKY_BUILD_TESTS=ON      # downloads JUCE 8.0.4 automatically
cmake --build build --config Release --target PinkyLite2_VST3 PinkyLite2_Standalone
```

## Tests
`PinkyTests` checks stability (every glitch type / shimmer interval, shimmer tail decay), that Glass rings on
in-scale notes, deterministic glitches, wobble pitch, presets and state save/restore:
`PinkyTests [optional-folder-for-ui-screenshots]`

## License
Source code: MIT (c) 2026 seoulbby. Built with [JUCE](https://juce.com) (AGPLv3 / commercial): if you distribute
binaries publicly, keep the source available (AGPLv3) or get a JUCE licence.
