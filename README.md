# MaxOx

A transparent VST3 maximizer plugin for macOS on Apple Silicon (M-series) with adaptive low-frequency management and an analog hardware-style UI.

## Features

- **Look-Ahead Limiter**: 5 ms look-ahead limiter with 4× oversampled
  inter-sample peak detection, soft-knee transition, cosine-windowed gain
  smoothing, and host-reported latency. Ceiling at −0.1 dBTP.
- **Adaptive Release**: Crest-factor-based release time — fast on sharp transients, slow on sustained material — preserves punch without pumping.
- **Adaptive Low-Frequency Cut**: 5-band peakiness detector (30 / 55 / 90 / 150 / 250 Hz) compares narrow vs. wide band-pass energy around each centre. Surgically notches resonant low-end build-up while leaving musical bass untouched, inspired by the LF resonance suppression in [CanaryVoiceTune](https://github.com/viplance/canary-voice-tune).
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
Input → Gain (+0…+24 dB) → Adaptive LF Cut → Look-Ahead Limiter → Output
                                  ↑                    ↑
                          5-band peakiness     5 ms delay line
                          detector (narrow     soft-knee with
                          vs. wide BP ratio)   adaptive release
```

1. **Gain stage** applies the user's drive setting.
2. **Adaptive Low-Frequency Cut** runs five probe bands. At each centre a narrow band-pass (Q = 6) and a wide one (Q = 0.8) track the signal. When narrow/wide energy exceeds the peakiness threshold, the band is a resonance and gets notched; broad musical content stays below threshold and passes through.
3. **Look-Ahead Limiter** delays the audio by 5 ms, scans ahead for peaks, and applies cosine-windowed gain reduction before the peak arrives. Release time adapts to the signal's crest factor.

## License

MaxOx's source is licensed under GPL-3.0-or-later; see `LICENSE`.

The generated VST binary also includes JUCE, which is used under its AGPLv3/commercial dual-license terms. If you want to publish a closed-source binary, obtain a commercial JUCE license first.
