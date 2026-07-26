#include <JuceHeader.h>
#include "DSP/LookAheadLimiter.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;
constexpr float kCeilingDb = -0.1f;

using Stereo = std::array<std::vector<float>, 2>;

Stereo render(const Stereo& input, LookAheadLimiter& limiter)
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

float measureTruePeak(const Stereo& signal)
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

bool check(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}
}

int main()
{
    bool passed = true;

    {
        LookAheadLimiter limiter;
        limiter.prepare(kSampleRate, kBlockSize);
        passed &= check(limiter.getLatencySamples() >= 240,
                        "reported latency must include the 5 ms look-ahead");

        Stereo silence {
            std::vector<float>(4096, 0.0f),
            std::vector<float>(4096, 0.0f)
        };
        const auto output = render(silence, limiter);
        passed &= check(measureTruePeak(output) == 0.0f,
                        "silence must remain silent");
    }

    {
        LookAheadLimiter limiter;
        limiter.prepare(kSampleRate, kBlockSize);
        const int impulsePosition = 1024;
        Stereo impulse {
            std::vector<float>(4096, 0.0f),
            std::vector<float>(4096, 0.0f)
        };
        impulse[0][(size_t) impulsePosition] = 0.25f;
        impulse[1][(size_t) impulsePosition] = 0.25f;

        const auto output = render(impulse, limiter);
        const auto peakIt = std::max_element(
            output[0].begin(), output[0].end(),
            [](float a, float b) { return std::abs(a) < std::abs(b); });
        const int measuredPosition =
            (int) std::distance(output[0].begin(), peakIt);
        const int expectedPosition = impulsePosition + limiter.getLatencySamples();

        passed &= check(std::abs(measuredPosition - expectedPosition) <= 1,
                        "reported latency must match the measured impulse delay");
    }

    {
        LookAheadLimiter limiter;
        limiter.prepare(kSampleRate, kBlockSize);
        constexpr int length = 96000;
        Stereo driven {
            std::vector<float>(length, 0.0f),
            std::vector<float>(length, 0.0f)
        };

        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            const float sample = 2.8f * (
                0.72f * std::sin(2.0 * juce::MathConstants<double>::pi * 997.0 * t)
                + 0.28f * std::sin(2.0 * juce::MathConstants<double>::pi * 11003.0 * t));
            driven[0][(size_t) i] = sample;
            driven[1][(size_t) i] = sample;
        }

        const auto output = render(driven, limiter);
        const float truePeak = measureTruePeak(output);
        const float truePeakDb =
            juce::Decibels::gainToDecibels(truePeak, -100.0f);

        std::cout << "Measured 8x true peak: " << truePeakDb << " dBTP\n";
        passed &= check(truePeakDb <= kCeilingDb + 0.1f,
                        "true peak must remain within 0.1 dB of the ceiling");
    }

    if (! passed)
        return 1;

    std::cout << "All MaxOx DSP tests passed\n";
    return 0;
}
