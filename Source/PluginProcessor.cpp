#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

namespace
{
constexpr int kMaxDelayMs = 2000;
constexpr float kFeedbackMax = 0.95f;

juce::StringArray getSyncChoices()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T", "1/8D", "1/16D" };
}

float getDivisionMultiplier(int choiceIndex)
{
    switch (choiceIndex)
    {
        case 0: return 1.0f;     // 1/1
        case 1: return 0.5f;     // 1/2
        case 2: return 0.25f;    // 1/4
        case 3: return 0.125f;   // 1/8
        case 4: return 0.0625f;  // 1/16
        case 5: return 0.1666667f; // 1/8T
        case 6: return 0.0833333f; // 1/16T
        case 7: return 0.75f;    // 1/8D
        case 8: return 0.375f;   // 1/16D
        default: return 0.25f;
    }
}

float applyDrive(float sample, float driveAmount)
{
    const float driven = sample * driveAmount;
    return std::tanh(driven);
}
}

EchoByHdbAudioProcessor::EchoByHdbAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

const juce::String EchoByHdbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EchoByHdbAudioProcessor::acceptsMidi() const
{
    return false;
}

bool EchoByHdbAudioProcessor::producesMidi() const
{
    return false;
}

bool EchoByHdbAudioProcessor::isMidiEffect() const
{
    return false;
}

double EchoByHdbAudioProcessor::getTailLengthSeconds() const
{
    return 2.5;
}

int EchoByHdbAudioProcessor::getNumPrograms()
{
    return 1;
}

int EchoByHdbAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EchoByHdbAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String EchoByHdbAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EchoByHdbAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void EchoByHdbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    maxDelaySamples = static_cast<int>(std::ceil(sampleRate * (kMaxDelayMs / 1000.0))) + samplesPerBlock;
    delayBuffer.setSize(getTotalNumOutputChannels(), maxDelaySamples);
    delayBuffer.clear();
    writePosition = 0;

    const auto smoothTime = 0.05;
    timeSmoothed.reset(sampleRate, smoothTime);
    feedbackSmoothed.reset(sampleRate, smoothTime);
    mixSmoothed.reset(sampleRate, smoothTime);
    outputSmoothed.reset(sampleRate, smoothTime);

    timeSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue(ParameterIDs::timeMs)->load());
    feedbackSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue(ParameterIDs::feedback)->load() / 100.0f);
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue(ParameterIDs::mix)->load() / 100.0f);
    outputSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue(ParameterIDs::output)->load()));

    updateFilters();

    for (auto& filter : lowCutFilters)
        filter.reset();
    for (auto& filter : highCutFilters)
        filter.reset();
}

void EchoByHdbAudioProcessor::releaseResources()
{
    delayBuffer.setSize(0, 0);
}

bool EchoByHdbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& inputLayout = layouts.getMainInputChannelSet();
    const auto& outputLayout = layouts.getMainOutputChannelSet();

    if (outputLayout != juce::AudioChannelSet::stereo())
        return false;

    if (inputLayout != juce::AudioChannelSet::stereo() && inputLayout != juce::AudioChannelSet::mono())
        return false;

    return true;
}

float EchoByHdbAudioProcessor::getSyncTimeSeconds(double bpm) const
{
    const auto divisionIndex = static_cast<int>(apvts.getRawParameterValue(ParameterIDs::syncDivision)->load());
    const float multiplier = getDivisionMultiplier(divisionIndex);
    const double quarterNoteSeconds = 60.0 / bpm;
    return static_cast<float>(quarterNoteSeconds * 4.0 * multiplier);
}

void EchoByHdbAudioProcessor::updateFilters()
{
    const auto sampleRate = getSampleRate();
    const auto lowCutHz = apvts.getRawParameterValue(ParameterIDs::lowCut)->load();
    const auto highCutHz = apvts.getRawParameterValue(ParameterIDs::highCut)->load();

    const auto lowCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, lowCutHz);
    const auto highCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, highCutHz);

    for (auto& filter : lowCutFilters)
        filter.state = lowCutCoeffs;
    for (auto& filter : highCutFilters)
        filter.state = highCutCoeffs;
}

void EchoByHdbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const bool syncEnabled = apvts.getRawParameterValue(ParameterIDs::sync)->load() > 0.5f;
    const float timeMs = apvts.getRawParameterValue(ParameterIDs::timeMs)->load();
    const float feedbackPercent = apvts.getRawParameterValue(ParameterIDs::feedback)->load();
    const float mixPercent = apvts.getRawParameterValue(ParameterIDs::mix)->load();
    const float driveDb = apvts.getRawParameterValue(ParameterIDs::drive)->load();
    const float outputDb = apvts.getRawParameterValue(ParameterIDs::output)->load();
    const bool pingPongEnabled = apvts.getRawParameterValue(ParameterIDs::pingPong)->load() > 0.5f;

    const float feedbackValue = juce::jlimit(0.0f, kFeedbackMax, feedbackPercent / 100.0f);
    const float mixValue = juce::jlimit(0.0f, 1.0f, mixPercent / 100.0f);

    timeSmoothed.setTargetValue(timeMs);
    feedbackSmoothed.setTargetValue(feedbackValue);
    mixSmoothed.setTargetValue(mixValue);
    outputSmoothed.setTargetValue(juce::Decibels::decibelsToGain(outputDb));

    updateFilters();

    double bpm = 0.0;
    if (syncEnabled)
    {
        if (auto* playHead = getPlayHead())
        {
            if (playHead->getCurrentPosition(positionInfo) && positionInfo.bpm > 0.0)
                bpm = positionInfo.bpm;
        }
    }

    const float driveGain = juce::Decibels::decibelsToGain(driveDb);

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        const float targetDelayMs = syncEnabled && bpm > 0.0
            ? getSyncTimeSeconds(bpm) * 1000.0f
            : timeSmoothed.getNextValue();

        const float delaySamples = juce::jlimit(1.0f, static_cast<float>(maxDelaySamples - 1),
            (targetDelayMs / 1000.0f) * static_cast<float>(getSampleRate()));

        const int delaySamplesInt = static_cast<int>(delaySamples);
        const float frac = delaySamples - static_cast<float>(delaySamplesInt);

        const int readIndexA = (writePosition - delaySamplesInt + maxDelaySamples) % maxDelaySamples;
        const int readIndexB = (readIndexA - 1 + maxDelaySamples) % maxDelaySamples;

        float delayedSamples[2]{};

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float sampleA = delayBuffer.getSample(channel, readIndexA);
            const float sampleB = delayBuffer.getSample(channel, readIndexB);
            delayedSamples[channel] = juce::jmap(frac, sampleA, sampleB);
        }

        const float feedbackAmount = feedbackSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float outputGain = outputSmoothed.getNextValue();

        float feedbackSamples[2]{};
        if (pingPongEnabled)
        {
            feedbackSamples[0] = delayedSamples[1];
            feedbackSamples[1] = delayedSamples[0];
        }
        else
        {
            feedbackSamples[0] = delayedSamples[0];
            feedbackSamples[1] = delayedSamples[1];
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float inputSample = buffer.getSample(channel, sampleIndex);
            float filtered = lowCutFilters[channel].processSample(feedbackSamples[channel]);
            filtered = highCutFilters[channel].processSample(filtered);
            filtered = applyDrive(filtered, driveGain);

            const float feedbackSample = filtered * feedbackAmount;
            const float writeSample = inputSample + feedbackSample;
            delayBuffer.setSample(channel, writePosition, writeSample);

            const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
            const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

            const float outputSample = (inputSample * dryGain) + (delayedSamples[channel] * wetGain);
            buffer.setSample(channel, sampleIndex, outputSample * outputGain);
        }

        if (++writePosition >= maxDelaySamples)
            writePosition = 0;
    }
}

bool EchoByHdbAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EchoByHdbAudioProcessor::createEditor()
{
    return new EchoByHdbAudioProcessorEditor(*this);
}

void EchoByHdbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void EchoByHdbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout EchoByHdbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::timeMs,
        "Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 0.01f, 0.5f),
        400.0f,
        "ms"));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::sync,
        "Sync",
        false));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::syncDivision,
        "Sync Division",
        getSyncChoices(),
        2));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::feedback,
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 95.0f, 0.01f),
        35.0f,
        "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::mix,
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        35.0f,
        "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::lowCut,
        "LowCut",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.5f),
        120.0f,
        "Hz"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::highCut,
        "HighCut",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.5f),
        8000.0f,
        "Hz"));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::pingPong,
        "PingPong",
        false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::drive,
        "Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f),
        6.0f,
        "dB"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::output,
        "Output",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.01f),
        0.0f,
        "dB"));

    return layout;
}
