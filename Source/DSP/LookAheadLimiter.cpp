#include "LookAheadLimiter.h"
#include <cmath>

void LookAheadLimiter::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    lookAheadSamples = (int) std::ceil(sampleRate * kLookAheadMs / 1000.0);
    lookAheadSamplesOversampled = lookAheadSamples * kOversampleFactor;

    for (auto& buf : delayBuffer) {
        buf.resize((size_t) (lookAheadSamplesOversampled
                             + maxBlockSize * kOversampleFactor + 1), 0.0f);
        std::fill(buf.begin(), buf.end(), 0.0f);
    }
    delayWritePos = 0;

    gainEnvelope.resize((size_t) (lookAheadSamples + maxBlockSize + 1), 1.0f);
    std::fill(gainEnvelope.begin(), gainEnvelope.end(), 1.0f);
    gainWritePos = 0;

    attackWindow.resize((size_t) lookAheadSamples + 1);
    for (int i = 0; i <= lookAheadSamples; ++i) {
        const float position = (float) i / (float) lookAheadSamples;
        attackWindow[(size_t) i] =
            0.5f - 0.5f * std::cos(position * juce::MathConstants<float>::pi);
    }

    auto timeConstant = [&](float ms) {
        return std::exp(-1.0f / (float) (sampleRate * ms / 1000.0));
    };
    releaseCoeff = timeConstant(kReleaseMs);
    fastReleaseCoeff = timeConstant(kFastReleaseMs);
    slowReleaseCoeff = timeConstant(kSlowReleaseMs);
    rmsCoeff = timeConstant(kRmsWindowMs);

    oversampler.reset();
    oversampler.initProcessing((size_t) maxBlockSize);
    latencySamples = lookAheadSamples
        + (int) std::lround(oversampler.getLatencyInSamples());

    currentGain = 1.0f;
    rmsEnvelope = 0.0f;
}

void LookAheadLimiter::reset()
{
    for (auto& buf : delayBuffer)
        std::fill(buf.begin(), buf.end(), 0.0f);
    std::fill(gainEnvelope.begin(), gainEnvelope.end(), 1.0f);
    delayWritePos = 0;
    gainWritePos = 0;
    currentGain = 1.0f;
    rmsEnvelope = 0.0f;
    oversampler.reset();
    gainReductionDb.store(0.0f, std::memory_order_relaxed);
}

float LookAheadLimiter::computeGain(float peakDb) const
{
    constexpr float limiterCeilingDb = kCeilingDb - kReconstructionMarginDb;

    if (peakDb <= limiterCeilingDb - kKneeDb * 0.5f)
        return 1.0f;

    float reductionDb;
    if (peakDb >= limiterCeilingDb + kKneeDb * 0.5f) {
        reductionDb = limiterCeilingDb - peakDb;
    } else {
        float delta = peakDb - (limiterCeilingDb - kKneeDb * 0.5f);
        reductionDb = -delta * delta / (2.0f * kKneeDb);
    }

    return juce::Decibels::decibelsToGain(reductionDb);
}

void LookAheadLimiter::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), kMaxChans);

    if (numSamples == 0 || delayBuffer[0].empty())
        return;

    auto outputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto oversampledBlock =
        oversampler.processSamplesUp(juce::dsp::AudioBlock<const float>(buffer));
    const int oversampledSamples = (int) oversampledBlock.getNumSamples();
    const int oversamplingFactor = oversampledSamples / numSamples;
    const auto bufSize = (int) delayBuffer[0].size();
    float maxGr = 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        float meanSquare = 0.0f;

        for (int os = 0; os < oversamplingFactor; ++os) {
            const int sampleIndex = i * oversamplingFactor + os;
            for (int ch = 0; ch < numChannels; ++ch) {
                const float sample =
                    oversampledBlock.getSample((size_t) ch, (size_t) sampleIndex);
                peak = juce::jmax(peak, std::abs(sample));
                meanSquare += sample * sample;
            }
        }
        meanSquare /= (float) (numChannels * oversamplingFactor);
        rmsEnvelope = rmsCoeff * rmsEnvelope + (1.0f - rmsCoeff) * meanSquare;

        float peakDb = juce::Decibels::gainToDecibels(peak, -96.0f);
        float targetGain = computeGain(peakDb);

        if (targetGain < currentGain) {
            currentGain = targetGain;
        } else {
            const float rms = std::sqrt(rmsEnvelope + 1.0e-12f);
            const float crestFactor = peak / (rms + 1.0e-12f);
            float adaptiveRelease = releaseCoeff;
            if (crestFactor <= 2.5f) {
                const float amount =
                    juce::jlimit(0.0f, 1.0f, (crestFactor - 1.5f) / 1.0f);
                adaptiveRelease =
                    slowReleaseCoeff
                    + amount * (releaseCoeff - slowReleaseCoeff);
            } else {
                const float amount =
                    juce::jlimit(0.0f, 1.0f, (crestFactor - 2.5f) / 1.5f);
                adaptiveRelease =
                    releaseCoeff
                    + amount * (fastReleaseCoeff - releaseCoeff);
            }
            currentGain = currentGain * adaptiveRelease + targetGain * (1.0f - adaptiveRelease);
        }

        currentGain = juce::jlimit(0.0001f, 1.0f, currentGain);
        gainEnvelope[(size_t) gainWritePos] = currentGain;

        float smoothedGain = 1.0f;
        for (int la = 0; la <= lookAheadSamples; ++la) {
            int idx = (gainWritePos - la + (int) gainEnvelope.size()) % (int) gainEnvelope.size();
            float envGain = gainEnvelope[(size_t) idx];
            float blended = 1.0f + (envGain - 1.0f) * attackWindow[(size_t) la];
            smoothedGain = juce::jmin(smoothedGain, blended);
        }

        for (int os = 0; os < oversamplingFactor; ++os) {
            const int sampleIndex = i * oversamplingFactor + os;
            for (int ch = 0; ch < numChannels; ++ch) {
                const float sample =
                    oversampledBlock.getSample((size_t) ch, (size_t) sampleIndex);
                delayBuffer[(size_t) ch][(size_t) delayWritePos] = sample;

                const int readPos =
                    (delayWritePos - lookAheadSamplesOversampled + bufSize) % bufSize;
                oversampledBlock.setSample(
                    (size_t) ch, (size_t) sampleIndex,
                    delayBuffer[(size_t) ch][(size_t) readPos] * smoothedGain);
            }
            delayWritePos = (delayWritePos + 1) % bufSize;
        }

        maxGr = juce::jmin(maxGr, smoothedGain);

        gainWritePos = (gainWritePos + 1) % (int) gainEnvelope.size();
    }

    oversampler.processSamplesDown(outputBlock);
    gainReductionDb.store(juce::Decibels::gainToDecibels(maxGr, -48.0f), std::memory_order_relaxed);
}
