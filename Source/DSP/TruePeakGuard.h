#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class TruePeakGuard
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    int getLatencySamples() const { return delaySamples; }
    float getGainReductionDb() const
    {
        return gainReductionDb.load(std::memory_order_relaxed);
    }

private:
    static constexpr int kMaxChans = 2;
    static constexpr float kCeilingDb = -0.11f;
    static constexpr float kAttackMs = 2.0f;
    static constexpr float kReleaseMs = 60.0f;

    juce::dsp::Oversampling<float> detector { kMaxChans, 3,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    std::array<std::vector<float>, kMaxChans> delayBuffer;
    std::vector<float> gainSchedule;
    std::vector<float> attackWindow;
    int delayWritePos = 0;
    int scheduleReadPos = 0;
    int delaySamples = 0;
    int attackSamples = 0;
    int samplesSinceFullSchedule = 0;
    float lastScheduledTarget = 1.0f;
    float currentGain = 1.0f;
    float releaseCoeff = 0.0f;
    float ceiling = 1.0f;

    std::atomic<float> gainReductionDb { 0.0f };
};
