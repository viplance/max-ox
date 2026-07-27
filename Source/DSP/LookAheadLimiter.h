#pragma once

#include <JuceHeader.h>
#include "PerceptualPeakShaver.h"
#include "TruePeakGuard.h"
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
    static constexpr float kReleaseMs = 90.0f;
    static constexpr float kFastReleaseMs = 35.0f;
    static constexpr float kSlowReleaseMs = 180.0f;
    static constexpr float kKneeDb = 0.5f;
    static constexpr float kRmsWindowMs = 50.0f;
    static constexpr float kMaxReleaseRateDbPerSec = 80.0f;

    float computeGain(float peakDb) const;

    double currentSampleRate = 44100.0;
    int lookAheadSamples = 0;
    int latencySamples = 0;

    float releaseCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;
    float slowReleaseCoeff = 0.0f;
    float rmsCoeff = 0.0f;
    float rmsEnvelope = 0.0f;
    float smoothedReleaseCoeff = 0.0f;
    float releaseCoeffSmoothing = 0.0f;
    float maxReleasePerSample = 1.0f;

    std::array<std::vector<float>, kMaxChans> delayBuffer;
    int delayWritePos = 0;

    std::vector<float> gainSchedule;
    std::vector<float> attackWindow;
    int scheduleReadPos = 0;
    int samplesSinceFullSchedule = 0;
    float lastScheduledTarget = 1.0f;

    float currentGain = 1.0f;

    PerceptualPeakShaver peakShaver;
    TruePeakGuard truePeakGuard;

    std::atomic<float> gainReductionDb { 0.0f };
};
