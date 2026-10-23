# Pinky Lite 2 💗

**Distortion · crush · filter · motion — by [seoulbby](https://github.com/seoulbby)**

A pink, animated, multi-mode distortion plugin for **FL Studio** (VST3, Windows/macOS) and standalone.
Successor to *Pinky Lite* (crush + filter + mix), rebuilt from scratch with the kind of saturation
you get from ShaperBox / Portal / Autochroma-style tools.

![light](preview-light.png)
![settings](preview-settings.png)

## What's inside
- **8 distortion modes** – Soft, Tube, Tape, Fuzz, Fold, Warp, Clip, Rect, each with *Shape* and *Bias*
- **2-band engine** – Linkwitz-Riley crossover with separate Low / High drive (Autochroma-style colour)
- **Motion** – tempo-synced LFO (1/1 … 1/32, triplets, dotted) with Sine / Tri / Saw / Square / Pump / Random,
  modulating Drive, Filter Cutoff and Crush (ShaperBox-style pumping)
- **Crush** – bit depth + sample-rate reduction
- **Filter** – LP / HP / BP state-variable with resonance
- **Tone** tilt EQ, **Width**, **Mix** (phase-coherent), **Input / Output**
- **Oversampling** Eco 1x / HQ 2x / Ultra 4x with automatic latency compensation
- **11 factory presets**, live transfer-curve / band / filter / LFO displays, level meters
- **Settings page inside the plugin** (gear icon): Light / Night theme, 4 accent colours, window size
  (80–150 % or drag the corner), animation level, knob feel, tooltips, auto-gain, safety limiter.
  Settings are saved with the project *and* remembered for new instances.

Tips: double-click a knob to reset it, mouse-wheel works on knobs, presets, and the LFO sync selector.

## Build (local)
Requirements: CMake ≥ 3.22, a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS), git.

```bash
git clone https://github.com/<you>/pinky-lite-2.git
cd pinky-lite-2
cmake -S . -B build -DPINKY_BUILD_TESTS=ON      # downloads JUCE 8.0.4 automatically
cmake --build build --config Release --target PinkyLite2_VST3 PinkyLite2_Standalone
```

Windows output: `build/PinkyLite2_artefacts/Release/VST3/Pinky Lite 2.vst3`
→ copy to `C:\Program Files\Common Files\VST3\`, then in FL Studio:
**Options → Manage plugins → Find installed plugins** (or *Start scan*).

## Build on GitHub (no local setup)
1. Create a new repo, push this folder.
2. **Actions → Build Pinky Lite 2** runs automatically; download `Pinky-Lite-2-Windows` from the run's artifacts.
3. To publish a release: `git tag v2.0.0 && git push --tags` – the zip files are attached automatically.

## Tests
`PinkyTests` checks that every mode/quality stays finite and limited, latency/phase alignment of the dry path,
preset rendering and state save/restore:
```bash
build/PinkyTests_artefacts/Release/PinkyTests [optional-folder-for-ui-screenshots]
```

## Project layout
```





Tests/Main.cpp             headless DSP + UI snapshot tests
```

## License
Source code: MIT © 2026 seoulbby. Built with [JUCE](https://juce.com) (AGPLv3 / commercial) – if you distribute
binaries publicly, keep the source available (AGPLv3) or get a JUCE licence.
