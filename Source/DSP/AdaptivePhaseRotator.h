#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class AdaptivePhaseRotator
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    float getPeakReductionDb() const
    {
        return peakReductionDb.load(std::memory_order_relaxed);
    }

    int getSelectedCandidate() const
    {
        return selectedCandidate.load(std::memory_order_relaxed);
    }

private:
    static constexpr int kMaxChans = 2;
    static constexpr int kStages = 4;
    static constexpr int kCandidates = 8;
    static constexpr float kMinimumBenefitDb = 0.3f;
    static constexpr float kSwitchAdvantageDb = 0.15f;
    static constexpr float kAnalysisMs = 50.0f;
    static constexpr float kCrossfadeMs = 20.0f;
    static constexpr float kMinimumHoldMs = 750.0f;

    struct AllPassStage
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float x1 = 0.0f;
        float x2 = 0.0f;
        float y1 = 0.0f;
        float y2 = 0.0f;

        float process(float input);
        void reset();
    };

    struct Candidate
    {
        float frequencyHz = 0.0f;
        float radius = 0.0f;
        std::array<std::array<AllPassStage, kStages>, kMaxChans> filters;
    };

    void configureCandidate(Candidate& candidate, float frequencyHz, float radius);
    void evaluateWindow();
    void beginTransition(int candidate);

    double currentSampleRate = 48000.0;
    int analysisSamples = 0;
    int crossfadeSamples = 0;
    int minimumHoldSamples = 0;

    std::array<Candidate, kCandidates> candidates;
    std::array<float, kCandidates> candidateWindowPeaks {};
    float dryWindowPeak = 0.0f;
    int samplesInWindow = 0;
    int samplesSinceSwitch = 0;

    int currentCandidate = 0;
    int transitionFrom = 0;
    int transitionTo = 0;
    int transitionPosition = 0;
    bool transitioning = false;

    std::atomic<float> peakReductionDb { 0.0f };
    std::atomic<int> selectedCandidate { 0 };
};
