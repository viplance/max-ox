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
    static constexpr float kMaxReduction = 0.90f;

    double currentSampleRate = 44100.0;

    std::array<std::array<juce::dsp::IIR::Filter<float>, kBands>, kMaxChans> narrowFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, kBands>, kMaxChans> wideFilters;

    std::array<std::array<float, kBands>, kMaxChans> narrowEnv {};
    std::array<std::array<float, kBands>, kMaxChans> wideEnv {};
    std::array<std::array<float, kBands>, kMaxChans> bandGain {};

    float envAlpha = 0.0f;
    float gainAlpha = 0.0f;

    float fullBandRmsEnv = 0.0f;
    float fullBandRmsAlpha = 0.0f;

    std::atomic<float> activity { 0.0f };
};
