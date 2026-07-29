#include "PluginEditor.h"
#include "DonationTracker.h"
#include <cmath>
#include <cstdint>

namespace
{
constexpr auto kSupportPromptStateKey = "supportPromptSchedule";
constexpr auto kSupportPromptStateVersion = "2";
constexpr auto kSupportPromptIntegritySalt =
    "MaxOx.SupportPrompt.2026.7.SharkoAudio";
}

class SupportPromptState
{
public:
    SupportPromptState()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "MaxOx";
        options.filenameSuffix = "support";
        options.folderName = "SharkoAudio";
        options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = 0;
        options.storageFormat = juce::PropertiesFile::storeAsCompressedBinary;

        properties = std::make_unique<juce::PropertiesFile>(options);

        const auto now = juce::Time::currentTimeMillis();
        if (!load()) {
            nextPromptAtMs = now + MaxOxConfig::kInitialSupportPromptDelayMs;
            repeatDelayMs = MaxOxConfig::kInitialSupportPromptDelayMs;
            save();
        }
    }

    bool isDue(std::int64_t now) const noexcept
    {
        return now >= nextPromptAtMs;
    }

    void markPromptShown(std::int64_t now)
    {
        nextPromptAtMs = now + repeatDelayMs;
        save();
    }

    void shortenRemainingTimeOnClose(std::int64_t now)
    {
        repeatDelayMs = juce::jmax(
            MaxOxConfig::kMinimumSupportPromptDelayMs,
            repeatDelayMs / MaxOxConfig::kSupportPromptCloseDivisor);

        const auto remainingMs = nextPromptAtMs - now;
        nextPromptAtMs = now + juce::jmax(
            MaxOxConfig::kMinimumSupportPromptDelayMs,
            remainingMs > 0
                ? remainingMs / MaxOxConfig::kSupportPromptCloseDivisor
                : repeatDelayMs);
        save();
    }

private:
    static juce::String createSignature(const juce::String& payload)
    {
        const auto signedPayload = payload + kSupportPromptIntegritySalt;
        return juce::SHA256(signedPayload.toUTF8()).toHexString();
    }

    bool load()
    {
        if (properties == nullptr || !properties->isValidFile())
            return false;

        const auto encoded = properties->getValue(kSupportPromptStateKey);
        if (encoded.isEmpty())
            return false;

        juce::MemoryBlock bytes;
        if (!bytes.fromBase64Encoding(encoded))
            return false;

        const auto stored = juce::String::fromUTF8(
            static_cast<const char*>(bytes.getData()),
            static_cast<int>(bytes.getSize()));
        const auto signatureSeparator = stored.lastIndexOfChar('|');
        if (signatureSeparator <= 0)
            return false;

        const auto payload = stored.substring(0, signatureSeparator);
        const auto signature = stored.substring(signatureSeparator + 1);
        if (signature != createSignature(payload))
            return false;

        juce::StringArray fields;
        fields.addTokens(payload, "|", {});
        if (fields.size() != 3 || fields[2] != kSupportPromptStateVersion)
            return false;

        const auto loadedNextPromptAtMs = fields[0].getLargeIntValue();
        const auto loadedRepeatDelayMs = fields[1].getLargeIntValue();
        if (loadedNextPromptAtMs <= 0
            || loadedRepeatDelayMs < MaxOxConfig::kMinimumSupportPromptDelayMs
            || loadedRepeatDelayMs > MaxOxConfig::kInitialSupportPromptDelayMs)
            return false;

        nextPromptAtMs = loadedNextPromptAtMs;
        repeatDelayMs = loadedRepeatDelayMs;
        return true;
    }

    void save()
    {
        if (properties == nullptr || !properties->isValidFile())
            return;

        const auto payload =
            juce::String(nextPromptAtMs) + "|"
            + juce::String(repeatDelayMs) + "|"
            + kSupportPromptStateVersion;
        const auto stored = payload + "|" + createSignature(payload);
        const juce::MemoryBlock bytes(
            stored.toRawUTF8(), stored.getNumBytesAsUTF8());

        properties->setValue(
            kSupportPromptStateKey, bytes.toBase64Encoding());
        properties->saveIfNeeded();
    }

    std::unique_ptr<juce::PropertiesFile> properties;
    std::int64_t nextPromptAtMs = 0;
    std::int64_t repeatDelayMs =
        MaxOxConfig::kInitialSupportPromptDelayMs;
};

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

