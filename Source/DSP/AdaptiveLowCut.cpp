#include "AdaptiveLowCut.h"
#include <cmath>

void AdaptiveLowCut::prepare(double sampleRate, [[maybe_unused]] int maxBlockSize)
{
    currentSampleRate = sampleRate;
    lookaheadSamples = (int) std::round(sampleRate * kLookaheadMs / 1000.0);

    envAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.012));
    gainAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * kReleaseMs / 1000.0));
    fullBandRmsAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.050));

    for (int ch = 0; ch < kMaxChans; ++ch) {
        delayBuffer[(size_t) ch].assign((size_t) lookaheadSamples, 0.0f);

        for (int stage = 0; stage < kInfrasonicStages; ++stage)
            infrasonicFilters[(size_t) ch][(size_t) stage].coefficients =
                juce::dsp::IIR::Coefficients<float>::makeHighPass(
                    sampleRate,
                    kInfrasonicCutoffHz,
                    kButterworthQ[stage]);

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
        for (auto& filter : infrasonicFilters[(size_t) ch])
            filter.reset();
        std::fill(delayBuffer[(size_t) ch].begin(),
                  delayBuffer[(size_t) ch].end(), 0.0f);
        fullBandRmsEnv[(size_t) ch] = 0.0f;

        for (int b = 0; b < kBands; ++b) {
            narrowFilters[(size_t)ch][(size_t)b].reset();
            wideFilters[(size_t)ch][(size_t)b].reset();
            narrowEnv[(size_t)ch][(size_t)b] = 0.0f;
            wideEnv[(size_t)ch][(size_t)b] = 0.0f;
        }
    }
    delayWritePos = 0;
    bandGain.fill(1.0f);
    activity.store(0.0f, std::memory_order_relaxed);
}

void AdaptiveLowCut::process(float* L, float* R, int numSamples, int numChannels)
{
    float maxActivity = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        std::array<float, kMaxChans> x {
            L[i],
            numChannels > 1 ? R[i] : 0.0f
        };

        // Infrasonic HP on live signal
        for (int ch = 0; ch < numChannels; ++ch)
            for (auto& filter : infrasonicFilters[(size_t) ch])
                x[(size_t) ch] = filter.processSample(x[(size_t) ch]);

        // Read delayed sample, write current into delay line
        const int readPos = delayWritePos;
        std::array<float, kMaxChans> delayed {};
        for (int ch = 0; ch < numChannels; ++ch) {
            delayed[(size_t) ch] = delayBuffer[(size_t) ch][(size_t) readPos];
            delayBuffer[(size_t) ch][(size_t) readPos] = x[(size_t) ch];
        }
        delayWritePos = (delayWritePos + 1) % lookaheadSamples;

        // Analyse the live (future) signal
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

        // Compute gain from live analysis
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

            const float proposedReduction =
                kBandMaxReduction[b] * resonance * energyWeight;

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

            bandGain[(size_t) b] +=
                gainAlpha * (targetGain - bandGain[(size_t) b]);

            const float appliedReduction = 1.0f - bandGain[(size_t) b];

            // Apply reduction to the DELAYED signal
            for (int ch = 0; ch < numChannels; ++ch)
                delayed[(size_t) ch] -=
                    appliedReduction * narrowSample[(size_t) ch][(size_t) b];

            maxActivity = juce::jmax(maxActivity, appliedReduction);
        }

        L[i] = delayed[0];
        if (numChannels > 1)
            R[i] = delayed[1];
    }

    activity.store(maxActivity, std::memory_order_relaxed);
}
