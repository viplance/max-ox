#include "LookAheadLimiter.h"
#include <cmath>

void LookAheadLimiter::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    lookAheadSamples = (int) std::ceil(sampleRate * kLookAheadMs / 1000.0);

    for (auto& buf : delayBuffer) {
        buf.resize((size_t) (lookAheadSamples + maxBlockSize + 1), 0.0f);
        std::fill(buf.begin(), buf.end(), 0.0f);
    }
    delayWritePos = 0;

    gainSchedule.resize((size_t) lookAheadSamples + 1, 1.0f);
    attackWindow.resize((size_t) lookAheadSamples + 1, 1.0f);
    for (int i = 0; i <= lookAheadSamples; ++i) {
        const float position = (float) i / (float) lookAheadSamples;
        attackWindow[(size_t) i] =
            0.5f - 0.5f
                * std::cos(position * juce::MathConstants<float>::pi);
    }
    scheduleReadPos = 0;
    samplesSinceFullSchedule = lookAheadSamples + 1;
    lastScheduledTarget = 1.0f;

    auto timeConstant = [&](float ms) {
        return std::exp(-1.0f / (float) (sampleRate * ms / 1000.0));
    };
    releaseCoeff = timeConstant(kReleaseMs);
    fastReleaseCoeff = timeConstant(kFastReleaseMs);
    slowReleaseCoeff = timeConstant(kSlowReleaseMs);
    shallowReleaseCoeff = timeConstant(kShallowReleaseMs);
    rmsCoeff = timeConstant(kRmsWindowMs);
    releaseCoeffSmoothing = timeConstant(kReleaseSmoothingMs);
    smoothedReleaseCoeff = releaseCoeff;
    maxReleasePerSample = juce::Decibels::decibelsToGain(
        kMaxReleaseRateDbPerSec / (float) sampleRate);

    peakShaver.prepare(sampleRate, 1);
    truePeakGuard.prepare(sampleRate, maxBlockSize);
    latencySamples = lookAheadSamples + truePeakGuard.getLatencySamples();

    currentGain = 1.0f;
    rmsEnvelope = 0.0f;
}

void LookAheadLimiter::reset()
{
    for (auto& buf : delayBuffer)
        std::fill(buf.begin(), buf.end(), 0.0f);
    std::fill(gainSchedule.begin(), gainSchedule.end(), 1.0f);
    delayWritePos = 0;
    scheduleReadPos = 0;
    samplesSinceFullSchedule = lookAheadSamples + 1;
    lastScheduledTarget = 1.0f;
    currentGain = 1.0f;
    rmsEnvelope = 0.0f;
    smoothedReleaseCoeff = releaseCoeff;
    peakShaver.reset();
    truePeakGuard.reset();
    gainReductionDb.store(0.0f, std::memory_order_relaxed);
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

    auto block = juce::dsp::AudioBlock<float>(buffer);
    peakShaver.process(block, numChannels);

    const auto bufSize = (int) delayBuffer[0].size();
    float maxGr = 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        float meanSquare = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch) {
            const float sample = buffer.getSample(ch, i);
            delayBuffer[(size_t) ch][(size_t) delayWritePos] = sample;
            const float mag = std::abs(sample);
            peak = juce::jmax(peak, mag);
            meanSquare += sample * sample;
        }
        meanSquare /= (float) numChannels;
        rmsEnvelope = rmsCoeff * rmsEnvelope + (1.0f - rmsCoeff) * meanSquare;

        float peakDb = juce::Decibels::gainToDecibels(peak, -96.0f);
        float targetGain = computeGain(peakDb);

        const int scheduleSize = (int) gainSchedule.size();
        const int duePosition =
            (scheduleReadPos + lookAheadSamples) % scheduleSize;
        const float rescheduleThreshold =
            juce::Decibels::decibelsToGain(-0.1f);
        const bool dueNeedsMoreReduction =
            targetGain < gainSchedule[(size_t) duePosition];
        const bool isNewReduction =
            targetGain < lastScheduledTarget * rescheduleThreshold
            || samplesSinceFullSchedule > lookAheadSamples;

        if (dueNeedsMoreReduction && isNewReduction) {
            for (int offset = 0; offset <= lookAheadSamples; ++offset) {
                const int position =
                    (scheduleReadPos + offset) % scheduleSize;
                const float candidate =
                    1.0f
                    + (targetGain - 1.0f) * attackWindow[(size_t) offset];
                gainSchedule[(size_t) position] =
                    juce::jmin(gainSchedule[(size_t) position], candidate);
            }
            lastScheduledTarget = targetGain;
            samplesSinceFullSchedule = 0;
        } else if (dueNeedsMoreReduction) {
            gainSchedule[(size_t) duePosition] = targetGain;
        }

        const float scheduledGain = gainSchedule[(size_t) scheduleReadPos];
        gainSchedule[(size_t) scheduleReadPos] = 1.0f;

        if (scheduledGain < currentGain) {
            currentGain = scheduledGain;
        } else {
            const float rms = std::sqrt(rmsEnvelope + 1.0e-12f);
            const float crestFactor = peak / (rms + 1.0e-12f);
            float targetRelease = releaseCoeff;
            if (crestFactor <= 2.5f) {
                const float amount =
                    juce::jlimit(0.0f, 1.0f, (crestFactor - 1.5f) / 1.0f);
                targetRelease =
                    slowReleaseCoeff
                    + amount * (releaseCoeff - slowReleaseCoeff);
            } else {
                const float amount =
                    juce::jlimit(0.0f, 1.0f, (crestFactor - 2.5f) / 1.5f);
                targetRelease =
                    releaseCoeff
                    + amount * (fastReleaseCoeff - releaseCoeff);
            }
            if (currentGain >= juce::Decibels::decibelsToGain(
                    -kShallowReductionDb))
                targetRelease = juce::jmin(
                    targetRelease, shallowReleaseCoeff);

            smoothedReleaseCoeff =
                releaseCoeffSmoothing * smoothedReleaseCoeff
                + (1.0f - releaseCoeffSmoothing) * targetRelease;
            float newGain =
                currentGain * smoothedReleaseCoeff
                + scheduledGain * (1.0f - smoothedReleaseCoeff);
            newGain = juce::jmin(newGain, currentGain * maxReleasePerSample);
            currentGain = newGain;
        }

        currentGain = juce::jlimit(0.0001f, 1.0f, currentGain);

        const int readPos = (delayWritePos - lookAheadSamples + bufSize) % bufSize;
        for (int ch = 0; ch < numChannels; ++ch) {
            const float delayed = delayBuffer[(size_t) ch][(size_t) readPos];
            buffer.setSample(ch, i, delayed * currentGain);
        }

        maxGr = juce::jmin(maxGr, currentGain);
        delayWritePos = (delayWritePos + 1) % bufSize;
        scheduleReadPos = (scheduleReadPos + 1) % scheduleSize;
        ++samplesSinceFullSchedule;
    }

    truePeakGuard.process(buffer);
    maxGr = juce::jmin(
        maxGr,
        juce::Decibels::decibelsToGain(
            truePeakGuard.getGainReductionDb()));
    gainReductionDb.store(juce::Decibels::gainToDecibels(maxGr, -48.0f), std::memory_order_relaxed);
}
