#pragma once

#include <JuceHeader.h>
#include <memory>

class DonationTracker
{
public:
    DonationTracker();

    bool hasDonated() const noexcept;
    void checkDonationStatusAsync();
    void openDonationPage();

    static juce::String getMachineIdHash();

private:
    struct SharedState
    {
        juce::String machineIdHash;
        std::atomic<bool> donated { false };
        std::atomic<bool> requestInFlight { false };
    };

    static juce::String getRawMachineId();

    static bool loadLocalDonationFlag();
    static void saveLocalDonationFlag(bool value);

    static std::unique_ptr<juce::PropertiesFile> createPropertiesFile();

    std::shared_ptr<SharedState> state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DonationTracker)
};
