#include "AdaptiveLowCut.h"
#include <cmath>

void AdaptiveLowCut::prepare(double sampleRate, [[maybe_unused]] int maxBlockSize)
{
    currentSampleRate = sampleRate;

    envAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.012));
    gainAttackAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.020));
    gainReleaseAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.080));
    fullBandRmsAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.050));

    for (int ch = 0; ch < kMaxChans; ++ch) {
        infrasonicFilters[(size_t) ch].coefficients =
            juce::dsp::IIR::Coefficients<float>::makeHighPass(
                sampleRate, kInfrasonicCutoffHz);

        for (int b = 0; b < kBands; ++b) {
            auto narrowCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
                sampleRate, kCentres[b], kNarrowQ);
            auto wideCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
                sampleRate, kCentres[b], kWideQ);

            narrowFilters[(size_t)ch][(size_t)b].coefficients = narrowCoeffs;
            wideFilters[(size_t)ch][(size_t)b].coefficients = wideCoeffs;
        }
    }

    reset();
}

void AdaptiveLowCut::reset()
{
    for (int ch = 0; ch < kMaxChans; ++ch) {
        infrasonicFilters[(size_t) ch].reset();
        fullBandRmsEnv[(size_t) ch] = 0.0f;

        for (int b = 0; b < kBands; ++b) {
            narrowFilters[(size_t)ch][(size_t)b].reset();
            wideFilters[(size_t)ch][(size_t)b].reset();
            narrowEnv[(size_t)ch][(size_t)b] = 0.0f;
            wideEnv[(size_t)ch][(size_t)b] = 0.0f;
        }
    }
    bandGain.fill(1.0f);
    activity.store(0.0f, std::memory_order_relaxed);
}

void AdaptiveLowCut::process(float* L, float* R, int numSamples, int numChannels)
{
    float maxActivity = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        std::array<float, kMaxChans> x {
            infrasonicFilters[0].processSample(L[i]),
            numChannels > 1 ? infrasonicFilters[1].processSample(R[i]) : 0.0f
        };
        std::array<std::array<float, kBands>, kMaxChans> narrowSample {};

        for (int ch = 0; ch < numChannels; ++ch) {
            fullBandRmsEnv[(size_t) ch] += fullBandRmsAlpha
                * (x[(size_t) ch] * x[(size_t) ch]
                   - fullBandRmsEnv[(size_t) ch]);

            for (int b = 0; b < kBands; ++b) {
                const float n =
                    narrowFilters[(size_t) ch][(size_t) b]
                        .processSample(x[(size_t) ch]);
                const float w =
                    wideFilters[(size_t) ch][(size_t) b]
                        .processSample(x[(size_t) ch]);
                narrowSample[(size_t) ch][(size_t) b] = n;

                narrowEnv[(size_t) ch][(size_t) b] += envAlpha
                    * (std::abs(n) - narrowEnv[(size_t) ch][(size_t) b]);
                wideEnv[(size_t) ch][(size_t) b] += envAlpha
                    * (std::abs(w) - wideEnv[(size_t) ch][(size_t) b]);
            }
        }

        for (int b = 0; b < kBands; ++b) {
            float resonance = 0.0f;
            float energyWeight = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch) {
                const float peakiness =
                    narrowEnv[(size_t) ch][(size_t) b]
                    / (wideEnv[(size_t) ch][(size_t) b] + 1.0e-8f);
                resonance = juce::jmax(
                    resonance,
                    juce::jlimit(
                        0.0f, 1.0f,
                        (peakiness - kPeakThreshold)
                            / (1.0f - kPeakThreshold)));

                const float fullRms =
                    std::sqrt(fullBandRmsEnv[(size_t) ch] + 1.0e-12f);
                const float bandToFull =
                    narrowEnv[(size_t) ch][(size_t) b]
                    / (fullRms + 1.0e-8f);
                energyWeight = juce::jmax(
                    energyWeight,
                    juce::jlimit(0.0f, 1.0f, bandToFull * 2.0f));
            }

            const float frequencyWeight =
                1.0f - (float) b / (float) kBands;
            const float proposedReduction =
                kMaxReduction * resonance * energyWeight * frequencyWeight;

            float originalPeak = 0.0f;
            float candidatePeak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch) {
                originalPeak =
                    juce::jmax(originalPeak, std::abs(x[(size_t) ch]));
                candidatePeak = juce::jmax(
                    candidatePeak,
                    std::abs(
                        x[(size_t) ch]
                        - proposedReduction
                            * narrowSample[(size_t) ch][(size_t) b]));
            }

            const float relativeBenefit = juce::jlimit(
                0.0f, 1.0f,
                (originalPeak - candidatePeak) / (originalPeak + 1.0e-8f));
            const float benefitWeight =
                juce::jlimit(0.0f, 1.0f, relativeBenefit * 8.0f);
            const float targetGain =
                1.0f - proposedReduction * benefitWeight;
            const float alpha =
                targetGain < bandGain[(size_t) b]
                    ? gainAttackAlpha
                    : gainReleaseAlpha;
            bandGain[(size_t) b] +=
                alpha * (targetGain - bandGain[(size_t) b]);

            const float appliedReduction = 1.0f - bandGain[(size_t) b];
            for (int ch = 0; ch < numChannels; ++ch)
                x[(size_t) ch] -=
                    appliedReduction * narrowSample[(size_t) ch][(size_t) b];

            maxActivity = juce::jmax(maxActivity, appliedReduction);
        }

        L[i] = x[0];
        if (numChannels > 1)
            R[i] = x[1];
    }

    activity.store(maxActivity, std::memory_order_relaxed);
}
