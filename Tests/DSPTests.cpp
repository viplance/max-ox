#include <JuceHeader.h>
#include "DSP/AdaptiveLowCut.h"
#include "DSP/AdaptivePhaseRotator.h"
#include "DSP/LookAheadLimiter.h"
#include "DSP/PerceptualPeakShaver.h"

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
        passed &= check(truePeakDb <= kCeilingDb + 0.02f,
                        "true peak must remain at or below the public ceiling");
        passed &= check(truePeakDb >= kCeilingDb - 0.1f,
                        "true-peak protection must not waste material headroom");
    }

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
        AdaptivePhaseRotator phaseRotator;
        phaseRotator.prepare(kSampleRate, kBlockSize);
        constexpr int length = 192000;
        std::vector<float> input(length);
        std::vector<float> left(length);
        std::vector<float> right(length);

        for (int i = 0; i < length; ++i) {
            const double phase =
                2.0 * juce::MathConstants<double>::pi * 55.0
                * (double) i / kSampleRate;
            const float sample = 0.35f * (
                std::cos(phase)
                + 0.75f * std::cos(2.0 * phase)
                + 0.50f * std::cos(3.0 * phase)
                + 0.25f * std::cos(4.0 * phase));
            input[(size_t) i] = sample;
            left[(size_t) i] = sample;
            right[(size_t) i] = sample;
        }

        for (int offset = 0; offset < length; offset += kBlockSize) {
            const int samples = std::min(kBlockSize, length - offset);
            juce::AudioBuffer<float> block(2, samples);
            block.copyFrom(0, 0, left.data() + offset, samples);
            block.copyFrom(1, 0, right.data() + offset, samples);
            phaseRotator.process(block);
            std::copy_n(
                block.getReadPointer(0), samples, left.data() + offset);
            std::copy_n(
                block.getReadPointer(1), samples, right.data() + offset);
        }

        float inputPeak = 0.0f;
        float outputPeak = 0.0f;
        double inputEnergy = 0.0;
        double outputEnergy = 0.0;
        float maximumChannelDifference = 0.0f;
        for (int i = length / 2; i < length; ++i) {
            inputPeak = std::max(inputPeak, std::abs(input[(size_t) i]));
            outputPeak = std::max(outputPeak, std::abs(left[(size_t) i]));
            inputEnergy += (double) input[(size_t) i] * input[(size_t) i];
            outputEnergy += (double) left[(size_t) i] * left[(size_t) i];
            maximumChannelDifference = std::max(
                maximumChannelDifference,
                std::abs(left[(size_t) i] - right[(size_t) i]));
        }

        const float measuredReduction = juce::Decibels::gainToDecibels(
            inputPeak / (outputPeak + 1.0e-12f), -48.0f);
        const double energyRatio = outputEnergy / inputEnergy;
        std::cout << "Phase rotator candidate: "
                  << phaseRotator.getSelectedCandidate()
                  << ", measured reduction: " << measuredReduction
                  << " dB\n";

        passed &= check(phaseRotator.getSelectedCandidate() != 0,
                        "phase rotator must engage on a reducible waveform");
        passed &= check(measuredReduction >= 0.25f,
                        "engaged phase rotation must reduce the linked peak");
        passed &= check(std::abs(energyRatio - 1.0) < 0.02,
                        "steady all-pass processing must preserve signal energy");
        passed &= check(maximumChannelDifference < 1.0e-6f,
                        "phase rotation must preserve identical stereo channels");
    }

    {
        AdaptivePhaseRotator phaseRotator;
        phaseRotator.prepare(kSampleRate, kBlockSize);
        constexpr int length = 144000;

        for (int offset = 0; offset < length; offset += kBlockSize) {
            const int samples = std::min(kBlockSize, length - offset);
            juce::AudioBuffer<float> block(2, samples);
            for (int i = 0; i < samples; ++i) {
                const int sampleIndex = offset + i;
                const float sample = 0.5f * std::sin(
                    2.0 * juce::MathConstants<double>::pi * 1000.0
                    * (double) sampleIndex / kSampleRate);
                block.setSample(0, i, sample);
                block.setSample(1, i, sample);
            }
            phaseRotator.process(block);
        }

        passed &= check(phaseRotator.getSelectedCandidate() == 0,
                        "phase rotator must bypass a waveform with no peak benefit");
    }

    {
        AdaptivePhaseRotator phaseRotator;
        phaseRotator.prepare(kSampleRate, kBlockSize);
        bool finite = true;

        for (int offset = 0; offset < 48000; offset += kBlockSize) {
            const int samples = std::min(kBlockSize, 48000 - offset);
            juce::AudioBuffer<float> block(1, samples);
            for (int i = 0; i < samples; ++i) {
                const double phase =
                    2.0 * juce::MathConstants<double>::pi * 90.0
                    * (double) (offset + i) / kSampleRate;
                block.setSample(
                    0, i,
                    0.4f * (std::cos(phase) + 0.6f * std::cos(2.0 * phase)));
            }
            phaseRotator.process(block);
            for (int i = 0; i < samples; ++i)
                finite &= std::isfinite(block.getSample(0, i));
        }

        passed &= check(finite,
                        "phase rotation must remain stable in mono operation");
    }

    {
        PerceptualPeakShaver peakShaver;
        peakShaver.prepare(kSampleRate, 4);
        juce::AudioBuffer<float> transientBuffer(2, 4096);
        transientBuffer.clear();
        for (int position = 64; position < 4096; position += 256) {
            transientBuffer.setSample(0, position, 2.0f);
            transientBuffer.setSample(1, position, 2.0f);
        }

        auto block = juce::dsp::AudioBlock<float>(transientBuffer);
        peakShaver.process(block, 2);

        float maximumChannelDifference = 0.0f;
        for (int i = 0; i < transientBuffer.getNumSamples(); ++i)
            maximumChannelDifference = std::max(
                maximumChannelDifference,
                std::abs(
                    transientBuffer.getSample(0, i)
                    - transientBuffer.getSample(1, i)));

        std::cout << "Peak shaver reduction: "
                  << peakShaver.getReductionDb()
                  << " dB, masking allowance: "
                  << peakShaver.getMaskingAllowance() << '\n';
        passed &= check(
            peakShaver.getReductionDb() >= 0.5f
                && peakShaver.getReductionDb() <= 1.51f,
            "transient peak shaving must stay inside the 0.5-1.5 dB budget");
        passed &= check(peakShaver.getMaskingAllowance() < 1.0f,
                        "audible residual energy must reduce masking allowance");
        passed &= check(maximumChannelDifference < 1.0e-6f,
                        "peak shaver must preserve identical stereo channels");
    }

    {
        PerceptualPeakShaver peakShaver;
        peakShaver.prepare(kSampleRate, 4);
        constexpr int samples = 32768;
        juce::AudioBuffer<float> sustained(2, samples);
        std::vector<float> reference((size_t) samples);

        for (int i = 0; i < samples; ++i) {
            const float sample = 1.2f * std::sin(
                2.0 * juce::MathConstants<double>::pi * 1000.0
                * (double) i / (kSampleRate * 4.0));
            reference[(size_t) i] = sample;
            sustained.setSample(0, i, sample);
            sustained.setSample(1, i, sample);
        }

        auto block = juce::dsp::AudioBlock<float>(sustained);
        peakShaver.process(block, 2);

        float settledDifference = 0.0f;
        for (int i = samples / 2; i < samples; ++i)
            settledDifference = std::max(
                settledDifference,
                std::abs(
                    sustained.getSample(0, i) - reference[(size_t) i]));

        passed &= check(settledDifference < 1.0e-5f,
                        "sustained tonal material must bypass transient shaving");
    }

    {
        PerceptualPeakShaver linkedShaver;
        PerceptualPeakShaver sideShaver;
        linkedShaver.prepare(kSampleRate, 4);
        sideShaver.prepare(kSampleRate, 4);
        juce::AudioBuffer<float> linked(2, 512);
        juce::AudioBuffer<float> side(2, 512);
        linked.clear();
        side.clear();
        linked.setSample(0, 32, 2.0f);
        linked.setSample(1, 32, 2.0f);
        side.setSample(0, 32, 2.0f);
        side.setSample(1, 32, -2.0f);

        auto linkedBlock = juce::dsp::AudioBlock<float>(linked);
        auto sideBlock = juce::dsp::AudioBlock<float>(side);
        linkedShaver.process(linkedBlock, 2);
        sideShaver.process(sideBlock, 2);

        passed &= check(
            sideShaver.getReductionDb() < linkedShaver.getReductionDb(),
            "side-dominant peaks must receive more conservative shaving");
    }

    {
        std::cout << "\n=== Gain anomaly diagnostic ===\n";

        struct TestSignal {
            const char* name;
            Stereo data;
        };

        constexpr int length = 192000;
        const double pi2 = 2.0 * juce::MathConstants<double>::pi;
        std::vector<TestSignal> signals;

        {
            Stereo s { std::vector<float>(length), std::vector<float>(length) };
            for (int i = 0; i < length; ++i) {
                const double t = (double) i / kSampleRate;
                const float v = 2.5f * std::sin(pi2 * 1000.0 * t);
                s[0][(size_t) i] = v;
                s[1][(size_t) i] = v;
            }
            signals.push_back({ "mono-1kHz-loud", std::move(s) });
        }
        {
            Stereo s { std::vector<float>(length), std::vector<float>(length) };
            for (int i = 0; i < length; ++i) {
                const double t = (double) i / kSampleRate;
                const float v = 2.0f * (
                    0.6f * std::sin(pi2 * 100.0 * t)
                    + 0.5f * std::sin(pi2 * 200.0 * t + 0.3)
                    + 0.35f * std::sin(pi2 * 500.0 * t + 1.1)
                    + 0.25f * std::sin(pi2 * 1000.0 * t + 0.7)
                    + 0.15f * std::sin(pi2 * 3000.0 * t + 2.0));
                s[0][(size_t) i] = v;
                s[1][(size_t) i] = v;
            }
            signals.push_back({ "mono-multiband", std::move(s) });
        }
        {
            Stereo s { std::vector<float>(length), std::vector<float>(length) };
            for (int i = 0; i < length; ++i) {
                const double t = (double) i / kSampleRate;
                s[0][(size_t) i] = 2.0f * std::sin(pi2 * 300.0 * t);
                s[1][(size_t) i] = 2.0f * std::sin(pi2 * 300.0 * t + 1.2);
            }
            signals.push_back({ "stereo-phase-offset", std::move(s) });
        }
        {
            Stereo s { std::vector<float>(length), std::vector<float>(length) };
            for (int i = 0; i < length; ++i) {
                const double t = (double) i / kSampleRate;
                const float mid = 2.0f * std::sin(pi2 * 200.0 * t);
                const float side = 0.8f * std::sin(pi2 * 5000.0 * t);
                s[0][(size_t) i] = mid + side;
                s[1][(size_t) i] = mid - side;
            }
            signals.push_back({ "stereo-mid-side", std::move(s) });
        }
        {
            Stereo s { std::vector<float>(length), std::vector<float>(length) };
            for (int i = 0; i < length; ++i) {
                const double t = (double) i / kSampleRate;
                float v = 0.35f * (
                    std::cos(pi2 * 80.0 * t)
                    + 0.75f * std::cos(2.0 * pi2 * 80.0 * t)
                    + 0.50f * std::cos(3.0 * pi2 * 80.0 * t)
                    + 0.25f * std::cos(4.0 * pi2 * 80.0 * t));
                s[0][(size_t) i] = v;
                s[1][(size_t) i] = v;
            }
            signals.push_back({ "asymmetric-80Hz", std::move(s) });
        }
        {
            Stereo s { std::vector<float>(length), std::vector<float>(length) };
            for (int i = 0; i < length; ++i) {
                const double t = (double) i / kSampleRate;
                float v = 1.5f * std::sin(pi2 * 400.0 * t);
                if (i % 9600 < 480)
                    v += 3.0f * std::sin(pi2 * 4000.0 * t)
                       * (1.0f - (float)(i % 9600) / 480.0f);
                s[0][(size_t) i] = v;
                s[1][(size_t) i] = v;
            }
            signals.push_back({ "transient-bursts", std::move(s) });
        }
        {
            Stereo s { std::vector<float>(length), std::vector<float>(length) };
            for (int i = 0; i < length; ++i) {
                const double t = (double) i / kSampleRate;
                const float carrier = std::sin(pi2 * 100.0 * t);
                const float tremolo = 0.5f + 0.5f * std::sin(pi2 * 6.0 * t);
                s[0][(size_t) i] = 3.0f * carrier * tremolo;
                s[1][(size_t) i] = 3.0f * carrier * tremolo;
            }
            signals.push_back({ "amplitude-modulated", std::move(s) });
        }

        constexpr int windowLen = 480;

        for (auto& sig : signals) {
            LookAheadLimiter limiter;
            limiter.prepare(kSampleRate, kBlockSize);
            const auto output = render(sig.data, limiter);
            const int latency = limiter.getLatencySamples();

            const int settledBeg = latency + (int)(kSampleRate * 0.15);
            const int analysisEnd = length - windowLen;

            std::vector<float> windowRmsOut;
            std::vector<float> windowRmsIn;
            for (int pos = settledBeg; pos + windowLen <= analysisEnd;
                 pos += windowLen) {
                double rmsIn = 0.0;
                double rmsOut = 0.0;
                for (int j = pos; j < pos + windowLen; ++j) {
                    const int inIdx = j - latency;
                    if (inIdx >= 0 && inIdx < length) {
                        for (int ch = 0; ch < 2; ++ch) {
                            rmsIn += (double) sig.data[(size_t) ch][(size_t) inIdx]
                                   * sig.data[(size_t) ch][(size_t) inIdx];
                            rmsOut += (double) output[(size_t) ch][(size_t) j]
                                    * output[(size_t) ch][(size_t) j];
                        }
                    }
                }
                windowRmsIn.push_back((float) std::sqrt(rmsIn / (windowLen * 2)));
                windowRmsOut.push_back((float) std::sqrt(rmsOut / (windowLen * 2)));
            }

            float maxGainUp = 0.0f;
            int maxGainUpIdx = -1;
            float maxGainJerk = 0.0f;
            int maxGainJerkIdx = -1;

            for (size_t k = 0; k < windowRmsOut.size(); ++k) {
                if (windowRmsIn[k] < 0.01f)
                    continue;
                const float gain = windowRmsOut[k] / windowRmsIn[k];
                if (gain > 1.0f + maxGainUp) {
                    maxGainUp = gain - 1.0f;
                    maxGainUpIdx = (int) k;
                }
            }

            for (size_t k = 1; k < windowRmsOut.size(); ++k) {
                if (windowRmsOut[k] < 0.01f || windowRmsOut[k - 1] < 0.01f)
                    continue;
                if (windowRmsIn[k] < 0.01f || windowRmsIn[k - 1] < 0.01f)
                    continue;
                const float g0 = windowRmsOut[k - 1] / windowRmsIn[k - 1];
                const float g1 = windowRmsOut[k] / windowRmsIn[k];
                const float ratio = g1 > g0 ? g1 / g0 : g0 / g1;
                const float jerkDb =
                    juce::Decibels::gainToDecibels(ratio, 0.0f);
                if (jerkDb > maxGainJerk) {
                    maxGainJerk = jerkDb;
                    maxGainJerkIdx = (int) k;
                }
            }

            const float gainUpDb = 20.0f * std::log10(1.0f + maxGainUp);
            std::cout << "  " << sig.name
                      << ": gain-up=" << gainUpDb << " dB"
                      << " jerk=" << maxGainJerk << " dB";
            if (maxGainUpIdx >= 0)
                std::cout << " (up@w" << maxGainUpIdx << ")";
            if (maxGainJerkIdx >= 0)
                std::cout << " (jerk@w" << maxGainJerkIdx << ")";
            std::cout << "\n";

            passed &= check(gainUpDb <= 0.5f,
                            (std::string(sig.name)
                             + ": limiter must not increase RMS").c_str());
            passed &= check(maxGainJerk <= 3.0f,
                            (std::string(sig.name)
                             + ": gain must not jerk >3 dB between windows").c_str());
        }
    }

    if (! passed)
        return 1;

    std::cout << "All MaxOx DSP tests passed\n";
    return 0;
}
