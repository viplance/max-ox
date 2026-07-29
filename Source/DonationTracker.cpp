#include "DonationTracker.h"
#include <thread>

#if JUCE_MAC
extern std::string getIOPlatformUUID();
#endif

namespace
{
constexpr auto kCloudFunctionBase =
    "https://us-central1-enotix.cloudfunctions.net/djsher-donate";
constexpr auto kDonationStateKey = "machineHasDonated";
constexpr int kRequestTimeoutMs = 5000;
}

DonationTracker::DonationTracker()
    : state(std::make_shared<SharedState>())
{
    state->machineIdHash = getMachineIdHash();
    state->donated.store(loadLocalDonationFlag(), std::memory_order_relaxed);
}

bool DonationTracker::hasDonated() const noexcept
{
    return state->donated.load(std::memory_order_relaxed);
}

juce::String DonationTracker::getRawMachineId()
{
#if JUCE_MAC
    auto uuid = getIOPlatformUUID();
    if (!uuid.empty())
        return juce::String(uuid);
#endif

    return juce::SystemStats::getUniqueDeviceID();
}

juce::String DonationTracker::getMachineIdHash()
{
    const auto raw = getRawMachineId();
    const auto salted = raw + "MaxOx.DonationTracker.2026.SharkoAudio";
    return juce::SHA256(salted.toUTF8()).toHexString();
}

std::unique_ptr<juce::PropertiesFile> DonationTracker::createPropertiesFile()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "MaxOx";
    options.filenameSuffix = "donation";
    options.folderName = "SharkoAudio";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsCompressedBinary;

    return std::make_unique<juce::PropertiesFile>(options);
}

bool DonationTracker::loadLocalDonationFlag()
{
    auto props = createPropertiesFile();
    if (props == nullptr || !props->isValidFile())
        return false;

    return props->getBoolValue(kDonationStateKey, false);
}

void DonationTracker::saveLocalDonationFlag(bool value)
{
    auto props = createPropertiesFile();
    if (props == nullptr || !props->isValidFile())
        return;

    props->setValue(kDonationStateKey, value);
    props->saveIfNeeded();
}

void DonationTracker::checkDonationStatusAsync()
{
    if (state->donated.load(std::memory_order_relaxed))
        return;

    bool expected = false;
    if (!state->requestInFlight.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
        return;

    auto sharedState = state;
    std::thread([sharedState]() {
        struct RequestGuard
        {
            std::shared_ptr<SharedState> state;
            ~RequestGuard()
            {
                state->requestInFlight.store(false, std::memory_order_release);
            }
        } guard { sharedState };

        auto url = juce::URL(
            juce::String(kCloudFunctionBase) + "/check?mid="
            + sharedState->machineIdHash);

        int statusCode = 0;
        auto stream = url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(kRequestTimeoutMs)
                .withExtraHeaders("Accept: application/json")
                .withStatusCode(&statusCode));

        if (stream == nullptr || statusCode != 200)
            return;

        auto response = stream->readEntireStreamAsString();
        if (response.contains("\"donated\":true"))
        {
            sharedState->donated.store(true, std::memory_order_release);
            saveLocalDonationFlag(true);
        }
    }).detach();
}

void DonationTracker::openDonationPage()
{
    juce::URL(
        juce::String(kCloudFunctionBase) + "/donate?mid="
        + state->machineIdHash)
        .launchInDefaultBrowser();
}