PopupCloseButton::PopupCloseButton()
    : juce::Button("Close support prompt")
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void PopupCloseButton::paintButton(
    juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto colour = juce::Colour::fromRGB(115, 108, 104);
    if (isMouseOverButton)
        colour = colour.brighter(isButtonDown ? 0.05f : 0.25f);

    const auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    g.setColour(colour);
    g.drawLine(
        juce::Line<float>(bounds.getTopLeft(), bounds.getBottomRight()),
        2.0f);
    g.drawLine(
        juce::Line<float>(bounds.getTopRight(), bounds.getBottomLeft()),
        2.0f);
}

SupportPromptComponent::SupportPromptComponent()
{
    setInterceptsMouseClicks(true, true);

    message.setText(
        "Do you like the MaxOx plugin?\nSupport the developer!",
        juce::dontSendNotification);
    message.setFont(juce::Font(juce::FontOptions(20.0f)));
    message.setColour(
        juce::Label::textColourId, juce::Colour::fromRGB(55, 20, 28));
    message.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(message);

    donateButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    donateButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour::fromRGB(150, 28, 48));
    donateButton.setColour(
        juce::TextButton::buttonOnColourId,
        juce::Colour::fromRGB(178, 38, 59));
    donateButton.setColour(
        juce::TextButton::textColourOffId,
        juce::Colour::fromRGB(245, 238, 220));
    donateButton.setColour(
        juce::TextButton::textColourOnId,
        juce::Colours::white);
    donateButton.onClick = [this] {
        if (onDonateClicked)
            onDonateClicked();
        dismiss();
    };
    addAndMakeVisible(donateButton);

    closeButton.onClick = [this] { dismiss(); };
    addAndMakeVisible(closeButton);
}

juce::Rectangle<int> SupportPromptComponent::getCardBounds() const
{
    return getLocalBounds().withSizeKeepingCentre(420, 174);
}

void SupportPromptComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black.withAlpha(0.52f));
    g.fillAll();

    auto card = getCardBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillRoundedRectangle(card.translated(0.0f, 4.0f), 10.0f);

    juce::ColourGradient background(
        juce::Colour::fromRGB(245, 238, 220),
        card.getCentreX(), card.getY(),
        juce::Colour::fromRGB(225, 215, 195),
        card.getCentreX(), card.getBottom(), false);
    g.setGradientFill(background);
    g.fillRoundedRectangle(card, 10.0f);

    g.setColour(juce::Colour::fromRGB(118, 22, 39).withAlpha(0.7f));
    g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);
}

void SupportPromptComponent::resized()
{
    const auto card = getCardBounds();
    message.setBounds(card.getX() + 28, card.getY() + 22,
                      card.getWidth() - 56, 66);
    donateButton.setBounds(card.getCentreX() - 66, card.getY() + 108,
                           132, 34);
    closeButton.setBounds(card.getRight() - 37, card.getY() + 9, 28, 28);
}

void SupportPromptComponent::mouseDown(const juce::MouseEvent& event)
{
    if (!getCardBounds().contains(event.getPosition()))
        dismiss();
}

void SupportPromptComponent::dismiss()
{
    if (!isVisible())
        return;

    setVisible(false);
    if (onDismiss)
        onDismiss();
}

