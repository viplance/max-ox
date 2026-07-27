#pragma once

#include <JuceHeader.h>

class AnalogKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnalogKnobLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    juce::Label* createSliderTextBox(juce::Slider& slider) override;
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
};
