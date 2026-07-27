#include "PluginEditor.h"
#include <cmath>
#include <cstdint>

void DonateHyperlinkButton::paintButton(
    juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto colour = findColour(juce::HyperlinkButton::textColourId);
    if (isMouseOverButton)
        colour = colour.darker(isButtonDown ? 1.3f : 0.4f);
    else if (!isEnabled())
        colour = colour.withMultipliedAlpha(0.4f);

    const auto font = juce::Font(juce::FontOptions(13.0f));
    const auto textBounds = getLocalBounds().reduced(1, 0);
    const auto textWidth =
        juce::GlyphArrangement::getStringWidthInt(font, getButtonText());

    g.setColour(colour);
    g.setFont(font);
    g.drawText(
        getButtonText(), textBounds,
        juce::Justification::centredLeft | juce::Justification::verticallyCentred,
        true);

    const float underlineY = (float)getHeight() - 1.5f;
    g.drawLine(
        (float)textBounds.getX(), underlineY,
        (float)(textBounds.getX() + textWidth), underlineY,
        1.0f);
}

MaxOxAudioProcessorEditor::MaxOxAudioProcessorEditor(MaxOxAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setRange(0.0, 24.0, 0.1);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 22);
    gainSlider.setTextValueSuffix(" dB");
    gainSlider.setRotaryParameters(
        juce::MathConstants<float>::pi * 1.25f,
        juce::MathConstants<float>::pi * 2.75f, true);
    gainSlider.setLookAndFeel(&knobLookAndFeel);
    gainSlider.setMouseCursor(juce::MouseCursor::PointingHandCursor);

    donateLink.setFont(
        juce::Font(juce::FontOptions(13.0f)),
        false,
        juce::Justification::centredLeft);
    donateLink.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    donateLink.setColour(
        juce::HyperlinkButton::textColourId,
        juce::Colour::fromRGB(225, 215, 195).withAlpha(0.78f));
    addAndMakeVisible(donateLink);

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "GAIN", gainSlider);

    addAndMakeVisible(gainSlider);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(grMeter);

    setSize(680, 410);
    startTimerHz(30);
}

MaxOxAudioProcessorEditor::~MaxOxAudioProcessorEditor()
{
    gainSlider.setLookAndFeel(nullptr);
}

void MaxOxAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawChassis(g, bounds);

    auto drawLabel = [&g](const juce::String& text, juce::Rectangle<int> area,
                          juce::Colour colour, float fontSize) {
        g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));
        g.setColour(colour.withAlpha(0.12f));
        g.drawText(text, area.translated(0, 1), juce::Justification::centred);
        g.setColour(colour);
        g.drawText(text, area, juce::Justification::centred);
    };

    auto cream = juce::Colour::fromRGB(225, 215, 195);
    auto dimCream = cream.withAlpha(0.65f);

    drawLabel("M A X O X", { 0, 18, getWidth(), 32 }, cream, 24.0f);

    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(dimCream.withAlpha(0.5f));
    g.drawText("v" PLUGIN_VERSION, juce::Rectangle<int>(getWidth() - 90, 17, 80, 19), juce::Justification::centredRight);

    drawLabel("INPUT", inputMeter.getBounds().withY(inputMeter.getY() - 19).withHeight(15), dimCream, 12.0f);
    drawLabel("OUTPUT", outputMeter.getBounds().withY(outputMeter.getY() - 19).withHeight(15), dimCream, 12.0f);
    drawLabel("LIMITING", grMeter.getBounds().withY(grMeter.getY() - 19).withHeight(15), dimCream, 12.0f);
    drawLabel("GAIN", gainSlider.getBounds().withY(gainSlider.getY() + 4).withHeight(18), cream, 14.0f);

    auto lcBounds = juce::Rectangle<float>(
        (float)gainSlider.getX() + (float)gainSlider.getWidth() * 0.5f - 30.0f,
        (float)gainSlider.getBottom() + 24.0f,
        60.0f, 12.0f);
    drawLowCutIndicator(g, lcBounds, lowCutActivity);

    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(dimCream.withAlpha(0.40f));
    g.drawText("by DJ Sher from Enotix",
        juce::Rectangle<int>(12, getHeight() - 27, 210, 19), juce::Justification::centredLeft);

    drawScrews(g, bounds);
}

void MaxOxAudioProcessorEditor::resized()
{
    const int cx = getWidth() / 2;
    const int meterW = 160;
    const int meterH = 100;
    const int meterY = 78;

    inputMeter.setBounds(30, meterY, meterW, meterH);
    grMeter.setBounds(cx - meterW / 2, meterY, meterW, meterH);
    outputMeter.setBounds(getWidth() - meterW - 30, meterY, meterW, meterH);

    donateLink.setBounds(24, 17, 110, 20);

    const int knobSize = 190;
    gainSlider.setBounds(cx - knobSize / 2, 185, knobSize, knobSize);
}

void MaxOxAudioProcessorEditor::timerCallback()
{
    inputMeter.setLevelDb(audioProcessor.getInputLevelDb());
    outputMeter.setLevelDb(audioProcessor.getOutputLevelDb());
    grMeter.setLevelDb(audioProcessor.getGainReductionDb());

    float newActivity = audioProcessor.getLowCutActivity();
    if (std::abs(newActivity - lowCutActivity) > 0.005f) {
        lowCutActivity = newActivity;
        repaint();
    }
}