MaxOxAudioProcessorEditor::MaxOxAudioProcessorEditor(MaxOxAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    supportPromptState = std::make_unique<SupportPromptState>();

    gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setRange(0.0, 24.0, 0.1);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
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
    donateLink.setURL({});
    donateLink.onClick = [this] {
        donationTracker.openDonationPage();
    };
    addAndMakeVisible(donateLink);

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "GAIN", gainSlider);

    addAndMakeVisible(gainSlider);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(grMeter);
    addChildComponent(supportPrompt);
    supportPrompt.onDismiss = [this] {
        if (supportPromptState != nullptr)
            supportPromptState->shortenRemainingTimeOnClose(
                juce::Time::currentTimeMillis());
    };
    supportPrompt.onDonateClicked = [this] {
        donationTracker.openDonationPage();
    };

    if (donationTracker.hasDonated())
        applyDonatedState();

    donationTracker.checkDonationStatusAsync();

    setSize(680, 410);
    startTimerHz(30);
}

MaxOxAudioProcessorEditor::~MaxOxAudioProcessorEditor()
{
    stopTimer();

    if (supportPromptState != nullptr)
        supportPromptState->shortenRemainingTimeOnClose(
            juce::Time::currentTimeMillis());

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
    g.drawText("v" PLUGIN_VERSION, juce::Rectangle<int>(getWidth() - 100, 17, 80, 19), juce::Justification::centredRight);

    drawLabel("INPUT", inputMeter.getBounds().withY(inputMeter.getY() - 19).withHeight(15), dimCream, 12.0f);
    drawLabel("OUTPUT", outputMeter.getBounds().withY(outputMeter.getY() - 19).withHeight(15), dimCream, 12.0f);
    drawLabel("LIMITING", grMeter.getBounds().withY(grMeter.getY() - 19).withHeight(15), dimCream, 12.0f);
    drawLabel("GAIN", gainSlider.getBounds().withY(gainSlider.getY() + 4).withHeight(18), cream, 14.0f);

    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(dimCream.withAlpha(0.40f));
    g.drawText("by DJ Sher from Enotix",
        juce::Rectangle<int>(20, getHeight() - 27, 210, 19), juce::Justification::centredLeft);

    drawScrews(g, bounds);
}

void MaxOxAudioProcessorEditor::resized()
{
    const int cx = getWidth() / 2;
    const int meterW = 160;
    const int meterH = 100;
    const int verticalOffset = 14;
    const int meterY = 78 + verticalOffset;

    inputMeter.setBounds(30, meterY, meterW, meterH);
    grMeter.setBounds(cx - meterW / 2, meterY, meterW, meterH);
    outputMeter.setBounds(getWidth() - meterW - 30, meterY, meterW, meterH);

    donateLink.setBounds(32, 17, 110, 20);
    supportPrompt.setBounds(getLocalBounds());

    const int knobWidth = 190;
    const int knobHeight = 168;
    gainSlider.setBounds(
        cx - knobWidth / 2, 185 + verticalOffset, knobWidth, knobHeight);
}

void MaxOxAudioProcessorEditor::timerCallback()
{
    inputMeter.setLevelDb(audioProcessor.getInputLevelDb());
    outputMeter.setLevelDb(audioProcessor.getOutputLevelDb());
    grMeter.setLevelDb(audioProcessor.getGainReductionDb());

    if (donationTracker.hasDonated())
    {
        if (donateLink.isVisible())
            applyDonatedState();
        return;
    }

    if (++donationCheckCounter >= kDonationCheckIntervalFrames)
    {
        donationCheckCounter = 0;
        donationTracker.checkDonationStatusAsync();
    }

    if (!supportPrompt.isVisible()
        && supportPromptState != nullptr
        && supportPromptState->isDue(juce::Time::currentTimeMillis())) {
        supportPromptState->markPromptShown(
            juce::Time::currentTimeMillis());
        supportPrompt.setVisible(true);
        supportPrompt.toFront(false);
    }
}

void MaxOxAudioProcessorEditor::applyDonatedState()
{
    donateLink.setVisible(false);
    supportPrompt.setVisible(false);
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
