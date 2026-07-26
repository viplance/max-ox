#include "AdaptivePhaseRotator.h"

#include <cmath>

float AdaptivePhaseRotator::AllPassStage::process(float input)
{
    const float output =
        b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    return output;
}

void AdaptivePhaseRotator::AllPassStage::reset()
{
    x1 = 0.0f;
    x2 = 0.0f;
    y1 = 0.0f;
    y2 = 0.0f;
}

void AdaptivePhaseRotator::configureCandidate(
    Candidate& candidate, float frequencyHz, float radius)
{
    candidate.frequencyHz = frequencyHz;
    candidate.radius = radius;

    const float omega =
        juce::MathConstants<float>::twoPi
        * frequencyHz / (float) currentSampleRate;
    const float cosine = std::cos(omega);
    const float b0 = radius * radius;
    const float b1 = -2.0f * radius * cosine;
    const float b2 = 1.0f;
    const float a1 = b1;
    const float a2 = b0;

    for (auto& channel : candidate.filters) {
        for (auto& stage : channel) {
            stage.b0 = b0;
            stage.b1 = b1;
            stage.b2 = b2;
            stage.a1 = a1;
            stage.a2 = a2;
            stage.reset();
        }
    }
}

void AdaptivePhaseRotator::prepare(
    double sampleRate, [[maybe_unused]] int maxBlockSize)
{
    currentSampleRate = sampleRate;
    analysisSamples =
        juce::jmax(1, (int) std::lround(sampleRate * kAnalysisMs / 1000.0));
    crossfadeSamples =
        juce::jmax(1, (int) std::lround(sampleRate * kCrossfadeMs / 1000.0));
    minimumHoldSamples =
        juce::jmax(1, (int) std::lround(sampleRate * kMinimumHoldMs / 1000.0));

    // The paper limits useful, ultra-short configurations to 40–200 Hz and
    // pole radii 0.6–0.98. This sparse bank covers its reported optima while
    // keeping continuous real-time analysis practical.
    constexpr std::array<std::array<float, 2>, kCandidates> configurations {{
        {{ 40.0f, 0.98f }},
        {{ 80.0f, 0.98f }},
        {{ 80.0f, 0.80f }},
        {{ 120.0f, 0.90f }},
        {{ 160.0f, 0.65f }},
        {{ 160.0f, 0.90f }},
        {{ 200.0f, 0.80f }},
        {{ 200.0f, 0.95f }}
    }};

    for (size_t i = 0; i < candidates.size(); ++i)
        configureCandidate(
            candidates[i], configurations[i][0], configurations[i][1]);

    reset();
}

void AdaptivePhaseRotator::reset()
{
    for (auto& candidate : candidates)
        for (auto& channel : candidate.filters)
            for (auto& stage : channel)
                stage.reset();

    candidateWindowPeaks.fill(0.0f);
    dryWindowPeak = 0.0f;
    samplesInWindow = 0;
    samplesSinceSwitch = minimumHoldSamples;
    currentCandidate = 0;
    transitionFrom = 0;
    transitionTo = 0;
    transitionPosition = 0;
    transitioning = false;
    peakReductionDb.store(0.0f, std::memory_order_relaxed);
    selectedCandidate.store(0, std::memory_order_relaxed);
}

void AdaptivePhaseRotator::beginTransition(int candidate)
{
    if (candidate == currentCandidate || transitioning)
        return;

    transitionFrom = currentCandidate;
    transitionTo = candidate;
    transitionPosition = 0;
    transitioning = true;
}

void AdaptivePhaseRotator::evaluateWindow()
{
    const float floor = juce::Decibels::decibelsToGain(-60.0f);
    int bestCandidate = 0;
    float bestReduction = 0.0f;
    float currentReduction = 0.0f;

    if (dryWindowPeak > floor) {
        for (int i = 0; i < kCandidates; ++i) {
            const float reduction = juce::Decibels::gainToDecibels(
                dryWindowPeak / (candidateWindowPeaks[(size_t) i] + 1.0e-12f),
                -48.0f);
            if (reduction > bestReduction) {
                bestReduction = reduction;
                bestCandidate = i + 1;
            }
            if (currentCandidate == i + 1)
                currentReduction = reduction;
        }
    }

    peakReductionDb.store(
        juce::jmax(0.0f, bestReduction), std::memory_order_relaxed);

    if (! transitioning && samplesSinceSwitch >= minimumHoldSamples) {
        if (currentCandidate != 0 && currentReduction < kMinimumBenefitDb) {
            beginTransition(0);
        } else if (bestReduction >= kMinimumBenefitDb
                   && bestCandidate != currentCandidate
                   && (currentCandidate == 0
                       || bestReduction
                           >= currentReduction + kSwitchAdvantageDb)) {
            beginTransition(bestCandidate);
        }
    }

    candidateWindowPeaks.fill(0.0f);
    dryWindowPeak = 0.0f;
    samplesInWindow = 0;
}

void AdaptivePhaseRotator::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), kMaxChans);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i) {
        std::array<float, kMaxChans> dry {};
        std::array<std::array<float, kMaxChans>, kCandidates> wet {};

        for (int ch = 0; ch < numChannels; ++ch) {
            dry[(size_t) ch] = buffer.getSample(ch, i);
            dryWindowPeak =
                juce::jmax(dryWindowPeak, std::abs(dry[(size_t) ch]));
        }

        for (int candidateIndex = 0;
             candidateIndex < kCandidates;
             ++candidateIndex) {
            auto& candidate = candidates[(size_t) candidateIndex];
            for (int ch = 0; ch < numChannels; ++ch) {
                float sample = dry[(size_t) ch];
                for (auto& stage : candidate.filters[(size_t) ch])
                    sample = stage.process(sample);
                wet[(size_t) candidateIndex][(size_t) ch] = sample;
                candidateWindowPeaks[(size_t) candidateIndex] =
                    juce::jmax(
                        candidateWindowPeaks[(size_t) candidateIndex],
                        std::abs(sample));
            }
        }

        float mix = 0.0f;
        if (transitioning) {
            const float position =
                (float) transitionPosition / (float) crossfadeSamples;
            mix = 0.5f - 0.5f * std::cos(
                juce::jlimit(0.0f, 1.0f, position)
                * juce::MathConstants<float>::pi);
        }

        for (int ch = 0; ch < numChannels; ++ch) {
            const auto sampleFor = [&](int selection) {
                return selection == 0
                    ? dry[(size_t) ch]
                    : wet[(size_t) (selection - 1)][(size_t) ch];
            };

            const float output = transitioning
                ? sampleFor(transitionFrom)
                    + mix * (sampleFor(transitionTo) - sampleFor(transitionFrom))
                : sampleFor(currentCandidate);
            buffer.setSample(ch, i, output);
        }

        if (transitioning) {
            ++transitionPosition;
            if (transitionPosition >= crossfadeSamples) {
                currentCandidate = transitionTo;
                transitioning = false;
                samplesSinceSwitch = 0;
                selectedCandidate.store(
                    currentCandidate, std::memory_order_relaxed);
            }
        } else {
            ++samplesSinceSwitch;
        }

        ++samplesInWindow;
        if (samplesInWindow >= analysisSamples)
            evaluateWindow();
    }
}
