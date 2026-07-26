#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

class LookAheadLimiter
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    float getGainReductionDb() const { return gainReductionDb.load(std::memory_order_relaxed); }

private:
    static constexpr int kMaxChans = 2;
    static constexpr float kCeilingDb = -0.1f;
    static constexpr float kLookAheadMs = 5.0f;
    static constexpr float kReleaseMs = 100.0f;
    static constexpr float kFastReleaseMs = 25.0f;
    static constexpr float kSlowReleaseMs = 400.0f;
    static constexpr float kKneeDb = 3.0f;
    static constexpr int kOversampleFactor = 4;

    float computeGain(float peakDb) const;

    double currentSampleRate = 44100.0;
    int lookAheadSamples = 0;
    float ceiling = 1.0f;

    float releaseCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;
    float slowReleaseCoeff = 0.0f;

    std::array<std::vector<float>, kMaxChans> delayBuffer;
    int delayWritePos = 0;

    std::vector<float> gainEnvelope;
    int gainWritePos = 0;

    float currentGain = 1.0f;

    juce::dsp::Oversampling<float> oversampler { kMaxChans, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };

    std::atomic<float> gainReductionDb { 0.0f };
};
