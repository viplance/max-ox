#include "NeedleMeter.h"
#include <cmath>

NeedleMeter::NeedleMeter(MeterType type) : meterType(type) {}

void NeedleMeter::setLevelDb(float db)
{
    float minDb, maxDb;
    if (meterType == MeterType::GainReduction) {
        minDb = -kGrMaxReductionDb;
        maxDb = 0.0f;
    } else {
        minDb = kMinDb;
        maxDb = kMaxDb;
    }

    const auto next = juce::jlimit(minDb, maxDb, db);
    smoothedDb += kVisualSmoothing * (next - smoothedDb);

    if (std::abs(smoothedDb - levelDb) > 0.02f) {
        levelDb = smoothedDb;
        repaint();
    }
}

void NeedleMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    drawFaceplate(g, bounds);

    float normalised;
    if (meterType == MeterType::GainReduction) {
        normalised = juce::jmap(levelDb, -kGrMaxReductionDb, 0.0f, 0.0f, 1.0f);
    } else {
        normalised = juce::jmap(levelDb, kMinDb, kMaxDb, 0.0f, 1.0f);
    }
    normalised = juce::jlimit(0.0f, 1.0f, normalised);

    drawNeedle(g, bounds, normalised);
}

void NeedleMeter::drawFaceplate(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient bg(
        juce::Colour::fromRGB(245, 238, 220), bounds.getCentreX(), bounds.getY(),
        juce::Colour::fromRGB(225, 215, 195), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour::fromRGB(90, 80, 65).withAlpha(0.6f));
    g.drawRoundedRectangle(bounds, 6.0f, 1.5f);

    g.setColour(juce::Colour::fromRGB(180, 170, 150).withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 0.5f);

    auto pivot = juce::Point<float>(bounds.getCentreX(), bounds.getBottom() - 12.0f);
    float radius = bounds.getWidth() * 0.38f;
    drawScaleMarks(g, pivot, radius);
    drawDbLabel(g, pivot);
}

void NeedleMeter::drawDbLabel(juce::Graphics& g, juce::Point<float> pivot)
{
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::italic)));
    g.setColour(juce::Colour::fromRGB(60, 50, 40).withAlpha(0.7f));
    g.drawText("dB",
        juce::Rectangle<float>(pivot.x - 12.0f, pivot.y - 18.0f, 24.0f, 12.0f),
        juce::Justification::centred);
}

