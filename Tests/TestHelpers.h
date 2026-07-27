#pragma once

#include <JuceHeader.h>
#include "DSP/LookAheadLimiter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace test {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;
constexpr float kCeilingDb = -0.1f;

using Stereo = std::array<std::vector<float>, 2>;

inline Stereo render(const Stereo& input, LookAheadLimiter& limiter)
{
    Stereo output {
        std::vector<float>(input[0].size(), 0.0f),
        std::vector<float>(input[1].size(), 0.0f)
    };

    for (size_t offset = 0; offset < input[0].size(); offset += kBlockSize) {
        const int samples = (int) std::min<size_t>(
            (size_t) kBlockSize, input[0].size() - offset);
        juce::AudioBuffer<float> block(2, samples);

        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom(ch, 0, input[(size_t) ch].data() + offset, samples);

        limiter.process(block);

        for (int ch = 0; ch < 2; ++ch)
            std::copy_n(block.getReadPointer(ch), samples,
                        output[(size_t) ch].data() + offset);
    }

    return output;
}

inline float measureTruePeak(const Stereo& signal)
{
    juce::dsp::Oversampling<float> meter(
        2, 3, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true, true);
    meter.initProcessing(kBlockSize);

    float peak = 0.0f;
    for (size_t offset = 0; offset < signal[0].size(); offset += kBlockSize) {
        const int samples = (int) std::min<size_t>(
            (size_t) kBlockSize, signal[0].size() - offset);
        juce::AudioBuffer<float> block(2, samples);

        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom(ch, 0, signal[(size_t) ch].data() + offset, samples);

        const auto oversampled =
            meter.processSamplesUp(juce::dsp::AudioBlock<const float>(block));
        for (size_t ch = 0; ch < oversampled.getNumChannels(); ++ch)
            for (size_t i = 0; i < oversampled.getNumSamples(); ++i)
                peak = std::max(peak, std::abs(oversampled.getSample(ch, i)));
    }

    return peak;
}

inline bool check(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

}
