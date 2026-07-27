#pragma once

#include <JuceHeader.h>
#include <array>

class AdaptiveLowCut
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(float* L, float* R, int numSamples, int numChannels);

    float getLowCutActivity() const { return activity.load(std::memory_order_relaxed); }

private:
    static constexpr int kMaxChans = 2;
    static constexpr int kBands = 5;
    static constexpr float kCentres[kBands] = { 30.0f, 55.0f, 90.0f, 150.0f, 250.0f };
    static constexpr float kNarrowQ = 6.0f;
    static constexpr float kWideQ = 0.8f;
    static constexpr float kPeakThreshold = 0.72f;
    static constexpr float kBandMaxReduction[kBands] = {
        0.45f, 0.32f, 0.23f, 0.15f, 0.08f
    };
    static constexpr int kInfrasonicStages = 2;
    static constexpr float kInfrasonicCutoffHz = 30.0f;
    static constexpr float kButterworthQ[kInfrasonicStages] = {
        0.5411961f, 1.3065630f
    };

    double currentSampleRate = 44100.0;

    std::array<
        std::array<juce::dsp::IIR::Filter<float>, kInfrasonicStages>,
        kMaxChans> infrasonicFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, kBands>, kMaxChans> narrowFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, kBands>, kMaxChans> wideFilters;

    std::array<std::array<float, kBands>, kMaxChans> narrowEnv {};
    std::array<std::array<float, kBands>, kMaxChans> wideEnv {};
    std::array<float, kBands> bandGain {};

    float envAlpha = 0.0f;
    float gainAttackAlpha = 0.0f;
    float gainReleaseAlpha = 0.0f;

    std::array<float, kMaxChans> fullBandRmsEnv {};
    float fullBandRmsAlpha = 0.0f;

    std::atomic<float> activity { 0.0f };
};
