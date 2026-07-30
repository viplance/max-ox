#include "TestHelpers.h"

bool runGainAnomalyTests()
{
    using namespace test;
    bool passed = true;

    std::cout << "\n=== Gain anomaly diagnostic ===\n";

    struct TestSignal {
        const char* name;
        Stereo data;
    };

    constexpr int length = 192000;
    const double pi2 = 2.0 * juce::MathConstants<double>::pi;
    std::vector<TestSignal> signals;

    {
        Stereo s { std::vector<float>(length), std::vector<float>(length) };
        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            const float v = 2.5f * std::sin(pi2 * 1000.0 * t);
            s[0][(size_t) i] = v;
            s[1][(size_t) i] = v;
        }
        signals.push_back({ "mono-1kHz-loud", std::move(s) });
    }
    {
        Stereo s { std::vector<float>(length), std::vector<float>(length) };
        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            const float v = 2.0f * (
                0.6f * std::sin(pi2 * 100.0 * t)
                + 0.5f * std::sin(pi2 * 200.0 * t + 0.3)
                + 0.35f * std::sin(pi2 * 500.0 * t + 1.1)
                + 0.25f * std::sin(pi2 * 1000.0 * t + 0.7)
                + 0.15f * std::sin(pi2 * 3000.0 * t + 2.0));
            s[0][(size_t) i] = v;
            s[1][(size_t) i] = v;
        }
        signals.push_back({ "mono-multiband", std::move(s) });
    }
    {
        Stereo s { std::vector<float>(length), std::vector<float>(length) };
        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            s[0][(size_t) i] = 2.0f * std::sin(pi2 * 300.0 * t);
            s[1][(size_t) i] = 2.0f * std::sin(pi2 * 300.0 * t + 1.2);
        }
        signals.push_back({ "stereo-phase-offset", std::move(s) });
    }
    {
        Stereo s { std::vector<float>(length), std::vector<float>(length) };
        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            const float mid = 2.0f * std::sin(pi2 * 200.0 * t);
            const float side = 0.8f * std::sin(pi2 * 5000.0 * t);
            s[0][(size_t) i] = mid + side;
            s[1][(size_t) i] = mid - side;
        }
        signals.push_back({ "stereo-mid-side", std::move(s) });
    }
    {
        Stereo s { std::vector<float>(length), std::vector<float>(length) };
        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            float v = 0.35f * (
                std::cos(pi2 * 80.0 * t)
                + 0.75f * std::cos(2.0 * pi2 * 80.0 * t)
                + 0.50f * std::cos(3.0 * pi2 * 80.0 * t)
                + 0.25f * std::cos(4.0 * pi2 * 80.0 * t));
            s[0][(size_t) i] = v;
            s[1][(size_t) i] = v;
        }
        signals.push_back({ "asymmetric-80Hz", std::move(s) });
    }
    {
        Stereo s { std::vector<float>(length), std::vector<float>(length) };
        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            float v = 1.5f * std::sin(pi2 * 400.0 * t);
            if (i % 9600 < 480)
                v += 3.0f * std::sin(pi2 * 4000.0 * t)
                   * (1.0f - (float)(i % 9600) / 480.0f);
            s[0][(size_t) i] = v;
            s[1][(size_t) i] = v;
        }
        signals.push_back({ "transient-bursts", std::move(s) });
    }
    {
        Stereo s { std::vector<float>(length), std::vector<float>(length) };
        for (int i = 0; i < length; ++i) {
            const double t = (double) i / kSampleRate;
            const float carrier = std::sin(pi2 * 100.0 * t);
            const float tremolo = 0.5f + 0.5f * std::sin(pi2 * 6.0 * t);
            s[0][(size_t) i] = 3.0f * carrier * tremolo;
            s[1][(size_t) i] = 3.0f * carrier * tremolo;
        }
        signals.push_back({ "amplitude-modulated", std::move(s) });
    }

    constexpr int windowLen = 480;

    for (auto& sig : signals) {
        LookAheadLimiter limiter;
        limiter.prepare(kSampleRate, kBlockSize);
        const auto output = render(sig.data, limiter);
        const int latency = limiter.getLatencySamples();

        const int settledBeg = latency + (int)(kSampleRate * 0.15);
        const int analysisEnd = length - windowLen;

        std::vector<float> windowRmsOut;
        std::vector<float> windowRmsIn;
        for (int pos = settledBeg; pos + windowLen <= analysisEnd;
             pos += windowLen) {
            double rmsIn = 0.0;
            double rmsOut = 0.0;
            for (int j = pos; j < pos + windowLen; ++j) {
                const int inIdx = j - latency;
                if (inIdx >= 0 && inIdx < length) {
                    for (int ch = 0; ch < 2; ++ch) {
                        rmsIn += (double) sig.data[(size_t) ch][(size_t) inIdx]
                               * sig.data[(size_t) ch][(size_t) inIdx];
                        rmsOut += (double) output[(size_t) ch][(size_t) j]
                                * output[(size_t) ch][(size_t) j];
                    }
                }
            }
            windowRmsIn.push_back((float) std::sqrt(rmsIn / (windowLen * 2)));
            windowRmsOut.push_back((float) std::sqrt(rmsOut / (windowLen * 2)));
        }

        float maxGainUp = 0.0f;
        int maxGainUpIdx = -1;
        float maxGainJerk = 0.0f;
        int maxGainJerkIdx = -1;

        for (size_t k = 0; k < windowRmsOut.size(); ++k) {
            if (windowRmsIn[k] < 0.01f)
                continue;
            const float gain = windowRmsOut[k] / windowRmsIn[k];
            if (gain > 1.0f + maxGainUp) {
                maxGainUp = gain - 1.0f;
                maxGainUpIdx = (int) k;
            }
        }

        for (size_t k = 1; k < windowRmsOut.size(); ++k) {
            if (windowRmsOut[k] < 0.01f || windowRmsOut[k - 1] < 0.01f)
                continue;
            if (windowRmsIn[k] < 0.01f || windowRmsIn[k - 1] < 0.01f)
                continue;
            const float g0 = windowRmsOut[k - 1] / windowRmsIn[k - 1];
            const float g1 = windowRmsOut[k] / windowRmsIn[k];
            const float ratio = g1 > g0 ? g1 / g0 : g0 / g1;
            const float jerkDb =
                juce::Decibels::gainToDecibels(ratio, 0.0f);

            const float inRatio = windowRmsIn[k] > windowRmsIn[k - 1]
                ? windowRmsIn[k] / windowRmsIn[k - 1]
                : windowRmsIn[k - 1] / windowRmsIn[k];
            const float inputChangeDb =
                juce::Decibels::gainToDecibels(inRatio, 0.0f);
            const bool isAttackTracking = (g1 < g0) && (inputChangeDb > 3.0f);
            if (isAttackTracking)
                continue;

            if (jerkDb > maxGainJerk) {
                maxGainJerk = jerkDb;
                maxGainJerkIdx = (int) k;
            }
        }

        const float gainUpDb = 20.0f * std::log10(1.0f + maxGainUp);
        std::cout << "  " << sig.name
                  << ": gain-up=" << gainUpDb << " dB"
                  << " jerk=" << maxGainJerk << " dB";
        if (maxGainUpIdx >= 0)
            std::cout << " (up@w" << maxGainUpIdx << ")";
        if (maxGainJerkIdx >= 0)
            std::cout << " (jerk@w" << maxGainJerkIdx << ")";
        std::cout << "\n";

        passed &= check(gainUpDb <= 0.5f,
                        (std::string(sig.name)
                         + ": limiter must not increase RMS").c_str());
        passed &= check(maxGainJerk <= 3.0f,
                        (std::string(sig.name)
                         + ": gain must not jerk >3 dB between windows").c_str());
    }

    return passed;
}
