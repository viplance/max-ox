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
    int getLatencySamples() const { return latencySamples; }

private:
    static constexpr int kMaxChans = 2;
    static constexpr float kCeilingDb = -0.1f;
    static constexpr float kLookAheadMs = 5.0f;
    static constexpr float kReleaseMs = 100.0f;
    static constexpr float kFastReleaseMs = 25.0f;
    static constexpr float kSlowReleaseMs = 400.0f;
    static constexpr float kKneeDb = 3.0f;
    static constexpr int kOversampleFactor = 4;
    static constexpr float kRmsWindowMs = 50.0f;
    // Downsampling filters can ring above the gain-limited oversampled signal.
    // This guard keeps the reconstructed output at the public dBTP ceiling.
    static constexpr float kReconstructionMarginDb = 0.35f;

    float computeGain(float peakDb) const;

    double currentSampleRate = 44100.0;
    int lookAheadSamples = 0;
    int lookAheadSamplesOversampled = 0;
    int latencySamples = 0;

    float releaseCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;
    float slowReleaseCoeff = 0.0f;
    float rmsCoeff = 0.0f;
    float rmsEnvelope = 0.0f;

    std::array<std::vector<float>, kMaxChans> delayBuffer;
    int delayWritePos = 0;

    std::vector<float> gainEnvelope;
    std::vector<float> attackWindow;
    int gainWritePos = 0;

    float currentGain = 1.0f;

    juce::dsp::Oversampling<float> oversampler { kMaxChans, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    std::atomic<float> gainReductionDb { 0.0f };
};
