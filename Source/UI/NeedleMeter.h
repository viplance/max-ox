#pragma once

#include <JuceHeader.h>

class NeedleMeter : public juce::Component
{
public:
    enum class MeterType { Level, GainReduction };

    explicit NeedleMeter(MeterType type = MeterType::Level);

    void setLevelDb(float db);
    void paint(juce::Graphics& g) override;

private:
    void drawFaceplate(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds, float normalised);
    void drawScaleMarks(juce::Graphics& g, juce::Point<float> pivot, float radius);
    void drawDbLabel(juce::Graphics& g, juce::Point<float> pivot);

    MeterType meterType;
    float levelDb = -48.0f;
    float smoothedDb = -48.0f;

    static constexpr float kMinDb = -48.0f;
    static constexpr float kMaxDb = 0.0f;
    static constexpr float kVisualSmoothing = 0.4375f;
    static constexpr float kGrMaxReductionDb = 12.0f;
    static constexpr float kNeedleAngleStart = -0.85f;
    static constexpr float kNeedleAngleEnd = 0.85f;
};