void NeedleMeter::drawScaleMarks(juce::Graphics& g, juce::Point<float> pivot, float radius)
{
    struct ScaleMark { float db; const char* label; bool major; };

    if (meterType == MeterType::GainReduction) {
        ScaleMark marks[] = {
            { 0.0f, "0", true },
            { -1.0f, nullptr, false },
            { -2.0f, nullptr, false },
            { -3.0f, "3", true },
            { -4.0f, nullptr, false },
            { -5.0f, nullptr, false },
            { -6.0f, "6", true },
            { -9.0f, "9", true },
            { -12.0f, "12", true },
        };

        for (auto& m : marks) {
            float norm = juce::jmap(m.db, -kGrMaxReductionDb, 0.0f, 0.0f, 1.0f);
            float angle = kNeedleAngleStart + norm * (kNeedleAngleEnd - kNeedleAngleStart);

            float outerR = radius + 6.0f;
            float innerR = m.major ? radius - 2.0f : radius + 1.0f;
            float labelR = radius + 18.0f;

            auto dir = juce::Point<float>(std::sin(angle), -std::cos(angle));
            auto outerPt = pivot + dir * outerR;
            auto innerPt = pivot + dir * innerR;
            auto labelPt = pivot + dir * labelR;

            g.setColour(juce::Colour::fromRGB(60, 50, 40).withAlpha(m.major ? 0.8f : 0.35f));
            g.drawLine(innerPt.x, innerPt.y, outerPt.x, outerPt.y, m.major ? 1.5f : 0.8f);

            if (m.major && m.label != nullptr) {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(juce::Colour::fromRGB(60, 50, 40).withAlpha(0.9f));
                g.drawText(m.label,
                    juce::Rectangle<float>(labelPt.x - 14.0f, labelPt.y - 6.0f, 28.0f, 12.0f),
                    juce::Justification::centred);
            }
        }
    } else {
        ScaleMark marks[] = {
            { -48.0f, "-48", false },
            { -36.0f, "-36", false },
            { -24.0f, "-24", true },
            { -18.0f, "-18", true },
            { -12.0f, "-12", true },
            { -9.0f, nullptr, false },
            { -6.0f, "-6", true },
            { -3.0f, "-3", true },
            { 0.0f, "0", true },
        };

        for (auto& m : marks) {
            float norm = juce::jmap(m.db, kMinDb, kMaxDb, 0.0f, 1.0f);
            float angle = kNeedleAngleStart + norm * (kNeedleAngleEnd - kNeedleAngleStart);

            float outerR = radius + 6.0f;
            float innerR = m.major ? radius - 2.0f : radius + 1.0f;
            float labelR = radius + 18.0f;

            auto dir = juce::Point<float>(std::sin(angle), -std::cos(angle));
            auto outerPt = pivot + dir * outerR;
            auto innerPt = pivot + dir * innerR;
            auto labelPt = pivot + dir * labelR;

            bool isRed = m.db > 0.0f;
            g.setColour(isRed
                ? juce::Colour::fromRGB(180, 50, 40).withAlpha(m.major ? 0.9f : 0.5f)
                : juce::Colour::fromRGB(60, 50, 40).withAlpha(m.major ? 0.8f : 0.35f));
            g.drawLine(innerPt.x, innerPt.y, outerPt.x, outerPt.y, m.major ? 1.5f : 0.8f);

            if (m.major && m.label != nullptr) {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(isRed
                    ? juce::Colour::fromRGB(180, 50, 40).withAlpha(0.9f)
                    : juce::Colour::fromRGB(60, 50, 40).withAlpha(0.9f));
                g.drawText(m.label,
                    juce::Rectangle<float>(labelPt.x - 14.0f, labelPt.y - 6.0f, 28.0f, 12.0f),
                    juce::Justification::centred);
            }
        }
    }
}

void NeedleMeter::drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds, float normalised)
{
    auto pivot = juce::Point<float>(bounds.getCentreX(), bounds.getBottom() - 12.0f);
    float needleLength = bounds.getWidth() * 0.38f;

    float angle = kNeedleAngleStart + normalised * (kNeedleAngleEnd - kNeedleAngleStart);

    auto tip = pivot + juce::Point<float>(std::sin(angle), -std::cos(angle)) * needleLength;

    g.setColour(juce::Colour::fromRGB(30, 25, 20).withAlpha(0.15f));
    g.drawLine(pivot.x + 1.0f, pivot.y + 1.0f, tip.x + 1.0f, tip.y + 1.0f, 2.5f);

    g.setColour(juce::Colour::fromRGB(20, 15, 10));
    g.drawLine(pivot.x, pivot.y, tip.x, tip.y, 1.8f);

    juce::ColourGradient needleGrad(
        juce::Colour::fromRGB(40, 30, 25), pivot.x, pivot.y,
        juce::Colour::fromRGB(180, 50, 40), tip.x, tip.y, false);
    g.setGradientFill(needleGrad);
    g.drawLine(pivot.x, pivot.y, tip.x, tip.y, 1.2f);

    g.setColour(juce::Colour::fromRGB(60, 50, 40));
    g.fillEllipse(pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f);

    juce::ColourGradient pivotGrad(
        juce::Colour::fromRGB(120, 110, 100), pivot.x - 2.0f, pivot.y - 2.0f,
        juce::Colour::fromRGB(50, 45, 40), pivot.x + 3.0f, pivot.y + 3.0f, true);
    g.setGradientFill(pivotGrad);
    g.fillEllipse(pivot.x - 4.0f, pivot.y - 4.0f, 8.0f, 8.0f);
}
