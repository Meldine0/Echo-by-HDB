#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EchoByHdbAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit EchoByHdbAudioProcessorEditor(EchoByHdbAudioProcessor&);
    ~EchoByHdbAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    EchoByHdbAudioProcessor& processor;

    juce::Label titleLabel;

    juce::Slider timeSlider;
    juce::Slider feedbackSlider;
    juce::Slider mixSlider;
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;
    juce::Slider driveSlider;
    juce::Slider outputSlider;

    juce::ToggleButton syncButton;
    juce::ToggleButton pingPongButton;
    juce::ComboBox syncDivisionBox;

    juce::Label timeValueLabel;
    juce::Label feedbackValueLabel;
    juce::Label mixValueLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> timeAttachment;
    std::unique_ptr<SliderAttachment> feedbackAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> lowCutAttachment;
    std::unique_ptr<SliderAttachment> highCutAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ButtonAttachment> syncAttachment;
    std::unique_ptr<ButtonAttachment> pingPongAttachment;
    std::unique_ptr<ComboBoxAttachment> syncDivisionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EchoByHdbAudioProcessorEditor)
};
