#include "PluginEditor.h"
#include "ParameterIDs.h"

namespace
{
void configureKnob(juce::Slider& slider, const juce::String& name, const juce::String& tooltip)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setName(name);
    slider.setTooltip(tooltip);
}

void configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
}
}

EchoByHdbAudioProcessorEditor::EchoByHdbAudioProcessorEditor(EchoByHdbAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(720, 420);

    titleLabel.setText("Echo by HDB", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    configureKnob(timeSlider, "Time", "Delay time in milliseconds or sync division");
    configureKnob(feedbackSlider, "Feedback", "Feedback amount");
    configureKnob(mixSlider, "Mix", "Dry / Wet mix");
    configureKnob(lowCutSlider, "LowCut", "High-pass filter in feedback loop");
    configureKnob(highCutSlider, "HighCut", "Low-pass filter in feedback loop");
    configureKnob(driveSlider, "Drive", "Soft saturation in feedback loop");
    configureKnob(outputSlider, "Output", "Output gain");

    syncButton.setButtonText("Sync");
    syncButton.setTooltip("Sync delay time to host tempo");

    pingPongButton.setButtonText("PingPong");
    pingPongButton.setTooltip("Ping-pong stereo feedback");

    syncDivisionBox.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T", "1/8D", "1/16D" }, 1);
    syncDivisionBox.setTooltip("Tempo sync division");

    configureLabel(timeValueLabel, "Time: 400 ms");
    configureLabel(feedbackValueLabel, "Feedback: 35 %");
    configureLabel(mixValueLabel, "Mix: 35 %");

    addAndMakeVisible(timeSlider);
    addAndMakeVisible(feedbackSlider);
    addAndMakeVisible(mixSlider);
    addAndMakeVisible(lowCutSlider);
    addAndMakeVisible(highCutSlider);
    addAndMakeVisible(driveSlider);
    addAndMakeVisible(outputSlider);
    addAndMakeVisible(syncButton);
    addAndMakeVisible(pingPongButton);
    addAndMakeVisible(syncDivisionBox);
    addAndMakeVisible(timeValueLabel);
    addAndMakeVisible(feedbackValueLabel);
    addAndMakeVisible(mixValueLabel);

    auto& apvts = processor.getAPVTS();
    timeAttachment = std::make_unique<SliderAttachment>(apvts, ParameterIDs::timeMs, timeSlider);
    feedbackAttachment = std::make_unique<SliderAttachment>(apvts, ParameterIDs::feedback, feedbackSlider);
    mixAttachment = std::make_unique<SliderAttachment>(apvts, ParameterIDs::mix, mixSlider);
    lowCutAttachment = std::make_unique<SliderAttachment>(apvts, ParameterIDs::lowCut, lowCutSlider);
    highCutAttachment = std::make_unique<SliderAttachment>(apvts, ParameterIDs::highCut, highCutSlider);
    driveAttachment = std::make_unique<SliderAttachment>(apvts, ParameterIDs::drive, driveSlider);
    outputAttachment = std::make_unique<SliderAttachment>(apvts, ParameterIDs::output, outputSlider);
    syncAttachment = std::make_unique<ButtonAttachment>(apvts, ParameterIDs::sync, syncButton);
    pingPongAttachment = std::make_unique<ButtonAttachment>(apvts, ParameterIDs::pingPong, pingPongButton);
    syncDivisionAttachment = std::make_unique<ComboBoxAttachment>(apvts, ParameterIDs::syncDivision, syncDivisionBox);

    startTimerHz(12);
}

void EchoByHdbAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1d1f23));

    g.setColour(juce::Colour(0xff2c2f36));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 12.0f);
}

void EchoByHdbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);
    titleLabel.setBounds(area.removeFromTop(40));

    auto valueArea = area.removeFromTop(30);
    timeValueLabel.setBounds(valueArea.removeFromLeft(area.getWidth() / 3));
    feedbackValueLabel.setBounds(valueArea.removeFromLeft(area.getWidth() / 2));
    mixValueLabel.setBounds(valueArea);

    auto topRow = area.removeFromTop(160);
    auto bottomRow = area.removeFromTop(160);

    auto knobWidth = topRow.getWidth() / 4;
    timeSlider.setBounds(topRow.removeFromLeft(knobWidth).reduced(8));
    feedbackSlider.setBounds(topRow.removeFromLeft(knobWidth).reduced(8));
    mixSlider.setBounds(topRow.removeFromLeft(knobWidth).reduced(8));
    outputSlider.setBounds(topRow.removeFromLeft(knobWidth).reduced(8));

    knobWidth = bottomRow.getWidth() / 3;
    lowCutSlider.setBounds(bottomRow.removeFromLeft(knobWidth).reduced(8));
    highCutSlider.setBounds(bottomRow.removeFromLeft(knobWidth).reduced(8));
    driveSlider.setBounds(bottomRow.removeFromLeft(knobWidth).reduced(8));

    auto buttonArea = area;
    syncButton.setBounds(buttonArea.removeFromLeft(120));
    syncDivisionBox.setBounds(buttonArea.removeFromLeft(120));
    pingPongButton.setBounds(buttonArea.removeFromLeft(120));
}

void EchoByHdbAudioProcessorEditor::timerCallback()
{
    const auto& apvts = processor.getAPVTS();
    const bool syncEnabled = apvts.getRawParameterValue(ParameterIDs::sync)->load() > 0.5f;
    const float timeValue = apvts.getRawParameterValue(ParameterIDs::timeMs)->load();
    const int divisionIndex = static_cast<int>(apvts.getRawParameterValue(ParameterIDs::syncDivision)->load());

    if (syncEnabled)
    {
        const auto labels = juce::StringArray({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T", "1/8D", "1/16D" });
        const auto divisionLabel = labels[juce::jlimit(0, labels.size() - 1, divisionIndex)];
        timeValueLabel.setText("Time: " + divisionLabel, juce::dontSendNotification);
    }
    else
    {
        timeValueLabel.setText("Time: " + juce::String(timeValue, 1) + " ms", juce::dontSendNotification);
    }

    const float feedbackValue = apvts.getRawParameterValue(ParameterIDs::feedback)->load();
    const float mixValue = apvts.getRawParameterValue(ParameterIDs::mix)->load();

    feedbackValueLabel.setText("Feedback: " + juce::String(feedbackValue, 1) + " %", juce::dontSendNotification);
    mixValueLabel.setText("Mix: " + juce::String(mixValue, 1) + " %", juce::dontSendNotification);
}
