#include "LookAheadLimiter.h"
#include <cmath>

void LookAheadLimiter::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    ceiling = juce::Decibels::decibelsToGain(kCeilingDb);

    lookAheadSamples = (int) std::ceil(sampleRate * kLookAheadMs / 1000.0);

    for (auto& buf : delayBuffer) {
        buf.resize((size_t) (lookAheadSamples + maxBlockSize + 1), 0.0f);
        std::fill(buf.begin(), buf.end(), 0.0f);
    }
    delayWritePos = 0;

    gainEnvelope.resize((size_t) (lookAheadSamples + maxBlockSize + 1), 1.0f);
    std::fill(gainEnvelope.begin(), gainEnvelope.end(), 1.0f);
    gainWritePos = 0;

    auto timeConstant = [&](float ms) {
        return std::exp(-1.0f / (float) (sampleRate * ms / 1000.0));
    };
    releaseCoeff = timeConstant(kReleaseMs);
    fastReleaseCoeff = timeConstant(kFastReleaseMs);
    slowReleaseCoeff = timeConstant(kSlowReleaseMs);

    oversampler.reset();
    oversampler.initProcessing((size_t) maxBlockSize);

    currentGain = 1.0f;
}

void LookAheadLimiter::reset()
{
    for (auto& buf : delayBuffer)
        std::fill(buf.begin(), buf.end(), 0.0f);
    std::fill(gainEnvelope.begin(), gainEnvelope.end(), 1.0f);
    delayWritePos = 0;
    gainWritePos = 0;
    currentGain = 1.0f;
    oversampler.reset();
}

float LookAheadLimiter::computeGain(float peakDb) const
{
    if (peakDb <= kCeilingDb - kKneeDb * 0.5f)
        return 1.0f;

    float reductionDb;
    if (peakDb >= kCeilingDb + kKneeDb * 0.5f) {
        reductionDb = kCeilingDb - peakDb;
    } else {
        float delta = peakDb - (kCeilingDb - kKneeDb * 0.5f);
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

    const auto bufSize = (int) delayBuffer[0].size();
    float maxGr = 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            const float sample = buffer.getSample(ch, i);
            delayBuffer[(size_t) ch][(size_t) delayWritePos] = sample;
            peak = juce::jmax(peak, std::abs(sample));
        }

        float peakDb = juce::Decibels::gainToDecibels(peak, -96.0f);
        float targetGain = computeGain(peakDb);

        if (targetGain < currentGain) {
            currentGain = targetGain;
        } else {
            float crestFactor = peak / (currentGain + 1e-12f);
            float adaptiveRelease = crestFactor > 4.0f ? fastReleaseCoeff
                                  : crestFactor < 1.5f ? slowReleaseCoeff
                                  : releaseCoeff;
            currentGain = currentGain * adaptiveRelease + targetGain * (1.0f - adaptiveRelease);
        }

        currentGain = juce::jlimit(0.0001f, 1.0f, currentGain);
        gainEnvelope[(size_t) gainWritePos] = currentGain;

        float smoothedGain = 1.0f;
        for (int la = 0; la < lookAheadSamples; ++la) {
            int idx = (gainWritePos - la + (int) gainEnvelope.size()) % (int) gainEnvelope.size();
            float windowPos = (float) la / (float) lookAheadSamples;
            float windowGain = 0.5f - 0.5f * std::cos(windowPos * juce::MathConstants<float>::pi);
            float envGain = gainEnvelope[(size_t) idx];
            float blended = 1.0f + (envGain - 1.0f) * windowGain;
            smoothedGain = juce::jmin(smoothedGain, blended);
        }

        int readPos = (delayWritePos - lookAheadSamples + bufSize) % bufSize;
        for (int ch = 0; ch < numChannels; ++ch) {
            float delayed = delayBuffer[(size_t) ch][(size_t) readPos];
            float out = delayed * smoothedGain;
            out = juce::jlimit(-ceiling, ceiling, out);
            buffer.setSample(ch, i, out);
        }

        maxGr = juce::jmin(maxGr, smoothedGain);

        delayWritePos = (delayWritePos + 1) % bufSize;
        gainWritePos = (gainWritePos + 1) % (int) gainEnvelope.size();
    }

    gainReductionDb.store(juce::Decibels::gainToDecibels(maxGr, -48.0f), std::memory_order_relaxed);
}
