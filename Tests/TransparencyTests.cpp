#include "TestHelpers.h"
#include "DSP/AdaptiveLowCut.h"

#include <cmath>
#include <iostream>
#include <random>
#include <vector>

namespace {

struct ClickReport {
    int count = 0;
    int worstSample = -1;
    float worstScore = 0.0f;
};

// Self-relative click detection: flags sudden d2 spikes relative to local
// running envelope. Works correctly regardless of phase shift.
ClickReport detectClicks(const float* x, int n, double sampleRate)
{
    ClickReport rep;
    if (n < 16) return rep;

    const float kAbsThresh = 0.005f;
    const float kSpikeRatio = 6.0f;
    const int refractory = (int)(sampleRate * 0.002);
    const float envAttack = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.0005));
    const float envRelease = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.010));
    const int warmup = (int)(sampleRate * 0.050);
    int lastClick = -refractory;
    float d2Env = 0.0f;

    for (int i = 2; i < std::min(warmup, n - 1); ++i) {
        float d2 = std::abs(x[i] - 2.0f * x[i - 1] + x[i - 2]);
        float coeff = (d2 > d2Env) ? envAttack : envRelease;
        d2Env += coeff * (d2 - d2Env);
    }

    for (int i = std::max(2, warmup); i < n - 1; ++i) {
        float d2 = std::abs(x[i] - 2.0f * x[i - 1] + x[i - 2]);
        float coeff = (d2 > d2Env) ? envAttack : envRelease;
        d2Env += coeff * (d2 - d2Env);

        if (d2 >= kAbsThresh && d2 > kSpikeRatio * d2Env
            && (i - lastClick) >= refractory) {
            ++rep.count;
            lastClick = i;
            if (d2 > rep.worstScore) {
                rep.worstScore = d2;
                rep.worstSample = i;
            }
        }
    }
    return rep;
}

}

