#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

class WeaveProcessor final : public juce::AudioProcessor
{
public:
    WeaveProcessor();
    ~WeaveProcessor() override = default;
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void requestRecord() noexcept { recordRequest.store(true, std::memory_order_release); }
    void requestReset() noexcept { resetRequest.store(true, std::memory_order_release); }
    void requestRegenerate() noexcept { regenerateRequest.store(true, std::memory_order_release); }
    bool hasSample() const noexcept { return sampleReady.load(std::memory_order_relaxed); }
    bool isRecording() const noexcept { return recordingDisplay.load(std::memory_order_relaxed); }
    bool importWav(const juce::File& file);
    bool exportWav(const juce::File& file);
    void savePreset(const juce::File& file);
    bool loadPreset(const juce::File& file);
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    struct Grain { double cursor = 0; double step = 1; int age = 0, length = 0; float gain = 0, pan = 0; bool active = false; };
    std::array<Grain, 24> grains {};
    juce::CriticalSection sampleMutex;
    std::array<bool, 128> heldNotes {};
    int currentNote = -1;
    int voiceCursor = 0;
    float midiGain = 1.0f;
    float midiSemitones = 0.0f;
    float pitchWheelSemitones = 0.0f;
    float lastPitchOffset = 0.0f;
    float nextGate = 1.0f;
    int nextRhythmRatchet = 1;
    int64_t sequencerSamples = 0;
    double loadedSampleRate = 44100.0;
    juce::AudioBuffer<float> capture;
    juce::AudioBuffer<float> restoredAudio;
    double restoredRate = 44100.0;
    void applyRestoredSample() noexcept;
    int sampleLength = 0, captured = 0;
    double rateHz = 44100.0, textureClock = 0, rhythmClock = 0;
    int rhythmStep = 0;
    std::array<float, 16> rhythmPattern {};
    std::array<int, 16> rhythmRatchets {};
    // Each NEW WEAVE draws a full sonic scene, not just a semitone offset.
    struct WeaveScene
    {
        float pitchSemitones = 0.0f;
        float pitchSpread = 0.0f;
        float region = 0.5f;
        float regionSpread = 1.0f;
        float durationScale = 1.0f;
        float densityScale = 1.0f;
        float reverseChance = 0.0f;
        float playbackRatio = 1.0f;
        float panBias = 0.0f;
        float gateScale = 1.0f;
        float volume = 1.0f;
        int rhythmStride = 1;
    } scene;
    uint32_t weaveNumber = 0;
    std::atomic<uint32_t> weaveNumberDisplay { 0 };
public:
    uint32_t getWeaveNumber() const noexcept { return weaveNumberDisplay.load(std::memory_order_relaxed); }
private:
    float smoothLevel = 0.0f, targetLevel = 0.0f;
    float dcInputL = 0.0f, dcInputR = 0.0f, dcOutputL = 0.0f, dcOutputR = 0.0f;
    float smoothWet = 0;
    uint32_t seed = 0xA17E529Bu;
    std::atomic<bool> recordRequest { false }, resetRequest { false }, regenerateRequest { false };
    std::atomic<bool> sampleReady { false }, recordingDisplay { false };
    uint32_t nextRand() noexcept { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
    float random01() noexcept { return static_cast<float>(nextRand() & 0xffffffu) / 16777216.0f; }
    void spawnGrain(bool rhythm, float variation, float accent = 1.0f) noexcept;
    void buildRhythmPattern() noexcept;
    void regenerateSound() noexcept;
    float readSample(int channel, double position) const noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeaveProcessor)
};
