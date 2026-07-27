#include "TestHelpers.h"
#include "DSP/AdaptiveLowCut.h"

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

    return passed;
}
