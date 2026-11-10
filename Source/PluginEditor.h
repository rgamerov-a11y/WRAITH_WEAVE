#pragma once
#include "PluginProcessor.h"
class WeaveEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WeaveEditor(WeaveProcessor&);
    ~WeaveEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void chooseFile(bool save, const juce::String& kind);
    WeaveProcessor& processor;
    juce::ComboBox mode;
    juce::Slider variation, mix, density, pitch, width, output, chance;
    juce::ToggleButton midiGate { "MIDI GATE" }, sync { "HOST SYNC" };
    juce::Label state;
    juce::TextButton record { "RECORD" }, reset { "RESET" }, regenerate { "NEW WEAVE" };
    juce::TextButton import { "IMPORT WAV" }, exportSample { "EXPORT WAV" };
    juce::TextButton savePresetButton { "SAVE PRESET" }, loadPresetButton { "LOAD PRESET" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ComboAttachment> modeAttach;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ButtonAttachment> midiGateAttach, syncAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeaveEditor)
};