void MaxOxAudioProcessorEditor::drawChassis(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient chassisGrad(
        juce::Colour::fromRGB(118, 22, 39), bounds.getCentreX(), bounds.getY(),
        juce::Colour::fromRGB(55, 6, 19), bounds.getCentreX(), bounds.getBottom(), false);
    chassisGrad.addColour(0.38, juce::Colour::fromRGB(96, 14, 32));
    chassisGrad.addColour(0.72, juce::Colour::fromRGB(73, 9, 25));
    g.setGradientFill(chassisGrad);
    g.fillAll();

    // A broad reflection gives the flat colour the curved response of coated metal.
    juce::ColourGradient reflection(
        juce::Colour::fromRGB(24, 2, 9).withAlpha(0.17f),
        bounds.getX(), bounds.getCentreY(),
        juce::Colour::fromRGB(25, 2, 9).withAlpha(0.20f),
        bounds.getRight(), bounds.getCentreY(), false);
    reflection.addColour(
        0.22, juce::Colour::fromRGB(185, 75, 88).withAlpha(0.045f));
    reflection.addColour(
        0.48, juce::Colour::fromRGB(220, 112, 120).withAlpha(0.13f));
    reflection.addColour(
        0.68, juce::Colour::fromRGB(151, 48, 65).withAlpha(0.035f));
    g.setGradientFill(reflection);
    g.fillRect(bounds);

    // Fine deterministic grain: bright and dark hairlines simulate brushed steel.
    const int top = (int) bounds.getY();
    const int bottom = (int) bounds.getBottom();
    const int left = (int) bounds.getX();
    const int width = (int) bounds.getWidth();
    for (int y = top; y < bottom; ++y) {
        std::uint32_t hash =
            (std::uint32_t) (y + 1) * 747796405u + 2891336453u;
        hash = ((hash >> ((hash >> 28u) + 4u)) ^ hash) * 277803737u;
        hash = (hash >> 22u) ^ hash;

        const float grain =
            (float) (hash & 0xffu) / 255.0f * 2.0f - 1.0f;
        if (grain >= 0.0f)
            g.setColour(
                juce::Colour::fromRGB(232, 144, 148)
                    .withAlpha(0.012f + grain * 0.038f));
        else
            g.setColour(
                juce::Colour::fromRGB(25, 2, 9)
                    .withAlpha(0.012f - grain * 0.040f));
        g.drawHorizontalLine(y, bounds.getX(), bounds.getRight());

        // Occasional short strokes break up perfect banding without visual noise.
        if ((hash & 0x0fu) == 0u) {
            const float x =
                (float) left + (float) ((hash >> 8u) % (std::uint32_t) width);
            const float length =
                24.0f + (float) ((hash >> 17u) & 0x7fu);
            g.setColour(
                juce::Colour::fromRGB(238, 158, 160).withAlpha(0.075f));
            g.drawHorizontalLine(
                y, x, juce::jmin(bounds.getRight(), x + length));
        }
    }

    g.setColour(juce::Colour::fromRGB(185, 74, 87).withAlpha(0.45f));
    g.drawLine(bounds.getX(), 0.5f, bounds.getRight(), 0.5f, 1.0f);
    g.setColour(juce::Colour::fromRGB(31, 3, 10).withAlpha(0.75f));
    g.drawLine(bounds.getX(), bounds.getBottom() - 0.5f, bounds.getRight(), bounds.getBottom() - 0.5f, 1.0f);

}

void MaxOxAudioProcessorEditor::drawScrews(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto drawScrew = [&g](float cx, float cy) {
        float r = 6.0f;
        juce::ColourGradient screwGrad(
            juce::Colour::fromRGB(90, 85, 78), cx - 2.0f, cy - 2.0f,
            juce::Colour::fromRGB(45, 42, 38), cx + 2.0f, cy + 2.0f, true);
        g.setGradientFill(screwGrad);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

        g.setColour(juce::Colour::fromRGB(30, 28, 25).withAlpha(0.5f));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);

        g.setColour(juce::Colour::fromRGB(60, 55, 50).withAlpha(0.6f));
        g.drawLine(cx - 3.0f, cy, cx + 3.0f, cy, 1.0f);
        g.drawLine(cx, cy - 3.0f, cx, cy + 3.0f, 1.0f);
    };

    float margin = 14.0f;
    drawScrew(margin, margin);
    drawScrew(bounds.getRight() - margin, margin);
    drawScrew(margin, bounds.getBottom() - margin);
    drawScrew(bounds.getRight() - margin, bounds.getBottom() - margin);
}

void MaxOxAudioProcessorEditor::drawLowCutIndicator(juce::Graphics& g, juce::Rectangle<float> bounds, float act)
{
    auto activeColor = juce::Colour::fromRGB(180, 140, 60);
    auto dimColor = juce::Colour::fromRGB(80, 70, 55);

    float alpha = 0.3f + act * 0.7f;
    auto color = dimColor.interpolatedWith(activeColor, act);

    g.setColour(color.withAlpha(alpha * 0.3f));
    g.fillRoundedRectangle(bounds.expanded(2.0f), 3.0f);

    g.setColour(color.withAlpha(alpha));
    g.fillRoundedRectangle(bounds, 2.0f);

    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(juce::Colour::fromRGB(20, 18, 16));
    g.drawText("LF CUT", bounds, juce::Justification::centred);
}
