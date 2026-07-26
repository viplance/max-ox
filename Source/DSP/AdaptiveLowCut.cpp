#include "AdaptiveLowCut.h"
#include <cmath>

void AdaptiveLowCut::prepare(double sampleRate, [[maybe_unused]] int maxBlockSize)
{
    currentSampleRate = sampleRate;

    envAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.012));
    gainAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.030));
    fullBandRmsAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.050));

    for (int ch = 0; ch < kMaxChans; ++ch) {
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
        for (int b = 0; b < kBands; ++b) {
            narrowFilters[(size_t)ch][(size_t)b].reset();
            wideFilters[(size_t)ch][(size_t)b].reset();
            narrowEnv[(size_t)ch][(size_t)b] = 0.0f;
            wideEnv[(size_t)ch][(size_t)b] = 0.0f;
            bandGain[(size_t)ch][(size_t)b] = 1.0f;
        }
    }
    fullBandRmsEnv = 0.0f;
}

void AdaptiveLowCut::process(float* L, float* R, int numSamples, int numChannels)
{
    float maxActivity = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch) {
        float* dst = (ch == 0) ? L : R;
        auto& nEnv = narrowEnv[(size_t)ch];
        auto& wEnv = wideEnv[(size_t)ch];
        auto& gain = bandGain[(size_t)ch];
        auto& narrow = narrowFilters[(size_t)ch];
        auto& wide = wideFilters[(size_t)ch];

        for (int i = 0; i < numSamples; ++i) {
            const float x = dst[i];

            fullBandRmsEnv += fullBandRmsAlpha * (x * x - fullBandRmsEnv);
            float fullRms = std::sqrt(fullBandRmsEnv + 1e-12f);

            for (int b = 0; b < kBands; ++b) {
                const float n = narrow[(size_t)b].processSample(x);
                const float w = wide[(size_t)b].processSample(x);

                nEnv[(size_t)b] += envAlpha * (std::abs(n) - nEnv[(size_t)b]);
                wEnv[(size_t)b] += envAlpha * (std::abs(w) - wEnv[(size_t)b]);

                float peakiness = nEnv[(size_t)b] / (wEnv[(size_t)b] + 1e-8f);

                float bandToFull = nEnv[(size_t)b] / (fullRms + 1e-8f);
                float energyWeight = juce::jlimit(0.0f, 1.0f, bandToFull * 2.0f);

                float resonance = juce::jlimit(0.0f, 1.0f,
                    (peakiness - kPeakThreshold) / (1.0f - kPeakThreshold));

                float effectiveReduction = kMaxReduction * resonance * energyWeight;

                float freqWeight = 1.0f - (float)b / (float)(kBands);
                effectiveReduction *= freqWeight;

                float targetGain = 1.0f - effectiveReduction;
                gain[(size_t)b] += gainAlpha * (targetGain - gain[(size_t)b]);

                dst[i] -= (1.0f - gain[(size_t)b]) * n;

                maxActivity = juce::jmax(maxActivity, 1.0f - gain[(size_t)b]);
            }
        }
    }

    activity.store(maxActivity, std::memory_order_relaxed);
}
