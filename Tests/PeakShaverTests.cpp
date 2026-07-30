#include "TestHelpers.h"
#include "DSP/PerceptualPeakShaver.h"

bool runPeakShaverTests()
{
    using namespace test;
    bool passed = true;

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

        std::cout << "  peak shaver reduction: "
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

    return passed;
}
