#include "PluginEditor.h"
#include <cmath>

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

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "GAIN", gainSlider);

    addAndMakeVisible(gainSlider);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(grMeter);

    setSize(680, 380);
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

    drawLabel("M A X O X", { 0, 18, getWidth(), 32 }, cream, 22.0f);

    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.setColour(dimCream.withAlpha(0.5f));
    g.drawText("v" PLUGIN_VERSION, juce::Rectangle<int>(getWidth() - 80, 18, 70, 16), juce::Justification::centredRight);

    drawLabel("INPUT", inputMeter.getBounds().withY(inputMeter.getY() - 18).withHeight(14), dimCream, 10.0f);
    drawLabel("OUTPUT", outputMeter.getBounds().withY(outputMeter.getY() - 18).withHeight(14), dimCream, 10.0f);
    drawLabel("GAIN", grMeter.getBounds().withY(grMeter.getY() - 18).withHeight(14), dimCream, 10.0f);
    drawLabel("GAIN", gainSlider.getBounds().withY(gainSlider.getY() - 20).withHeight(16), cream, 12.0f);

    auto lcBounds = juce::Rectangle<float>(
        (float)gainSlider.getX() + (float)gainSlider.getWidth() * 0.5f - 30.0f,
        (float)gainSlider.getBottom() + 24.0f,
        60.0f, 12.0f);
    drawLowCutIndicator(g, lcBounds, lowCutActivity);

    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(dimCream.withAlpha(0.40f));
    g.drawText("by DJ Sher from Enotix",
        juce::Rectangle<int>(12, getHeight() - 24, 180, 16), juce::Justification::centredLeft);

    drawScrews(g, bounds);
    drawVentSlots(g, bounds);
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

    const int knobSize = 140;
    gainSlider.setBounds(cx - knobSize / 2, 210, knobSize, knobSize);
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
        juce::Colour::fromRGB(55, 52, 48), bounds.getCentreX(), bounds.getY(),
        juce::Colour::fromRGB(35, 33, 30), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(chassisGrad);
    g.fillAll();

    g.setColour(juce::Colour::fromRGB(70, 65, 58).withAlpha(0.15f));
    for (float yy = 0; yy < bounds.getHeight(); yy += 2.0f)
        g.drawHorizontalLine((int)yy, bounds.getX(), bounds.getRight());

    g.setColour(juce::Colour::fromRGB(80, 75, 65).withAlpha(0.4f));
    g.drawLine(bounds.getX(), 0.5f, bounds.getRight(), 0.5f, 1.0f);
    g.setColour(juce::Colour::fromRGB(20, 18, 16).withAlpha(0.6f));
    g.drawLine(bounds.getX(), bounds.getBottom() - 0.5f, bounds.getRight(), bounds.getBottom() - 0.5f, 1.0f);

    float panelY = 64.0f;
    float panelH = 135.0f;
    auto panelBounds = juce::Rectangle<float>(20.0f, panelY, bounds.getWidth() - 40.0f, panelH);

    g.setColour(juce::Colour::fromRGB(25, 23, 20).withAlpha(0.4f));
    g.fillRoundedRectangle(panelBounds, 4.0f);
    g.setColour(juce::Colour::fromRGB(80, 75, 65).withAlpha(0.25f));
    g.drawRoundedRectangle(panelBounds, 4.0f, 0.5f);
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

void MaxOxAudioProcessorEditor::drawVentSlots(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float slotY = bounds.getBottom() - 36.0f;
    float slotWidth = 30.0f;
    float slotHeight = 3.0f;
    float spacing = 6.0f;
    int numSlots = 5;

    float totalW = (float)numSlots * slotWidth + (float)(numSlots - 1) * spacing;
    float startX = bounds.getCentreX() - totalW * 0.5f;

    for (int i = 0; i < numSlots; ++i) {
        float sx = startX + (float)i * (slotWidth + spacing);
        g.setColour(juce::Colour::fromRGB(15, 14, 12).withAlpha(0.6f));
        g.fillRoundedRectangle(sx, slotY, slotWidth, slotHeight, 1.5f);
        g.setColour(juce::Colour::fromRGB(70, 65, 58).withAlpha(0.2f));
        g.drawRoundedRectangle(sx, slotY, slotWidth, slotHeight, 1.5f, 0.5f);
    }
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

    g.setFont(juce::Font(juce::FontOptions(8.0f)));
    g.setColour(juce::Colour::fromRGB(20, 18, 16));
    g.drawText("LF CUT", bounds, juce::Justification::centred);
}
