#include <iostream>

bool runLimiterTests();
bool runAdaptiveLowCutTests();
bool runPeakShaverTests();
bool runGainAnomalyTests();
bool runTransparencyTests();

int main()
{
    bool passed = true;

    std::cout << "[LookAheadLimiter]\n";
    passed &= runLimiterTests();

    std::cout << "[AdaptiveLowCut]\n";
    passed &= runAdaptiveLowCutTests();

    std::cout << "[PerceptualPeakShaver]\n";
    passed &= runPeakShaverTests();

    std::cout << "[GainAnomaly]\n";
    passed &= runGainAnomalyTests();

    std::cout << "[Transparency]\n";
    passed &= runTransparencyTests();

    if (! passed) {
        std::cout << "\nSome tests FAILED\n";
        return 1;
    }

    std::cout << "\nAll MaxOx DSP tests passed\n";
    return 0;
}
