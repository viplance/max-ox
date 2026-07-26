#include "PerceptualPeakShaver.h"

#include <cmath>

float PerceptualPeakShaver::Biquad::process(float input)
{
    const float output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return output;
}

void PerceptualPeakShaver::Biquad::reset()
{
    z1 = 0.0f;
    z2 = 0.0f;
}

float PerceptualPeakShaver::erbRate(float frequencyHz)
{
    return 21.4f * std::log10(1.0f + 0.00437f * frequencyHz);
}

float PerceptualPeakShaver::inverseErbRate(float rate)
{
    return (std::pow(10.0f, rate / 21.4f) - 1.0f) / 0.00437f;
}

void PerceptualPeakShaver::configureBandPass(
    Biquad& filter, double sampleRate, float frequencyHz, float q)
{
    const float omega =
        juce::MathConstants<float>::twoPi
        * frequencyHz / (float) sampleRate;
    const float alpha = std::sin(omega) / (2.0f * q);
    const float a0 = 1.0f + alpha;

    filter.b0 = alpha / a0;
    filter.b1 = 0.0f;
    filter.b2 = -alpha / a0;
    filter.a1 = -2.0f * std::cos(omega) / a0;
    filter.a2 = (1.0f - alpha) / a0;
    filter.reset();
}

void PerceptualPeakShaver::prepare(
    double baseSampleRate, int oversamplingFactor)
{
    analysisSampleRate = baseSampleRate;
    factor = juce::jmax(1, oversamplingFactor);
    processingSampleRate = baseSampleRate * (double) factor;
    clipStart = juce::Decibels::decibelsToGain(kClipStartDb);

    const float minimumRate = erbRate(60.0f);
    const float maximumFrequency =
        juce::jmin(18000.0f, (float) baseSampleRate * 0.45f);
    const float maximumRate = erbRate(maximumFrequency);

    for (int band = 0; band < kErbBands; ++band) {
        const float position =
            ((float) band + 0.5f) / (float) kErbBands;
        const float centre = inverseErbRate(
            minimumRate + position * (maximumRate - minimumRate));
        const float erbBandwidth =
            24.7f * (1.0f + 0.00437f * centre);
        const float q = juce::jlimit(
            0.5f, 6.0f, centre / (2.0f * erbBandwidth));

        for (int ch = 0; ch < kMaxChans; ++ch) {
            configureBandPass(
                sourceFilters[(size_t) ch][(size_t) band],
                baseSampleRate, centre, q);
            configureBandPass(
                residualFilters[(size_t) ch][(size_t) band],
                baseSampleRate, centre, q);
        }
    }

    auto coefficient = [](double sampleRate, float milliseconds) {
        return std::exp(
            -1.0f
            / (float) (sampleRate * (double) milliseconds / 1000.0));
    };

    sourceAttack = coefficient(baseSampleRate, 2.0f);
    sourceRelease = coefficient(baseSampleRate, 80.0f);
    residualAttack = coefficient(baseSampleRate, 0.5f);
    residualRelease = coefficient(baseSampleRate, 50.0f);
    allowanceAttack = coefficient(baseSampleRate, 2.0f);
    allowanceRelease = coefficient(baseSampleRate, 120.0f);
    rmsCoeff = coefficient(processingSampleRate, 20.0f);
    slowPeakCoeff = coefficient(processingSampleRate, 35.0f);

    reset();
}

void PerceptualPeakShaver::reset()
{
    for (int ch = 0; ch < kMaxChans; ++ch) {
        rmsEnvelope[(size_t) ch] = 0.0f;
        slowPeakEnvelope[(size_t) ch] = 0.0f;
        for (int band = 0; band < kErbBands; ++band) {
            sourceFilters[(size_t) ch][(size_t) band].reset();
            residualFilters[(size_t) ch][(size_t) band].reset();
            sourceEnvelope[(size_t) ch][(size_t) band] = 0.0f;
            residualEnvelope[(size_t) ch][(size_t) band] = 0.0f;
        }
    }

    allowanceState = 1.0f;
    reductionDb.store(0.0f, std::memory_order_relaxed);
    maskingAllowance.store(1.0f, std::memory_order_relaxed);
}

void PerceptualPeakShaver::analyseMasking(
    const std::array<float, kMaxChans>& source,
    const std::array<float, kMaxChans>& residual,
    int numChannels)
{
    const float maskingRatio =
        juce::Decibels::decibelsToGain(kMaskingRatioDb);
    constexpr float absolutePowerFloor = 1.0e-10f;

    float maximumNoiseToMask = 0.0f;
    float maximumTonalConcentration = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch) {
        float channelSourcePower = 0.0f;
        float channelStrongestBand = 0.0f;

        for (int band = 0; band < kErbBands; ++band) {
            const float sourceBand =
                sourceFilters[(size_t) ch][(size_t) band]
                    .process(source[(size_t) ch]);
            const float residualBand =
                residualFilters[(size_t) ch][(size_t) band]
                    .process(residual[(size_t) ch]);
            const float sourcePower = sourceBand * sourceBand;
            const float residualPower = residualBand * residualBand;

            const float sourceCoeff =
                sourcePower > sourceEnvelope[(size_t) ch][(size_t) band]
                    ? sourceAttack
                    : sourceRelease;
            const float residualCoeff =
                residualPower > residualEnvelope[(size_t) ch][(size_t) band]
                    ? residualAttack
                    : residualRelease;

            auto& trackedSource =
                sourceEnvelope[(size_t) ch][(size_t) band];
            auto& trackedResidual =
                residualEnvelope[(size_t) ch][(size_t) band];
            trackedSource =
                sourceCoeff * trackedSource
                + (1.0f - sourceCoeff) * sourcePower;
            trackedResidual =
                residualCoeff * trackedResidual
                + (1.0f - residualCoeff) * residualPower;

            const float maskingPower =
                trackedSource * maskingRatio * maskingRatio
                + absolutePowerFloor;
            maximumNoiseToMask = juce::jmax(
                maximumNoiseToMask,
                trackedResidual / maskingPower);
            channelSourcePower += trackedSource;
            channelStrongestBand =
                juce::jmax(channelStrongestBand, trackedSource);
        }

        maximumTonalConcentration = juce::jmax(
            maximumTonalConcentration,
            channelStrongestBand / (channelSourcePower + 1.0e-12f));
    }

    float targetAllowance = maximumNoiseToMask > 1.0f
        ? 1.0f / std::sqrt(maximumNoiseToMask)
        : 1.0f;

    if (maximumTonalConcentration > 0.45f) {
        const float tonalPenalty = juce::jlimit(
            0.35f, 1.0f,
            1.0f - (maximumTonalConcentration - 0.45f) * 1.5f);
        targetAllowance *= tonalPenalty;
    }

    const float coefficient =
        targetAllowance < allowanceState
            ? allowanceAttack
            : allowanceRelease;
    allowanceState =
        coefficient * allowanceState
        + (1.0f - coefficient) * targetAllowance;
    allowanceState = juce::jlimit(0.0f, 1.0f, allowanceState);
    maskingAllowance.store(allowanceState, std::memory_order_relaxed);
}

