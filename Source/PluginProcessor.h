#pragma once

#include <JuceHeader.h>
#include "DSP/LookAheadLimiter.h"
#include "DSP/AdaptiveLowCut.h"
#include <atomic>

class MaxOxAudioProcessor : public juce::AudioProcessor
{
public:
    MaxOxAudioProcessor();
    ~MaxOxAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getInputLevelDb() const { return inputLevelDb.load(std::memory_order_relaxed); }
    float getOutputLevelDb() const { return outputLevelDb.load(std::memory_order_relaxed); }
    float getGainReductionDb() const { return limiter.getGainReductionDb(); }
    float getLowCutActivity() const { return adaptiveLowCut.getLowCutActivity(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateMeter(std::atomic<float>& target, float blockPeak, float releaseCoeff);

    std::atomic<float>* gainParam = nullptr;

    LookAheadLimiter limiter;
    AdaptiveLowCut adaptiveLowCut;

    double currentSampleRate = 48000.0;

    std::atomic<float> inputLevelDb { -48.0f };
    std::atomic<float> outputLevelDb { -48.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaxOxAudioProcessor)
};