bool runTransparencyTests()
{
    using namespace test;
    bool passed = true;

    std::cout << "\n=== Transparency test (sub-threshold signal) ===\n";

    constexpr double sr = 48000.0;
    constexpr float amplitude = 0.5f; // -6 dBFS
    constexpr float freq = 440.0f;
    constexpr int totalSamples = (int)(sr * 4.0);

    std::vector<float> inputSignal(totalSamples);
    for (int i = 0; i < totalSamples; ++i) {
        const double t = (double) i / sr;
        inputSignal[(size_t) i] = amplitude * std::sin(
            2.0 * juce::MathConstants<double>::pi * freq * t);
    }

    // Test 1: Fixed block size (256) — lowcut + limiter
    {
        constexpr int blockSize = 256;

        AdaptiveLowCut lowCut;
        LookAheadLimiter limiter;

        lowCut.prepare(sr, blockSize);
        limiter.prepare(sr, blockSize);

        const int latency = limiter.getLatencySamples();
        std::vector<float> output(totalSamples + latency, 0.0f);

        int outPos = 0;
        for (int offset = 0; offset < totalSamples; offset += blockSize) {
            const int samples = std::min(blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block(2, samples);
            for (int i = 0; i < samples; ++i) {
                block.setSample(0, i, inputSignal[(size_t)(offset + i)]);
                block.setSample(1, i, inputSignal[(size_t)(offset + i)]);
            }

            float* L = block.getWritePointer(0);
            float* R = block.getWritePointer(1);
            lowCut.process(L, R, samples, 2);
            limiter.process(block);

            for (int i = 0; i < samples; ++i)
                output[(size_t)(outPos + i)] = block.getSample(0, i);
            outPos += samples;
        }

        const int analyzeStart = latency + (int)(sr * 0.5);
        const int analyzeLen = outPos - analyzeStart - (int)(sr * 0.1);

        if (analyzeLen > 0) {
            auto clicks = detectClicks(
                output.data() + analyzeStart, analyzeLen, sr);
            std::cout << "  fixed-256: clicks=" << clicks.count;
            if (clicks.count > 0)
                std::cout << " worst=" << clicks.worstScore
                          << " @sample " << (analyzeStart + clicks.worstSample);
            std::cout << "\n";
            passed &= check(clicks.count == 0,
                            "sub-threshold sine must pass transparently (fixed blocks)");
        }
    }

    // Test 2: Varying block sizes (simulating DAW buffer changes)
    {
        constexpr int maxBlockSize = 512;

        AdaptiveLowCut lowCut;
        LookAheadLimiter limiter;

        lowCut.prepare(sr, maxBlockSize);
        limiter.prepare(sr, maxBlockSize);

        const int latency = limiter.getLatencySamples();
        std::vector<float> output(totalSamples + latency, 0.0f);

        std::mt19937 rng(42);
        std::array<int, 5> blockSizes = { 64, 128, 256, 512, 480 };

        int outPos = 0;
        int offset = 0;
        while (offset < totalSamples) {
            const int blockSize = blockSizes[rng() % blockSizes.size()];
            const int samples = std::min(blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block(2, samples);
            for (int i = 0; i < samples; ++i) {
                block.setSample(0, i, inputSignal[(size_t)(offset + i)]);
                block.setSample(1, i, inputSignal[(size_t)(offset + i)]);
            }

            float* L = block.getWritePointer(0);
            float* R = block.getWritePointer(1);
            lowCut.process(L, R, samples, 2);
            limiter.process(block);

            for (int i = 0; i < samples; ++i)
                output[(size_t)(outPos + i)] = block.getSample(0, i);
            outPos += samples;
            offset += samples;
        }

        const int analyzeStart = latency + (int)(sr * 0.5);
        const int analyzeLen = outPos - analyzeStart - (int)(sr * 0.1);

        if (analyzeLen > 0) {
            auto clicks = detectClicks(
                output.data() + analyzeStart, analyzeLen, sr);
            std::cout << "  varying-blocks: clicks=" << clicks.count;
            if (clicks.count > 0)
                std::cout << " worst=" << clicks.worstScore
                          << " @sample " << (analyzeStart + clicks.worstSample);
            std::cout << "\n";
            passed &= check(clicks.count == 0,
                            "sub-threshold sine must pass transparently (varying blocks)");
        }
    }

    // Test 3: Limiter-only distortion measurement
    {
        constexpr int blockSize = 256;

        LookAheadLimiter limiter;
        limiter.prepare(sr, blockSize);

        const int latency = limiter.getLatencySamples();
        std::vector<float> output(totalSamples + latency, 0.0f);

        int outPos = 0;
        for (int offset = 0; offset < totalSamples; offset += blockSize) {
            const int samples = std::min(blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block(2, samples);
            for (int i = 0; i < samples; ++i) {
                block.setSample(0, i, inputSignal[(size_t)(offset + i)]);
                block.setSample(1, i, inputSignal[(size_t)(offset + i)]);
            }

            limiter.process(block);

            for (int i = 0; i < samples; ++i)
                output[(size_t)(outPos + i)] = block.getSample(0, i);
            outPos += samples;
        }

        const int analyzeStart = latency + (int)(sr * 0.3);
        const int analyzeEnd = std::min(outPos, totalSamples) - (int)(sr * 0.1);
        float maxError = 0.0f;
        for (int i = analyzeStart; i < analyzeEnd; ++i) {
            const int inIdx = i - latency;
            if (inIdx >= 0 && inIdx < totalSamples) {
                const float err = std::abs(output[(size_t) i] - inputSignal[(size_t) inIdx]);
                maxError = std::max(maxError, err);
            }
        }
        const float maxErrorDb = 20.0f * std::log10(maxError / amplitude + 1e-12f);
        std::cout << "  limiter-only distortion: " << maxErrorDb << " dB re signal\n";
        passed &= check(maxErrorDb < -40.0f,
                        "limiter must not distort sub-threshold signal more than -40 dB");
    }

    // Test 4: Voice-like harmonic signal below threshold
    {
        constexpr int blockSize = 256;
        constexpr float voiceFreq = 220.0f;
        constexpr float voiceAmp = 0.4f;

        std::vector<float> voiceSignal(totalSamples);
        for (int i = 0; i < totalSamples; ++i) {
            const double t = (double) i / sr;
            const double phase = 2.0 * juce::MathConstants<double>::pi * voiceFreq * t;
            voiceSignal[(size_t) i] = voiceAmp * (
                0.5f * std::sin(phase)
                + 0.3f * std::sin(2.0 * phase)
                + 0.15f * std::sin(3.0 * phase)
                + 0.08f * std::sin(5.0 * phase)
                + 0.04f * std::sin(7.0 * phase));
        }

        AdaptiveLowCut lowCut;
        LookAheadLimiter limiter;

        lowCut.prepare(sr, blockSize);
        limiter.prepare(sr, blockSize);

        const int latency = limiter.getLatencySamples();
        std::vector<float> output(totalSamples + latency, 0.0f);

        int outPos = 0;
        for (int offset = 0; offset < totalSamples; offset += blockSize) {
            const int samples = std::min(blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block(2, samples);
            for (int i = 0; i < samples; ++i) {
                block.setSample(0, i, voiceSignal[(size_t)(offset + i)]);
                block.setSample(1, i, voiceSignal[(size_t)(offset + i)]);
            }

            float* L = block.getWritePointer(0);
            float* R = block.getWritePointer(1);
            lowCut.process(L, R, samples, 2);
            limiter.process(block);

            for (int i = 0; i < samples; ++i)
                output[(size_t)(outPos + i)] = block.getSample(0, i);
            outPos += samples;
        }

        const int analyzeStart = latency + (int)(sr * 0.5);
        const int analyzeLen = outPos - analyzeStart - (int)(sr * 0.1);

        if (analyzeLen > 0) {
            auto clicks = detectClicks(
                output.data() + analyzeStart, analyzeLen, sr);
            std::cout << "  voice-harmonic: clicks=" << clicks.count;
            if (clicks.count > 0)
                std::cout << " worst=" << clicks.worstScore
                          << " @sample " << (analyzeStart + clicks.worstSample)
                          << " (t=" << (double)(analyzeStart + clicks.worstSample) / sr << "s)";
            std::cout << "\n";
            passed &= check(clicks.count == 0,
                            "sub-threshold voice-like signal must not produce clicks");
        }
    }

    // Test 5: Long signal (10 seconds) — catch periodic artifacts
    {
        constexpr int blockSize = 256;
        constexpr int longSamples = (int)(sr * 10.0);
        constexpr float voiceFreq = 200.0f;
        constexpr float voiceAmp = 0.4f;

        std::vector<float> longSignal(longSamples);
        std::mt19937 noiseRng(123);
        std::uniform_real_distribution<float> noiseDist(-0.003f, 0.003f);
        for (int i = 0; i < longSamples; ++i) {
            const double t = (double) i / sr;
            const double vibrato = 3.0 * std::sin(2.0 * juce::MathConstants<double>::pi * 5.0 * t);
            const double instantFreq = voiceFreq + vibrato;
            const double phase = 2.0 * juce::MathConstants<double>::pi * instantFreq * t;
            float sample = voiceAmp * (
                0.5f * std::sin(phase)
                + 0.35f * std::sin(2.0 * phase)
                + 0.2f * std::sin(3.0 * phase)
                + 0.1f * std::sin(5.0 * phase));
            sample += noiseDist(noiseRng);
            longSignal[(size_t) i] = sample;
        }

        AdaptiveLowCut lowCut;
        LookAheadLimiter limiter;

        lowCut.prepare(sr, blockSize);
        limiter.prepare(sr, blockSize);

        const int latency = limiter.getLatencySamples();
        std::vector<float> output(longSamples + latency, 0.0f);

        int outPos = 0;
        for (int offset = 0; offset < longSamples; offset += blockSize) {
            const int samples = std::min(blockSize, longSamples - offset);
            juce::AudioBuffer<float> block(2, samples);
            for (int i = 0; i < samples; ++i) {
                block.setSample(0, i, longSignal[(size_t)(offset + i)]);
                block.setSample(1, i, longSignal[(size_t)(offset + i)]);
            }

            float* L = block.getWritePointer(0);
            float* R = block.getWritePointer(1);
            lowCut.process(L, R, samples, 2);
            limiter.process(block);

            for (int i = 0; i < samples; ++i)
                output[(size_t)(outPos + i)] = block.getSample(0, i);
            outPos += samples;
        }

        const int analyzeStart = latency + (int)(sr * 1.0);
        const int analyzeLen = outPos - analyzeStart - (int)(sr * 0.5);

        if (analyzeLen > 0) {
            auto clicks = detectClicks(
                output.data() + analyzeStart, analyzeLen, sr);
            std::cout << "  long-voice-10s: clicks=" << clicks.count;
            if (clicks.count > 0)
                std::cout << " worst=" << clicks.worstScore
                          << " @sample " << (analyzeStart + clicks.worstSample)
                          << " (t=" << (double)(analyzeStart + clicks.worstSample) / sr << "s)";
            std::cout << "\n";
            passed &= check(clicks.count == 0,
                            "DSP chain must not add discontinuities to sub-threshold voice signal");
        }
    }

    // Test 6: Component isolation
    {
        constexpr int blockSize = 256;
        constexpr int longSamples = (int)(sr * 10.0);
        constexpr float voiceFreq = 200.0f;
        constexpr float voiceAmp = 0.4f;

        std::vector<float> longSignal(longSamples);
        std::mt19937 noiseRng2(123);
        std::uniform_real_distribution<float> noiseDist2(-0.003f, 0.003f);
        for (int i = 0; i < longSamples; ++i) {
            const double t = (double) i / sr;
            const double vibrato = 3.0 * std::sin(2.0 * juce::MathConstants<double>::pi * 5.0 * t);
            const double instantFreq = voiceFreq + vibrato;
            const double phase = 2.0 * juce::MathConstants<double>::pi * instantFreq * t;
            float sample = voiceAmp * (
                0.5f * std::sin(phase)
                + 0.35f * std::sin(2.0 * phase)
                + 0.2f * std::sin(3.0 * phase)
                + 0.1f * std::sin(5.0 * phase));
            sample += noiseDist2(noiseRng2);
            longSignal[(size_t) i] = sample;
        }

        auto countAddedClicks = [&](const std::vector<float>& out, int lat,
                                    int outLen) -> int {
            const int aStart = lat + (int)(sr * 1.0);
            const int aLen = outLen - aStart - (int)(sr * 0.5);
            if (aLen <= 0) return -1;
            auto report = detectClicks(out.data() + aStart, aLen, sr);
            return report.count;
        };

        // LowCut only
        {
            AdaptiveLowCut lowCut;
            lowCut.prepare(sr, blockSize);
            std::vector<float> out(longSamples, 0.0f);
            for (int offset = 0; offset < longSamples; offset += blockSize) {
                const int samples = std::min(blockSize, longSamples - offset);
                std::vector<float> L(longSignal.begin() + offset,
                                     longSignal.begin() + offset + samples);
                std::vector<float> R(L);
                lowCut.process(L.data(), R.data(), samples, 2);
                std::copy_n(L.data(), samples, out.data() + offset);
            }
            int clicks = countAddedClicks(out, 0, longSamples);
            std::cout << "  isolate-lowcut: added-clicks=" << clicks << "\n";
        }

        // Limiter only
        {
            LookAheadLimiter limiter;
            limiter.prepare(sr, blockSize);
            const int lat = limiter.getLatencySamples();
            std::vector<float> out(longSamples + lat, 0.0f);
            int outPos = 0;
            float minGR = 0.0f;
            for (int offset = 0; offset < longSamples; offset += blockSize) {
                const int samples = std::min(blockSize, longSamples - offset);
                juce::AudioBuffer<float> block(2, samples);
                for (int i = 0; i < samples; ++i) {
                    block.setSample(0, i, longSignal[(size_t)(offset + i)]);
                    block.setSample(1, i, longSignal[(size_t)(offset + i)]);
                }
                limiter.process(block);
                minGR = std::min(minGR, limiter.getGainReductionDb());
                for (int i = 0; i < samples; ++i)
                    out[(size_t)(outPos + i)] = block.getSample(0, i);
                outPos += samples;
            }
            int clicks = countAddedClicks(out, lat, outPos);
            std::cout << "  isolate-limiter: added-clicks=" << clicks
                      << " minGR=" << minGR << " dB\n";
        }

        // LowCut + Limiter (full chain without phase rotator)
        {
            AdaptiveLowCut lc;
            LookAheadLimiter lim;
            lc.prepare(sr, blockSize);
            lim.prepare(sr, blockSize);
            int lat = lim.getLatencySamples();
            std::vector<float> out(longSamples + lat, 0.0f);
            int outPos = 0;
            for (int offset = 0; offset < longSamples; offset += blockSize) {
                int samples = std::min(blockSize, longSamples - offset);
                juce::AudioBuffer<float> block(2, samples);
                for (int j = 0; j < samples; ++j) {
                    block.setSample(0, j, longSignal[(size_t)(offset + j)]);
                    block.setSample(1, j, longSignal[(size_t)(offset + j)]);
                }
                lc.process(block.getWritePointer(0), block.getWritePointer(1), samples, 2);
                lim.process(block);
                for (int j = 0; j < samples; ++j)
                    out[(size_t)(outPos + j)] = block.getSample(0, j);
                outPos += samples;
            }
            int clicks = countAddedClicks(out, lat, outPos);
            std::cout << "  combo lc+lim: " << clicks << "\n";
        }
    }

    // Test: Real-world voice signal (164 Hz) — limiter transparency at sub-threshold
    {
        constexpr int blockSize = 256;
        constexpr int longSamples = (int)(sr * 5.0);
        constexpr float voiceAmp = 0.078f; // -22 dBFS

        std::vector<float> realVoice(longSamples);
        for (int i = 0; i < longSamples; ++i) {
            const double t = (double) i / sr;
            const double phase = 2.0 * juce::MathConstants<double>::pi * 164.0 * t;
            realVoice[(size_t) i] = voiceAmp * (
                std::sin(phase)
                + 0.9f * std::sin(2.0 * phase)
                + 0.3f * std::sin(3.0 * phase));
        }

        LookAheadLimiter lim;
        lim.prepare(sr, blockSize);
        const int lat = lim.getLatencySamples();
        std::vector<float> out(longSamples + lat, 0.0f);
        int op = 0;
        for (int offset = 0; offset < longSamples; offset += blockSize) {
            int samples = std::min(blockSize, longSamples - offset);
            juce::AudioBuffer<float> block(2, samples);
            for (int j = 0; j < samples; ++j) {
                block.setSample(0, j, realVoice[(size_t)(offset + j)]);
                block.setSample(1, j, realVoice[(size_t)(offset + j)]);
            }
            lim.process(block);
            for (int j = 0; j < samples; ++j)
                out[(size_t)(op + j)] = block.getSample(0, j);
            op += samples;
        }

        const int aStart = lat + (int)(sr * 1.0);
        const int aEnd = std::min(op, longSamples) - (int)(sr * 0.1);
        float maxErr = 0.0f;
        for (int i = aStart; i < aEnd; ++i) {
            const int inIdx = i - lat;
            if (inIdx >= 0 && inIdx < longSamples) {
                float err = std::abs(out[(size_t) i] - realVoice[(size_t) inIdx]);
                maxErr = std::max(maxErr, err);
            }
        }
        float errDb = 20.0f * std::log10(maxErr / voiceAmp + 1e-12f);
        std::cout << "  real-voice-164Hz (limiter-only): " << errDb << " dB\n";
        passed &= check(errDb < -40.0f,
                        "sub-threshold voice must pass through limiter transparently");
    }

    // Test: LowCut gain modulation on steady voice
    {
        constexpr int blockSize = 256;
        constexpr int longSamples = (int)(sr * 5.0);

        std::vector<float> sig(longSamples);
        for (int i = 0; i < longSamples; ++i) {
            const double t = (double) i / sr;
            const double phase = 2.0 * juce::MathConstants<double>::pi * 164.0 * t;
            sig[(size_t) i] = 0.5f * (
                std::sin(phase) + 0.9f * std::sin(2.0 * phase)
                + 0.3f * std::sin(3.0 * phase));
        }

        AdaptiveLowCut lc;
        lc.prepare(sr, blockSize);
        std::vector<float> out(longSamples, 0.0f);
        for (int offset = 0; offset < longSamples; offset += blockSize) {
            int samples = std::min(blockSize, longSamples - offset);
            std::vector<float> L(sig.begin() + offset, sig.begin() + offset + samples);
            std::vector<float> R(L);
            lc.process(L.data(), R.data(), samples, 2);
            std::copy_n(L.data(), samples, out.data() + offset);
        }

        // Measure per-period gain stability (164 Hz = ~293 samples/period)
        constexpr int periodSamples = 293;
        float maxGainDb = -999.0f, minGainDb = 999.0f;
        const int settleStart = (int)(sr * 1.0);
        for (int i = settleStart; i + periodSamples < longSamples; i += periodSamples) {
            double rmsIn = 0.0, rmsOut = 0.0;
            for (int j = 0; j < periodSamples; ++j) {
                rmsIn += (double)sig[(size_t)(i+j)] * sig[(size_t)(i+j)];
                rmsOut += (double)out[(size_t)(i+j)] * out[(size_t)(i+j)];
            }
            rmsIn = std::sqrt(rmsIn / periodSamples);
            rmsOut = std::sqrt(rmsOut / periodSamples);
            if (rmsIn > 0.01) {
                float gDb = 20.0f * std::log10((float)(rmsOut / rmsIn) + 1e-12f);
                maxGainDb = std::max(maxGainDb, gDb);
                minGainDb = std::min(minGainDb, gDb);
            }
        }
        float gainSwing = maxGainDb - minGainDb;
        std::cout << "  lowcut-gain-stability: swing=" << gainSwing
                  << " dB (min=" << minGainDb << " max=" << maxGainDb << ")\n";
        passed &= check(gainSwing < 1.0f,
                        "lowcut must not create >1 dB gain modulation on steady voice");
    }

    return passed;
}
