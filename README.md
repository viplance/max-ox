# MaxOx

A transparent VST3 maximizer plugin for macOS on Apple Silicon (M-series) with adaptive low-frequency management and an analog hardware-style UI.

## Features

- **Look-Ahead Limiter**: 5 ms look-ahead limiter with 8× oversampled
  inter-sample peak detection, soft-knee transition, cosine-windowed gain
  smoothing, and host-reported latency. A reconstruction-aware 8× guard after
  downsampling holds the final ceiling at −0.1 dBTP without a permanent
  safety-margin loss.
- **Adaptive Release**: Crest-factor-based release time — fast on sharp transients, slow on sustained material — preserves punch without pumping.
- **Adaptive Low-Frequency Control**: stereo-linked 5-band peakiness
  detector (30 / 55 / 90 / 150 / 250 Hz) compares narrow vs. wide band-pass
  energy and reduces a resonance only when the proposed change also lowers the
  instantaneous linked peak. A transparent 10 Hz infrasonic filter removes
  inaudible headroom consumption.
- **Adaptive Phase Rotation**: continuously evaluates eight stereo-linked
  four-stage all-pass configurations in the perceptually constrained
  40–200 Hz / radius 0.65–0.98 region. Processing engages only when measured
  peak reduction is at least 0.3 dB, with hysteresis and a 20 ms crossfade.
- **Perceptual Transient Shaving**: a soft peak shaver inside the existing 8×
  path removes at most 0.5–1.5 dB from high-crest transient events before the
  clean limiter. Sustained tonal material bypasses it automatically.
- **ERB Masking Control**: a 16-band ERB-spaced model tracks source and
  residual energy with simultaneous and temporal masking. Shaving depth falls
  automatically when its error becomes insufficiently masked, with additional
  protection for tonal and stereo-side material.
- **Single Gain Knob**: One control, 0–24 dB. Push harder for louder; the limiter and adaptive LF cut handle the rest.
- **Analog Needle Meters**: Three VU-style needle meters — Input level, Gain Reduction, and Output level — plus a glowing LF CUT activity indicator.
- **Hardware Chassis UI**: Dark metal faceplate with screws, ventilation slots, and cream-on-dark typography.
- **Apple Silicon Native**: Built for `arm64` with CMake and JUCE 8.

## Building for macOS (Apple Silicon)

Requirements:

- macOS 11+
- CMake 3.20+
- Xcode Command Line Tools
- `pnpm` (used by the build script)

JUCE is fetched automatically by the `fetch-juce` script (or copied from a sibling project if available).

### 1. Install Build Prerequisites

```bash
brew install cmake
npm install -g pnpm
```

### 2. Build the Plugin

```bash
pnpm run build
```

The build copies the `.vst3` bundle into `~/Library/Audio/Plug-Ins/VST3/` automatically and code-signs it ad-hoc for local use.

To build manually:

```bash
pnpm run fetch-juce
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Clean

```bash
pnpm run clean        # remove build/
pnpm run clean:all    # remove build/ and vendor/
```

## DSP Architecture

```
Input → Gain → Adaptive LF → Phase Rotator → 8× Peak Shaver → Limiter → TP Guard → Output
                    ↑               ↑               ↑             ↑
             linked peak      8 all-pass       ERB masking    true-peak
             benefit test     candidates       controller     envelope
```

1. **Gain stage** applies the user's drive setting.
2. **Adaptive Low-Frequency Control** runs five probe bands. At each centre a narrow band-pass (Q = 6) and a wide one (Q = 0.8) track the signal. The linked gain changes only when the resonance is significant and attenuation reduces the linked instantaneous peak used as a limiter-burden proxy.
3. **Adaptive Phase Rotation** continuously runs a sparse bank derived from
   the useful parameter bounds reported by Välimäki et al. A linked 50 ms
   analysis window selects a candidate only above 0.3 dB benefit; a 750 ms
   hold prevents rapid switching, and bypass remains a first-class candidate.
4. **Perceptual Peak Shaver** operates directly in the limiter's 8× domain.
   Peak/RMS crest and peak novelty restrict it to transients, while a smooth
   saturating knee and a hard 1.5 dB reduction budget bound the nonlinear
   error. A 16-band ERB approximation measures the filtered residual against
   the temporally smeared masking energy and continuously controls its depth.
5. **Look-Ahead Limiter** delays the audio by 5 ms and schedules a cosine
   attack only when a new, deeper peak appears; sustained constraints extend
   the schedule in O(1). The release adapts continuously to the measured
   peak/RMS crest factor. After reconstruction, a separate 8× detector and
   2 ms guard use only the attenuation actually required to reach −0.1 dBTP.

## License

MaxOx's source is licensed under GPL-3.0-or-later; see `LICENSE`.

The generated VST binary also includes JUCE, which is used under its AGPLv3/commercial dual-license terms. If you want to publish a closed-source binary, obtain a commercial JUCE license first.
