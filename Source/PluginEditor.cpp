#include "PluginEditor.h"

WeaveEditor::WeaveEditor(WeaveProcessor& p) : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(680, 520);
    mode.addItem("TEXTURE", 1); mode.addItem("RHYTHM", 2);
    addAndMakeVisible(mode);
    const auto makeSlider = [this](juce::Slider& s, const char* id)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 20);
        addAndMakeVisible(s);
        sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, id, s));
    };
    makeSlider(variation, "variation"); makeSlider(mix, "mix"); makeSlider(density, "density");
    makeSlider(pitch, "pitch"); makeSlider(width, "width"); makeSlider(output, "output"); makeSlider(chance, "chance");
    addAndMakeVisible(midiGate); addAndMakeVisible(sync);
    midiGateAttach = std::make_unique<ButtonAttachment>(processor.parameters, "midiGate", midiGate);
    syncAttach = std::make_unique<ButtonAttachment>(processor.parameters, "sync", sync);
    for (auto* b : { &record, &reset, &regenerate, &import, &exportSample, &savePresetButton, &loadPresetButton }) addAndMakeVisible(*b);
    record.onClick = [this] { processor.requestRecord(); };
    reset.onClick = [this] { processor.requestReset(); };
    regenerate.onClick = [this] { processor.requestRegenerate(); };
    import.onClick = [this] { chooseFile(false, "wav"); };
    exportSample.onClick = [this] { chooseFile(true, "wav"); };
    savePresetButton.onClick = [this] { chooseFile(true, "preset"); };
    loadPresetButton.onClick = [this] { chooseFile(false, "preset"); };
    state.setJustificationType(juce::Justification::centred);
    state.setColour(juce::Label::textColourId, juce::Colour(0xffc7d6b4));
    addAndMakeVisible(state);
    modeAttach = std::make_unique<ComboAttachment>(processor.parameters, "mode", mode);
    startTimerHz(10);
}

void WeaveEditor::chooseFile(bool save, const juce::String& kind)
{
    const bool wav = kind == "wav";
    const juce::String pattern = wav ? "*.wav" : "*.weave";
    fileChooser = std::make_unique<juce::FileChooser>(save ? "Save" : "Open", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), pattern);
    juce::Component::SafePointer<WeaveEditor> safe(this);
    const int flags = save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                           : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [safe, save, wav](const juce::FileChooser& chooser)
    {
        if (safe == nullptr) return;
        auto file = chooser.getResult();
        if (file == juce::File()) return;
        file = file.withFileExtension(wav ? ".wav" : ".weave");
        if (wav)
        {
            const bool ok = save ? safe->processor.exportWav(file) : safe->processor.importWav(file);
            if (!ok) safe->state.setText("WAV operation failed", juce::dontSendNotification);
        }
        else if (save) safe->processor.savePreset(file);
        else if (!safe->processor.loadPreset(file)) safe->state.setText("Preset load failed", juce::dontSendNotification);
    });
}
void WeaveEditor::timerCallback()
{
    state.setText(processor.isRecording() ? "RECORDING 2 SECONDS..." :
      (processor.hasSample() ? "SAMPLE READY - NEW WEAVE #" + juce::String(processor.getWeaveNumber()) : "NO SAMPLE - RECORD OR IMPORT WAV"), juce::dontSendNotification);
}
void WeaveEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff202a22));
    g.setColour(juce::Colour(0xffe1e5cf));
    g.setFont(juce::FontOptions(27.0f, juce::Font::bold));
    g.drawText("WRAITH / WEAVE", 22, 10, getWidth() - 44, 38, juce::Justification::centred);
    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colour(0xffb2c2a2));
    g.drawText("GENERATIVE SAMPLER  |  TEXTURE + RHYTHM  |  0.4", 20, 50, getWidth() - 40, 18, juce::Justification::centred);
    const char* names[] = { "VARIATION", "MIX", "DENSITY", "PITCH", "WIDTH", "OUTPUT", "CHANCE" };
    g.setFont(juce::FontOptions(12.0f));
    for (int i = 0; i < 7; ++i)
    {
        const int x = 18 + (i % 4) * 165;
        const int y = 124 + (i / 4) * 130;
        g.drawText(names[i], x, y, 150, 18, juce::Justification::centred);
    }
}
void WeaveEditor::resized()
{
    mode.setBounds(245, 82, 190, 30);
    juce::Slider* sliders[] = { &variation, &mix, &density, &pitch, &width, &output, &chance };
    for (int i = 0; i < 7; ++i)
        sliders[i]->setBounds(18 + (i % 4) * 165, 144 + (i / 4) * 130, 150, 102);
    midiGate.setBounds(520, 280, 140, 24);
    sync.setBounds(520, 308, 140, 24);
    record.setBounds(22, 378, 195, 35);
    regenerate.setBounds(242, 378, 195, 35);
    reset.setBounds(462, 378, 195, 35);
    import.setBounds(22, 426, 145, 30);
    exportSample.setBounds(185, 426, 145, 30);
    savePresetButton.setBounds(350, 426, 145, 30);
    loadPresetButton.setBounds(512, 426, 145, 30);
    state.setBounds(20, 473, getWidth() - 40, 28);
}
