#pragma once

#include "PluginProcessor.h"
#include "UI/NeedleMeter.h"
#include "UI/AnalogKnob.h"
#include <JuceHeader.h>

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
    void drawChassis(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawScrews(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawVentSlots(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawLowCutIndicator(juce::Graphics& g, juce::Rectangle<float> bounds, float activity);

    MaxOxAudioProcessor& audioProcessor;
    AnalogKnobLookAndFeel knobLookAndFeel;

    juce::Slider gainSlider;
    NeedleMeter inputMeter { NeedleMeter::MeterType::Level };
    NeedleMeter outputMeter { NeedleMeter::MeterType::Level };
    NeedleMeter grMeter { NeedleMeter::MeterType::GainReduction };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    float lowCutActivity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaxOxAudioProcessorEditor)
};
