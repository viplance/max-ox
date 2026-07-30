#include "TruePeakGuard.h"

#include <cmath>

void TruePeakGuard::prepare(double sampleRate, int maxBlockSize)
{
    ceiling = juce::Decibels::decibelsToGain(kCeilingDb);
    attackSamples =
        juce::jmax(1, (int) std::ceil(sampleRate * kAttackMs / 1000.0));

    detector.reset();
    detector.initProcessing((size_t) maxBlockSize);
    const int detectorLatency =
        (int) std::ceil(detector.getLatencyInSamples());
    delaySamples = attackSamples + detectorLatency;

    for (auto& channel : delayBuffer)
        channel.resize((size_t) (delaySamples + maxBlockSize + 1), 0.0f);

    gainSchedule.resize((size_t) delaySamples + 1, 1.0f);
    attackWindow.resize((size_t) attackSamples + 1, 1.0f);
    for (int i = 0; i <= attackSamples; ++i) {
        const float position = (float) i / (float) attackSamples;
        attackWindow[(size_t) i] =
            0.5f - 0.5f
                * std::cos(position * juce::MathConstants<float>::pi);
    }

    releaseCoeff = std::exp(
        -1.0f / (float) (sampleRate * kReleaseMs / 1000.0));
    shallowReleaseCoeff = std::exp(
        -1.0f / (float) (sampleRate * kShallowReleaseMs / 1000.0));
    reset();
}

void TruePeakGuard::reset()
{
    detector.reset();
    for (auto& channel : delayBuffer)
        std::fill(channel.begin(), channel.end(), 0.0f);
    std::fill(gainSchedule.begin(), gainSchedule.end(), 1.0f);
    delayWritePos = 0;
    scheduleReadPos = 0;
    samplesSinceFullSchedule = delaySamples + 1;
    lastScheduledTarget = 1.0f;
    currentGain = 1.0f;
    gainReductionDb.store(0.0f, std::memory_order_relaxed);
}

void TruePeakGuard::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), kMaxChans);
    const int numSamples = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
        return;

    auto detected = detector.processSamplesUp(
        juce::dsp::AudioBlock<const float>(buffer));
    const int detectorFactor = (int) detected.getNumSamples() / numSamples;
    const int scheduleSize = (int) gainSchedule.size();
    const int delaySize = (int) delayBuffer[0].size();
    float minimumGain = 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        float truePeak = 0.0f;
        for (int os = 0; os < detectorFactor; ++os) {
            const int detectorIndex = i * detectorFactor + os;
            for (int ch = 0; ch < numChannels; ++ch)
                truePeak = juce::jmax(
                    truePeak,
                    std::abs(detected.getSample(
                        (size_t) ch, (size_t) detectorIndex)));
        }

        const float targetGain =
            truePeak > ceiling ? ceiling / truePeak : 1.0f;
        const int duePosition =
            (scheduleReadPos + delaySamples) % scheduleSize;
        const float rescheduleThreshold =
            juce::Decibels::decibelsToGain(-0.05f);
        const bool needsReduction =
            targetGain < gainSchedule[(size_t) duePosition];
        const bool isNewReduction =
            targetGain < lastScheduledTarget * rescheduleThreshold
            || samplesSinceFullSchedule > delaySamples;

        if (needsReduction && isNewReduction) {
            for (int offset = 0; offset <= delaySamples; ++offset) {
                const int position =
                    (scheduleReadPos + offset) % scheduleSize;
                const float candidate = offset <= attackSamples
                    ? 1.0f
                        + (targetGain - 1.0f)
                            * attackWindow[(size_t) offset]
                    : targetGain;
                gainSchedule[(size_t) position] =
                    juce::jmin(gainSchedule[(size_t) position], candidate);
            }
            lastScheduledTarget = targetGain;
            samplesSinceFullSchedule = 0;
        } else if (needsReduction) {
            gainSchedule[(size_t) duePosition] = targetGain;
        }

        const float scheduledGain = gainSchedule[(size_t) scheduleReadPos];
        gainSchedule[(size_t) scheduleReadPos] = 1.0f;
        if (scheduledGain < currentGain)
            currentGain = scheduledGain;
        else {
            const float activeReleaseCoeff =
                currentGain >= juce::Decibels::decibelsToGain(
                    -kShallowReductionDb)
                    ? shallowReleaseCoeff
                    : releaseCoeff;
            currentGain =
                activeReleaseCoeff * currentGain
                + (1.0f - activeReleaseCoeff) * scheduledGain;
        }

        for (int ch = 0; ch < numChannels; ++ch) {
            const float input = buffer.getSample(ch, i);
            delayBuffer[(size_t) ch][(size_t) delayWritePos] = input;
            const int readPos =
                (delayWritePos - delaySamples + delaySize) % delaySize;
            buffer.setSample(
                ch, i,
                delayBuffer[(size_t) ch][(size_t) readPos] * currentGain);
        }

        minimumGain = juce::jmin(minimumGain, currentGain);
        delayWritePos = (delayWritePos + 1) % delaySize;
        scheduleReadPos = (scheduleReadPos + 1) % scheduleSize;
        ++samplesSinceFullSchedule;
    }

    gainReductionDb.store(
        juce::Decibels::gainToDecibels(minimumGain, -48.0f),
        std::memory_order_relaxed);
}
