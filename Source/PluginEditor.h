#pragma once

#include "PluginConstants.h"
#include "PluginProcessor.h"
#include "DonationTracker.h"
#include "UI/NeedleMeter.h"
#include "UI/AnalogKnob.h"
#include <JuceHeader.h>

class DonateHyperlinkButton final : public juce::HyperlinkButton
{
public:
    using juce::HyperlinkButton::HyperlinkButton;

    void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
};

class PopupCloseButton final : public juce::Button
{
public:
    PopupCloseButton();

    void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
};

class SupportPromptComponent final : public juce::Component
{
public:
    SupportPromptComponent();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    std::function<void()> onDismiss;
    std::function<void()> onDonateClicked;

private:
    juce::Rectangle<int> getCardBounds() const;
    void dismiss();

    juce::Label message;
    juce::TextButton donateButton { "Donate to me" };
    PopupCloseButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SupportPromptComponent)
};

class SupportPromptState;

class MaxOxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit MaxOxAudioProcessorEditor(MaxOxAudioProcessor&);
    ~MaxOxAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyDonatedState();

    static constexpr int kDonationCheckIntervalFrames = 900;
    int donationCheckCounter = 0;
    void drawChassis(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawScrews(juce::Graphics& g, juce::Rectangle<float> bounds);

    MaxOxAudioProcessor& audioProcessor;
    AnalogKnobLookAndFeel knobLookAndFeel;

    DonateHyperlinkButton donateLink {
        "Donate to me",
        juce::URL(MaxOxConfig::kDonateUrl)
    };
    SupportPromptComponent supportPrompt;
    juce::Slider gainSlider;
    NeedleMeter inputMeter { NeedleMeter::MeterType::Level };
    NeedleMeter outputMeter { NeedleMeter::MeterType::Level };
    NeedleMeter grMeter { NeedleMeter::MeterType::GainReduction };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<SupportPromptState> supportPromptState;
    DonationTracker donationTracker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaxOxAudioProcessorEditor)
};
