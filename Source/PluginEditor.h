#pragma once
#include "PluginProcessor.h"

class WeaveEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WeaveEditor(WeaveProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    WeaveProcessor& processor;
    juce::ComboBox mode;
    juce::Slider variation, mix, density;
    juce::Label state;
    juce::TextButton record { "RECORD 2 SEC" }, reset { "RESET" }, regenerate { "NEW WEAVE" };
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAttachment> modeAttach;
    std::unique_ptr<SliderAttachment> variationAttach, mixAttach, densityAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeaveEditor)
};
