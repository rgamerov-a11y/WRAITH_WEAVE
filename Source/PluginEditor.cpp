#include "PluginEditor.h"

WeaveEditor::WeaveEditor(WeaveProcessor& p) : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(540, 390);
    mode.addItem("TEXTURE", 1); mode.addItem("RHYTHM", 2);
    addAndMakeVisible(mode);
    auto configure = [this](juce::Slider& s, const juce::String& suffix)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 22);
        s.setTextValueSuffix(suffix);
        addAndMakeVisible(s);
    };
    configure(variation, " weave"); configure(mix, " wet"); configure(density, " density");
    for (auto* b : { &record, &reset, &regenerate }) addAndMakeVisible(*b);
    record.onClick = [this] { processor.requestRecord(); };
    reset.onClick = [this] { processor.requestReset(); };
    regenerate.onClick = [this] { processor.requestRegenerate(); };
    state.setJustificationType(juce::Justification::centred);
    state.setColour(juce::Label::textColourId, juce::Colour(0xffc7d6b4));
    addAndMakeVisible(state);
    modeAttach = std::make_unique<ComboAttachment>(processor.parameters, "mode", mode);
    variationAttach = std::make_unique<SliderAttachment>(processor.parameters, "variation", variation);
    mixAttach = std::make_unique<SliderAttachment>(processor.parameters, "mix", mix);
    densityAttach = std::make_unique<SliderAttachment>(processor.parameters, "density", density);
    startTimerHz(12);
}

void WeaveEditor::timerCallback()
{
    state.setText(processor.isRecording() ? "RECORDING..." : (processor.hasSample() ? "SAMPLE READY" : "NO SAMPLE — PRESS RECORD"), juce::dontSendNotification);
}

void WeaveEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff202a22));
    g.setColour(juce::Colour(0xffe1e5cf));
    g.setFont(juce::FontOptions(27.0f, juce::Font::bold));
    g.drawText("WRAITH / WEAVE", 22, 12, getWidth() - 44, 42, juce::Justification::centred);
    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colour(0xffb2c2a2));
    g.drawText("GENERATIVE TWO-SECOND SAMPLER  •  0.1", 20, 55, getWidth() - 40, 18, juce::Justification::centred);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("VARIATION", 35, 130, 140, 20, juce::Justification::centred);
    g.drawText("MIX", 200, 130, 140, 20, juce::Justification::centred);
    g.drawText("DENSITY", 365, 130, 140, 20, juce::Justification::centred);
}

void WeaveEditor::resized()
{
    mode.setBounds(185, 85, 170, 30);
    variation.setBounds(35, 152, 140, 140);
    mix.setBounds(200, 152, 140, 140);
    density.setBounds(365, 152, 140, 140);
    record.setBounds(35, 305, 140, 34);
    regenerate.setBounds(200, 305, 140, 34);
    reset.setBounds(365, 305, 140, 34);
    state.setBounds(25, 345, getWidth() - 50, 28);
}
