#include "TestHelpers.h"

bool runLimiterTests()
{
    using namespace test;
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

        std::cout << "  true peak: " << truePeakDb << " dBTP\n";
        passed &= check(truePeakDb <= kCeilingDb + 0.02f,
                        "true peak must remain at or below the public ceiling");
        passed &= check(truePeakDb >= kCeilingDb - 0.1f,
                        "true-peak protection must not waste material headroom");
    }

    return passed;
}
