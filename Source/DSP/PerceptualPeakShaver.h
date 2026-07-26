#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class PerceptualPeakShaver
{
public:
    void prepare(double baseSampleRate, int oversamplingFactor);
    void reset();
    void process(juce::dsp::AudioBlock<float>& block, int numChannels);

    float getReductionDb() const
    {
        return reductionDb.load(std::memory_order_relaxed);
    }

    float getMaskingAllowance() const
    {
        return maskingAllowance.load(std::memory_order_relaxed);
    }

private:
    static constexpr int kMaxChans = 2;
    static constexpr int kErbBands = 16;
    static constexpr float kClipStartDb = -0.5f;
    static constexpr float kMinimumShaveDb = 0.5f;
    static constexpr float kMaximumShaveDb = 1.5f;
    static constexpr float kMaskingRatioDb = -18.0f;

    struct Biquad
    {
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;

        float process(float input);
        void reset();
    };

    static float erbRate(float frequencyHz);
    static float inverseErbRate(float rate);
    static void configureBandPass(
        Biquad& filter, double sampleRate, float frequencyHz, float q);
    void analyseMasking(
        const std::array<float, kMaxChans>& source,
        const std::array<float, kMaxChans>& residual,
        int numChannels);

    double analysisSampleRate = 48000.0;
    double processingSampleRate = 192000.0;
    int factor = 4;
    float clipStart = 1.0f;

    std::array<std::array<Biquad, kErbBands>, kMaxChans> sourceFilters;
    std::array<std::array<Biquad, kErbBands>, kMaxChans> residualFilters;
    std::array<std::array<float, kErbBands>, kMaxChans> sourceEnvelope {};
    std::array<std::array<float, kErbBands>, kMaxChans> residualEnvelope {};
    std::array<float, kMaxChans> rmsEnvelope {};
    std::array<float, kMaxChans> slowPeakEnvelope {};

    float sourceAttack = 0.0f;
    float sourceRelease = 0.0f;
    float residualAttack = 0.0f;
    float residualRelease = 0.0f;
    float rmsCoeff = 0.0f;
    float slowPeakCoeff = 0.0f;
    float allowanceAttack = 0.0f;
    float allowanceRelease = 0.0f;
    float allowanceState = 1.0f;

    std::atomic<float> reductionDb { 0.0f };
    std::atomic<float> maskingAllowance { 1.0f };
};