void PerceptualPeakShaver::process(
    juce::dsp::AudioBlock<float>& block, int numChannels)
{
    numChannels = juce::jmin(
        numChannels, juce::jmin((int) block.getNumChannels(), kMaxChans));
    const int numSamples = (int) block.getNumSamples();
    float maximumReduction = 0.0f;
    std::array<float, kMaxChans> sourceAccumulator {};
    std::array<float, kMaxChans> residualAccumulator {};
    int accumulatedSamples = 0;

    for (int i = 0; i < numSamples; ++i) {
        std::array<float, kMaxChans> input {};
        float transientAmount = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch) {
            input[(size_t) ch] =
                block.getSample((size_t) ch, (size_t) i);
            const float magnitude = std::abs(input[(size_t) ch]);
            rmsEnvelope[(size_t) ch] =
                rmsCoeff * rmsEnvelope[(size_t) ch]
                + (1.0f - rmsCoeff) * magnitude * magnitude;
            slowPeakEnvelope[(size_t) ch] = juce::jmax(
                magnitude,
                slowPeakCoeff * slowPeakEnvelope[(size_t) ch]);

            const float crest = magnitude
                / (std::sqrt(rmsEnvelope[(size_t) ch] + 1.0e-12f)
                   + 1.0e-12f);
            const float novelty = magnitude
                / (slowPeakEnvelope[(size_t) ch] + 1.0e-12f);
            const float crestAmount =
                juce::jlimit(0.0f, 1.0f, (crest - 1.8f) / 2.2f);
            const float noveltyAmount =
                juce::jlimit(0.0f, 1.0f, (novelty - 0.72f) / 0.28f);
            transientAmount = juce::jmax(
                transientAmount, crestAmount * noveltyAmount);
        }

        float sidePenalty = 1.0f;
        if (numChannels == 2) {
            const float mid = 0.5f * (input[0] + input[1]);
            const float side = 0.5f * (input[0] - input[1]);
            const float sideFraction =
                side * side / (mid * mid + side * side + 1.0e-12f);
            sidePenalty = 1.0f - 0.5f * sideFraction;
        }

        float shaveDepthDb = 0.0f;
        if (transientAmount > 0.15f) {
            shaveDepthDb =
                (kMinimumShaveDb
                 + (kMaximumShaveDb - kMinimumShaveDb) * transientAmount)
                * allowanceState * sidePenalty;
        }
        const float minimumGain =
            juce::Decibels::decibelsToGain(-shaveDepthDb);

        for (int ch = 0; ch < numChannels; ++ch) {
            const float magnitude = std::abs(input[(size_t) ch]);
            float output = input[(size_t) ch];

            if (shaveDepthDb > 0.0f && magnitude > clipStart) {
                const float kneeWidth = clipStart * 0.25f;
                const float excess = magnitude - clipStart;
                const float softMagnitude =
                    clipStart
                    + kneeWidth * (1.0f - std::exp(-excess / kneeWidth));
                const float limitedMagnitude =
                    juce::jmax(softMagnitude, magnitude * minimumGain);
                output = std::copysign(limitedMagnitude, input[(size_t) ch]);
                maximumReduction = juce::jmax(
                    maximumReduction,
                    juce::Decibels::gainToDecibels(
                        magnitude / (limitedMagnitude + 1.0e-12f), 0.0f));
            }

            block.setSample((size_t) ch, (size_t) i, output);
            sourceAccumulator[(size_t) ch] += input[(size_t) ch];
            residualAccumulator[(size_t) ch] +=
                output - input[(size_t) ch];
        }

        ++accumulatedSamples;
        if (accumulatedSamples >= factor) {
            std::array<float, kMaxChans> source {};
            std::array<float, kMaxChans> residual {};
            for (int ch = 0; ch < numChannels; ++ch) {
                source[(size_t) ch] =
                    sourceAccumulator[(size_t) ch] / (float) accumulatedSamples;
                residual[(size_t) ch] =
                    residualAccumulator[(size_t) ch]
                    / (float) accumulatedSamples;
                sourceAccumulator[(size_t) ch] = 0.0f;
                residualAccumulator[(size_t) ch] = 0.0f;
            }
            analyseMasking(source, residual, numChannels);
            accumulatedSamples = 0;
        }
    }

    reductionDb.store(maximumReduction, std::memory_order_relaxed);
}
