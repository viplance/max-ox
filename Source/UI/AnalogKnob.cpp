#include "AnalogKnob.h"
#include <cmath>

AnalogKnobLookAndFeel::AnalogKnobLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(225, 215, 195));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void AnalogKnobLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider&)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
    auto centre = bounds.getCentre();
    centre.y += 24.0f;
    float radius = juce::jmin(
        45.0f,
        juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.38f);

    {
        juce::ColourGradient shadow(
            juce::Colours::black.withAlpha(0.25f), centre.x, centre.y + 2.0f,
            juce::Colours::transparentBlack, centre.x, centre.y + radius + 8.0f, true);
        g.setGradientFill(shadow);
        g.fillEllipse(centre.x - radius - 4.0f, centre.y - radius - 2.0f,
                       (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);
    }

    {
        juce::ColourGradient knobGrad(
            juce::Colour::fromRGB(85, 80, 72), centre.x - radius * 0.5f, centre.y - radius * 0.5f,
            juce::Colour::fromRGB(40, 36, 32), centre.x + radius * 0.5f, centre.y + radius * 0.5f, true);
        g.setGradientFill(knobGrad);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    }

    {
        juce::ColourGradient rimGrad(
            juce::Colour::fromRGB(100, 95, 85).withAlpha(0.6f), centre.x, centre.y - radius,
            juce::Colour::fromRGB(30, 28, 25).withAlpha(0.6f), centre.x, centre.y + radius, false);
        g.setGradientFill(rimGrad);
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
    }

    {
        float innerRadius = radius * 0.85f;
        juce::ColourGradient innerGrad(
            juce::Colour::fromRGB(70, 65, 58), centre.x, centre.y - innerRadius * 0.5f,
            juce::Colour::fromRGB(48, 44, 38), centre.x, centre.y + innerRadius * 0.5f, false);
        g.setGradientFill(innerGrad);
        g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius,
                       innerRadius * 2.0f, innerRadius * 2.0f);
    }

    float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    float pointerLength = radius * 0.72f;
    auto pointerTip = centre + juce::Point<float>(std::sin(angle), -std::cos(angle)) * pointerLength;
    auto pointerBase = centre + juce::Point<float>(std::sin(angle), -std::cos(angle)) * (radius * 0.2f);

    g.setColour(juce::Colour::fromRGB(220, 200, 170));
    g.drawLine(pointerBase.x, pointerBase.y, pointerTip.x, pointerTip.y, 2.5f);

    g.setColour(juce::Colour::fromRGB(255, 240, 210).withAlpha(0.7f));
    g.drawLine(pointerBase.x, pointerBase.y, pointerTip.x, pointerTip.y, 1.2f);

    int numTicks = 13;
    float tickRadius = radius + 10.0f;
    float tickOuterRadius = radius + 18.0f;
    for (int i = 0; i < numTicks; ++i) {
        float tickNorm = (float)i / (float)(numTicks - 1);
        float tickAngle = rotaryStartAngle + tickNorm * (rotaryEndAngle - rotaryStartAngle);

        bool isMajor = (i % 3 == 0);
        float inner = isMajor ? tickRadius - 2.0f : tickRadius;
        float outer = isMajor ? tickOuterRadius : tickOuterRadius - 4.0f;

        auto innerPt = centre + juce::Point<float>(std::sin(tickAngle), -std::cos(tickAngle)) * inner;
        auto outerPt = centre + juce::Point<float>(std::sin(tickAngle), -std::cos(tickAngle)) * outer;

        g.setColour(juce::Colour::fromRGB(200, 190, 170).withAlpha(isMajor ? 0.9f : 0.4f));
        g.drawLine(innerPt.x, innerPt.y, outerPt.x, outerPt.y, isMajor ? 1.5f : 0.8f);

        if (isMajor) {
            float dbVal = tickNorm * 24.0f;
            auto labelPt = centre + juce::Point<float>(std::sin(tickAngle), -std::cos(tickAngle)) * (tickOuterRadius + 10.0f);
            auto labelBounds = juce::Rectangle<float>(
                labelPt.x - 16.0f, labelPt.y - 6.0f, 32.0f, 12.0f);

            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(juce::Colour::fromRGB(200, 190, 170).withAlpha(0.85f));
            g.drawText(juce::String((int)dbVal),
                labelBounds,
                juce::Justification::centred);
        }
    }
}

void AnalogKnobLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(juce::Colour::fromRGB(225, 215, 195));
    g.drawText(label.getText(), label.getLocalBounds(), juce::Justification::centred);
}
