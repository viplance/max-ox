#include "TestHelpers.h"
#include "DSP/AdaptiveLowCut.h"

namespace {

float measureSettledGain(float frequency)
{
    constexpr int length = 96000;
    constexpr float amplitude = 0.5f;
    AdaptiveLowCut lowFrequencyControl;
    lowFrequencyControl.prepare(test::kSampleRate, test::kBlockSize);
    std::vector<float> left(length);
    std::vector<float> right(length);

    for (int i = 0; i < length; ++i) {
        const float sample = amplitude * std::sin(
            2.0 * juce::MathConstants<double>::pi * frequency
            * (double) i / test::kSampleRate);
        left[(size_t) i] = sample;
        right[(size_t) i] = sample;
    }

    for (int offset = 0; offset < length; offset += test::kBlockSize) {
        const int samples = std::min(test::kBlockSize, length - offset);
        lowFrequencyControl.process(
            left.data() + offset, right.data() + offset, samples, 2);
    }

    double inputEnergy = 0.0;
    double outputEnergy = 0.0;
    for (int i = length / 2; i < length; ++i) {
        const float input = amplitude * std::sin(
            2.0 * juce::MathConstants<double>::pi * frequency
            * (double) i / test::kSampleRate);
        inputEnergy += (double) input * input;
        outputEnergy += (double) left[(size_t) i] * left[(size_t) i];
    }

    return (float) std::sqrt(outputEnergy / inputEnergy);
}

}

bool runAdaptiveLowCutTests()
{
    using namespace test;
    bool passed = true;

    {
        AdaptiveLowCut lowFrequencyControl;
        lowFrequencyControl.prepare(kSampleRate, kBlockSize);
        constexpr int length = 48000;
        std::vector<float> left(length);
        std::vector<float> right(length);

        for (int i = 0; i < length; ++i) {
            const float sample = 0.8f * std::sin(
                2.0 * juce::MathConstants<double>::pi * 55.0
                * (double) i / kSampleRate);
            left[(size_t) i] = sample;
            right[(size_t) i] = sample;
        }

        for (int offset = 0; offset < length; offset += kBlockSize) {
            const int samples = std::min(kBlockSize, length - offset);
            lowFrequencyControl.process(
                left.data() + offset, right.data() + offset, samples, 2);
        }

        float maximumChannelDifference = 0.0f;
        float settledPeak = 0.0f;
        for (int i = length / 2; i < length; ++i) {
            maximumChannelDifference = std::max(
                maximumChannelDifference,
                std::abs(left[(size_t) i] - right[(size_t) i]));
            settledPeak = std::max(settledPeak, std::abs(left[(size_t) i]));
        }

        passed &= check(maximumChannelDifference < 1.0e-6f,
                        "linked LF control must preserve identical stereo channels");
        passed &= check(settledPeak <= 0.8f,
                        "LF control must not increase a sustained resonant peak");
    }

    {
        const float gain10Hz = measureSettledGain(10.0f);
        const float gain30Hz = measureSettledGain(30.0f);
        const float gain55Hz = measureSettledGain(55.0f);
        const float gain90Hz = measureSettledGain(90.0f);
        const float gain1kHz = measureSettledGain(1000.0f);

        std::cout << "  adaptive low cut gain:"
                  << " 10Hz=" << juce::Decibels::gainToDecibels(gain10Hz)
                  << "dB 30Hz=" << juce::Decibels::gainToDecibels(gain30Hz)
                  << "dB 55Hz=" << juce::Decibels::gainToDecibels(gain55Hz)
                  << "dB 90Hz=" << juce::Decibels::gainToDecibels(gain90Hz)
                  << "dB 1kHz=" << juce::Decibels::gainToDecibels(gain1kHz)
                  << "dB\n";

        passed &= check(gain10Hz < 0.1f,
                        "24 dB/oct infrasonic filter must strongly reject 10 Hz");
        passed &= check(gain30Hz < 0.75f,
                        "infrasonic filter must attenuate its 30 Hz corner");
        passed &= check(gain55Hz > 0.6f,
                        "adaptive control must preserve most content above 50 Hz");
        passed &= check(gain90Hz > 0.7f,
                        "adaptive control must avoid excessive upper-bass reduction");
        passed &= check(gain1kHz > 0.995f,
                        "infrasonic processing must remain transparent at 1 kHz");
    }

    return passed;
}
