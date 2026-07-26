#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr float kMinDb = -48.0f;
constexpr float kMaxDb = 24.0f;

float peakToDb(float peak) {
    return juce::jlimit(kMinDb, kMaxDb, juce::Decibels::gainToDecibels(peak, kMinDb));
}
}

MaxOxAudioProcessor::MaxOxAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    gainParam = apvts.getRawParameterValue("GAIN");
}

MaxOxAudioProcessor::~MaxOxAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MaxOxAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("GAIN", 1), "Gain",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

void MaxOxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    limiter.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(limiter.getLatencySamples());
    adaptiveLowCut.prepare(sampleRate, samplesPerBlock);
    phaseRotator.prepare(sampleRate, samplesPerBlock);
}

void MaxOxAudioProcessor::releaseResources()
{
    limiter.reset();
    adaptiveLowCut.reset();
    phaseRotator.reset();
}

bool MaxOxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();
    return input == output
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

void MaxOxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float inputPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(ch, 0, numSamples));

    const float gainDb = gainParam->load();
    const float gain = juce::Decibels::decibelsToGain(gainDb);
    if (std::abs(gain - 1.0f) > 1e-6f)
        buffer.applyGain(gain);

    auto* L = buffer.getWritePointer(0);
    auto* R = numChannels > 1 ? buffer.getWritePointer(1) : L;
    adaptiveLowCut.process(L, R, numSamples, juce::jmin(numChannels, 2));

    phaseRotator.process(buffer);
    limiter.process(buffer);

    float outputPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(ch, 0, numSamples));

    const float releaseCoeff = 1.0f - std::exp((float)-numSamples / (float)(currentSampleRate * 0.42));
    updateMeter(inputLevelDb, inputPeak, releaseCoeff);
    updateMeter(outputLevelDb, outputPeak, releaseCoeff);
}

void MaxOxAudioProcessor::updateMeter(std::atomic<float>& target, float blockPeak, float releaseCoeff)
{
    const float nextDb = peakToDb(blockPeak);
    const float current = target.load(std::memory_order_relaxed);
    const float smoothed = nextDb > current ? nextDb : current + (nextDb - current) * releaseCoeff;
    target.store(juce::jlimit(kMinDb, kMaxDb, smoothed), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MaxOxAudioProcessor::createEditor()
{
    return new MaxOxAudioProcessorEditor(*this);
}

void MaxOxAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid()) {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void MaxOxAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MaxOxAudioProcessor();
}
