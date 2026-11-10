#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

WeaveProcessor::WeaveProcessor()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                            .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "WEAVE_STATE", makeLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout WeaveProcessor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Mode", juce::StringArray{"Texture", "Rhythm"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"variation", 1}, "Variation", juce::NormalisableRange<float>(0, 1), 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix", 1}, "Wet Mix", juce::NormalisableRange<float>(0, 1), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"density", 1}, "Density", juce::NormalisableRange<float>(0.1f, 1.0f), 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"pitch", 1}, "Pitch", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"width", 1}, "Stereo Width", juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output", 1}, "Output", juce::NormalisableRange<float>(-18.0f, 6.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"chance", 1}, "Rhythm Chance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"midiGate", 1}, "MIDI Gate", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync", 1}, "Host Sync", true));
    return { p.begin(), p.end() };
}

bool WeaveProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (b.getMainInputChannelSet() == juce::AudioChannelSet::mono() || b.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void WeaveProcessor::prepareToPlay(double sr, int)
{
    rateHz = sr > 0 ? sr : 44100.0;
    sampleLength = juce::jmax(1, juce::roundToInt(rateHz * 2.0));
    const juce::ScopedLock sampleLock(sampleMutex);
    if (sampleReady.load() && capture.getNumSamples() > 0)
    {
        restoredAudio.makeCopyOf(capture);
        restoredRate = loadedSampleRate;
    }
    capture.setSize(2, sampleLength, false, true, true);
    capture.clear();
    loadedSampleRate = rateHz;
    heldNotes.fill(false); currentNote = -1; midiGain = 1.0f; midiSemitones = pitchWheelSemitones = 0.0f;
    captured = 0;
    textureClock = rhythmClock = 0;
    grains = {};
    sampleReady.store(false);
    recordingDisplay.store(false);
    smoothWet = 0;
    smoothLevel = 0; targetLevel = 0;
    rhythmStep = 0;
    dcInputL = dcInputR = dcOutputL = dcOutputR = 0;
    regenerateSound();
    applyRestoredSample();
}

float WeaveProcessor::readSample(int channel, double pos) const noexcept
{
    if (sampleLength < 2) return 0.0f;
    pos = std::fmod(pos, static_cast<double>(sampleLength));
    if (pos < 0) pos += sampleLength;
    const int i = static_cast<int>(pos);
    const int j = (i + 1) % sampleLength;
    const float frac = static_cast<float>(pos - i);
    const float* ptr = capture.getReadPointer(channel);
    return ptr[i] + frac * (ptr[j] - ptr[i]);
}

void WeaveProcessor::buildRhythmPattern() noexcept
{
    // Three rhythm families create audible structure differences, not only note probabilities.
    const int family = static_cast<int>(nextRand() % 3u);
    for (int i = 0; i < 16; ++i)
    {
        const bool downbeat = (i % 4) == 0;
        const bool offbeat = (i % 4) == 2;
        float probability = 0.0f;
        if (family == 0) probability = downbeat ? 0.08f : (offbeat ? 0.38f : 0.87f); // sparse
        if (family == 1) probability = downbeat ? 0.05f : (offbeat ? 0.18f : 0.54f); // groove
        if (family == 2) probability = downbeat ? 0.05f : (offbeat ? 0.13f : 0.26f); // busy
        rhythmPattern[static_cast<size_t>(i)] = juce::jlimit(0.01f, 0.99f,
            probability + (random01() - 0.5f) * 0.22f);
        rhythmRatchets[static_cast<size_t>(i)] = random01() < (family == 2 ? 0.48f : 0.14f) ? 2 : 1;
    }
}

void WeaveProcessor::regenerateSound() noexcept
{
    seed ^= 0x9E3779B9u + nextRand();
    if (seed == 0) seed = 0xA17E529Bu;
    ++weaveNumber;
    weaveNumberDisplay.store(weaveNumber, std::memory_order_relaxed);

    // Six deliberately contrasting families. Repeated clicks deliberately step
    // through different families so successive results cannot all be subtle.
    const int family = static_cast<int>((weaveNumber - 1u) % 6u);
    static constexpr float transpositions[] { -19.0f, -12.0f, 0.0f, 7.0f, 12.0f, 19.0f };
    static constexpr float durations[]      { 0.48f, 1.6f, 0.23f, 1.1f, 2.0f, 0.65f };
    static constexpr float densities[]      { 1.8f, 0.55f, 2.3f, 0.8f, 1.25f, 1.6f };
    static constexpr float reverses[]       { 0.95f, 0.02f, 0.50f, 0.82f, 0.12f, 0.66f };
    static constexpr float regions[]        { 0.10f, 0.78f, 0.45f, 0.92f, 0.22f, 0.61f };
    static constexpr float spreads[]        { 0.12f, 0.35f, 0.9f, 0.09f, 0.55f, 0.22f };
    scene.pitchSemitones = transpositions[family] + (random01() - 0.5f) * 3.0f;
    scene.pitchSpread = 2.0f + random01() * 6.0f;
    scene.region = juce::jlimit(0.0f, 1.0f, regions[family] + (random01() - 0.5f) * 0.1f);
    scene.regionSpread = spreads[family];
    scene.durationScale = durations[family];
    scene.densityScale = densities[family];
    scene.reverseChance = reverses[family];
    scene.playbackRatio = (family == 2 || family == 4) ? 0.50f : 1.0f;
    scene.panBias = (random01() - 0.5f) * 0.9f;
    scene.gateScale = (family == 2 ? 0.55f : (family == 4 ? 1.7f : 1.0f));
    scene.volume = (family == 2 ? 0.85f : 1.0f);
    scene.rhythmStride = (family == 1 || family == 4) ? 2 : 1;
    buildRhythmPattern();
    rhythmStep = 0;
    textureClock = 0;
    rhythmClock = 0;
    // Existing grains finish naturally, preventing clicks during regeneration.
}

void WeaveProcessor::spawnGrain(bool rhythm, float variation, float accent) noexcept
{
    for (auto& g : grains)
    {
        if (g.active) continue;
        const float r = random01();
        const float lenSeconds = rhythm ? (0.035f + r * 0.19f)
                                        : (0.24f + r * 0.58f);
        g.length = juce::jlimit(64, sampleLength,
            static_cast<int>(lenSeconds * scene.durationScale * rateHz));
        g.age = 0;
        const float regionPosition = scene.region +
            (random01() * 2.0f - 1.0f) * scene.regionSpread * 0.5f;
        g.cursor = juce::jlimit(0.0, static_cast<double>(sampleLength - 1),
            static_cast<double>(regionPosition) * (sampleLength - 1));
        static constexpr float intervals[] { -12, -7, -5, 0, 0, 0, 5, 7, 12 };
        const float semitones = scene.pitchSemitones
            + intervals[nextRand() % 9] * variation
            + (random01() * 2.0f - 1.0f) * scene.pitchSpread * variation;
        float voicePitch = lastPitchOffset;
        int heldCount = 0;
        for (bool active : heldNotes) if (active) ++heldCount;
        if (heldCount > 0)
        {
            const int chosen = voiceCursor++ % heldCount;
            int index = 0;
            for (int note = 0; note < 128; ++note)
                if (heldNotes[static_cast<size_t>(note)] && index++ == chosen)
                {
                    voicePitch = *parameters.getRawParameterValue("pitch")
                        + pitchWheelSemitones + static_cast<float>(note - 60);
                    break;
                }
        }
        g.step = std::pow(2.0, (semitones + voicePitch) / 12.0) * scene.playbackRatio;
        if (random01() < scene.reverseChance) g.step = -g.step;
        const float width = *parameters.getRawParameterValue("width") * (rhythm ? 1.0f : 0.85f);
        g.pan = juce::jlimit(-1.0f, 1.0f,
            scene.panBias + (random01() * 2.0f - 1.0f) * width);
        g.gain = (rhythm ? 0.70f : 0.42f) * accent * scene.volume;
        g.active = true;
        break;
    }
}

void WeaveProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const juce::ScopedLock sampleLock(sampleMutex);
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) { heldNotes[static_cast<size_t>(msg.getNoteNumber())] = true; midiGain = msg.getFloatVelocity(); }
        if (msg.isNoteOff()) heldNotes[static_cast<size_t>(msg.getNoteNumber())] = false;
        if (msg.isAllNotesOff() || msg.isAllSoundOff()) heldNotes.fill(false);
        if (msg.isPitchWheel()) pitchWheelSemitones = ((msg.getPitchWheelValue() - 8192.0f) / 8192.0f) * 2.0f;
    }
    currentNote = -1;
    for (int note = 0; note < 128; ++note) if (heldNotes[static_cast<size_t>(note)]) currentNote = note;
    midiSemitones = currentNote >= 0 ? static_cast<float>(currentNote - 60) : 0.0f;
    const int inputs = getTotalNumInputChannels();
    const auto* inL = buffer.getReadPointer(0);
    const auto* inR = inputs > 1 ? buffer.getReadPointer(1) : inL;
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);

    if (resetRequest.exchange(false))
    {
        sampleReady.store(false); recordingDisplay.store(false);
        restoredAudio.setSize(0, 0);
        captured = 0; targetLevel = 0.0f;
        grains = {}; // Buffer is no longer available; restart wet output from silence.
    }
    if (recordRequest.exchange(false))
    {
        captured = 0; grains = {};
        restoredAudio.setSize(0, 0);
        targetLevel = 0.0f;
        sampleReady.store(false); recordingDisplay.store(true);
    }
    if (regenerateRequest.exchange(false))
    {
        regenerateSound();
    }
    const bool rhythm = *parameters.getRawParameterValue("mode") >= 0.5f;
    const float variation = *parameters.getRawParameterValue("variation");
    const float mix = *parameters.getRawParameterValue("mix");
    const float density = *parameters.getRawParameterValue("density");
    const float gate = *parameters.getRawParameterValue("midiGate") >= 0.5f && currentNote < 0 ? 0.0f : 1.0f;
    const float chance = *parameters.getRawParameterValue("chance");
    lastPitchOffset = *parameters.getRawParameterValue("pitch") + midiSemitones + pitchWheelSemitones;
    const float outputGain = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("output")->load(std::memory_order_relaxed));
    const double textureInterval = rateHz * (0.115 - 0.09 * density) / scene.densityScale;
    // Rhythm uses the host tempo, falling back to 120 BPM when unavailable.
    double bpm = 120.0;
    if (auto* playhead = getPlayHead())
        if (auto position = playhead->getPosition())
            if (auto hostBpm = position->getBpm()) bpm = juce::jlimit(30.0, 300.0, *hostBpm);
    const double stepSamples = rateHz * 60.0 / bpm / 4.0;
    if (*parameters.getRawParameterValue("sync") >= 0.5f)
        if (auto* ph = getPlayHead())
            if (auto position = ph->getPosition())
                if (auto ppq = position->getPpqPosition())
                {
                    const int hostStep = static_cast<int>(std::floor(*ppq * 4.0)) & 15;
                    if (rhythmStep != hostStep)
                    {
                        rhythmStep = hostStep;
                        rhythmClock = 0;
                    }
                }
    // This mode intentionally runs the pattern from recording completion; a
    // future update will phase-lock each step to Ableton transport position.

    for (int i = 0; i < n; ++i)
    {
        const float dryL = inL[i];
        const float dryR = inR[i];
        if (recordingDisplay.load(std::memory_order_relaxed) && captured < sampleLength)
        {
            capture.setSample(0, captured, dryL);
            capture.setSample(1, captured, dryR);
            if (++captured >= sampleLength)
            {
                recordingDisplay.store(false);
                sampleReady.store(true);
                textureClock = rhythmClock = 0;
                rhythmStep = 0;
                targetLevel = 1.0f;
            }
        }
        float wetL = 0, wetR = 0;
        if (sampleReady.load(std::memory_order_relaxed))
        {
            if (rhythm)
            {
                if (rhythmClock <= 0)
                {
                    const int ix = rhythmStep & 15;
                    if (rhythmPattern[static_cast<size_t>(ix)] < juce::jmin(1.0f, chance * (0.12f + 0.82f * density) * scene.densityScale))
                    {
                        const float accent = ix % 4 == 0 ? 1.0f : 0.7f;
                        spawnGrain(true, variation, accent);
                        if (rhythmRatchets[static_cast<size_t>(ix)] == 2 && density > 0.65f)
                            spawnGrain(true, variation, accent * 0.35f);
                    }
                    rhythmStep += scene.rhythmStride;
                    rhythmClock += stepSamples * scene.gateScale;
                }
                rhythmClock -= 1.0;
            }
            else
            {
                if (textureClock <= 0)
                {
                    spawnGrain(false, variation);
                    textureClock += textureInterval;
                }
                textureClock -= 1.0;
            }
            float sumWindow = 0.0f;
            for (auto& g : grains)
            {
                if (!g.active) continue;
                const float phase = static_cast<float>(g.age) / static_cast<float>(g.length);
                const float window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
                const float left = std::sqrt((1.0f - g.pan) * 0.5f);
                const float right = std::sqrt((1.0f + g.pan) * 0.5f);
                wetL += readSample(0, g.cursor) * window * g.gain * left;
                wetR += readSample(1, g.cursor) * window * g.gain * right;
                sumWindow += window * g.gain;
                g.cursor += g.step;
                if (++g.age >= g.length) g.active = false;
            }
            // Normalize extreme overlap so high-density Texture stays usable.
            const float normalization = 1.0f / juce::jmax(1.0f, sumWindow * 0.72f);
            wetL *= normalization;
            wetR *= normalization;
        }
        // A short fade prevents sudden output jumps at record completion/reset.
        smoothLevel += (targetLevel - smoothLevel) * 0.0009f;
        wetL *= smoothLevel * gate * (currentNote < 0 ? 1.0f : juce::jlimit(0.4f, 1.0f, midiGain * 1.25f));
        wetR *= smoothLevel * gate * (currentNote < 0 ? 1.0f : juce::jlimit(0.4f, 1.0f, midiGain * 1.25f));
        smoothWet += 0.001f * (mix - smoothWet);
        const float dryGain = 1.0f - smoothWet;
        // Remove DC from accumulated random textures before the soft saturator.
        const float vL = dryL * dryGain + wetL * smoothWet;
        const float vR = dryR * dryGain + wetR * smoothWet;
        const float hpL = vL - dcInputL + 0.995f * dcOutputL;
        const float hpR = vR - dcInputR + 0.995f * dcOutputR;
        dcInputL = vL; dcInputR = vR;
        dcOutputL = hpL; dcOutputR = hpR;
        // Soft clipping is peak protection, not a true-peak limiter.
        outL[i] = std::tanh(hpL * outputGain);
        outR[i] = std::tanh(hpR * outputGain);
    }
}

// XML state carries a compressed stereo 2-second capture. Version 1 remains backwards compatible.
void WeaveProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::MemoryBlock raw;
    {
        const juce::ScopedLock lock(sampleMutex);
        juce::MemoryOutputStream stream(raw, false);
        const int count = sampleReady.load() ? sampleLength : 0;
        stream.writeInt(count);
        stream.writeDouble(rateHz);
        for (int ch = 0; ch < 2; ++ch)
            if (count > 0) stream.write(capture.getReadPointer(ch), sizeof(float) * static_cast<size_t>(count));
    }
    juce::MemoryOutputStream compressed;
    { juce::GZIPCompressorOutputStream gzip(compressed, 6); gzip.write(raw.getData(), raw.getSize()); }
    auto state = parameters.copyState();
    state.setProperty("sampleData", compressed.getMemoryBlock().toBase64Encoding(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void WeaveProcessor::applyRestoredSample() noexcept
{
    if (restoredAudio.getNumSamples() == 0 || sampleLength < 2 || capture.getNumSamples() != sampleLength) return;
    const int count = restoredAudio.getNumSamples();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < sampleLength; ++i)
        {
            const double pos = static_cast<double>(i) * restoredRate / rateHz;
            const int a = juce::jlimit(0, count - 1, static_cast<int>(pos));
            const int b = juce::jmin(a + 1, count - 1);
            capture.setSample(ch, i, juce::jmap(static_cast<float>(pos - a), restoredAudio.getSample(ch, a), restoredAudio.getSample(ch, b)));
        }
    loadedSampleRate = rateHz;
    captured = sampleLength;
    sampleReady.store(true); recordingDisplay.store(false);
    targetLevel = 1.0f;
}

void WeaveProcessor::setStateInformation(const void* ptr, int bytes)
{
    auto xml = getXmlFromBinary(ptr, bytes);
    if (!xml || !xml->hasTagName(parameters.state.getType())) return;
    auto state = juce::ValueTree::fromXml(*xml);
    const auto encoded = state.getProperty("sampleData").toString();
    state.removeProperty("sampleData", nullptr);
    parameters.replaceState(state);
    if (encoded.isEmpty()) return;
    juce::MemoryBlock compressed;
    if (!compressed.fromBase64Encoding(encoded)) return;
    juce::MemoryInputStream input(compressed, false);
    juce::GZIPDecompressorInputStream gzip(input);
    juce::MemoryBlock uncompressed;
    juce::MemoryOutputStream raw(uncompressed, false);
    if (raw.writeFromInputStream(gzip, 4 * 1024 * 1024) <= 0) return;
    juce::MemoryInputStream reader(uncompressed, false);
    const int count = reader.readInt();
    const double sourceRate = reader.readDouble();
    if (count < 1 || count > 384000 || !(sourceRate >= 8000 && sourceRate <= 192000)) return;
    if (reader.getNumBytesRemaining() < static_cast<int64_t>(count) * 2 * static_cast<int>(sizeof(float))) return;
    juce::AudioBuffer<float> decoded(2, count);
    for (int ch = 0; ch < 2; ++ch) reader.read(decoded.getWritePointer(ch), sizeof(float) * static_cast<size_t>(count));
    const juce::ScopedLock lock(sampleMutex);
    restoredAudio.makeCopyOf(decoded);
    restoredRate = sourceRate;
    applyRestoredSample();
}

bool WeaveProcessor::importWav(const juce::File& file)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(file));
    if (!reader || reader->lengthInSamples <= 0) return false;
    const int count = static_cast<int>(juce::jmin(reader->lengthInSamples, static_cast<juce::int64>(reader->sampleRate * 2.1)));
    juce::AudioBuffer<float> decoded(2, count);
    if (!reader->read(&decoded, 0, count, 0, true, true)) return false;
    const juce::ScopedLock lock(sampleMutex);
    if (sampleLength < 2) return false;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < sampleLength; ++i)
        {
            const double sourcePos = static_cast<double>(i) * reader->sampleRate / rateHz;
            const int a = juce::jlimit(0, count - 1, static_cast<int>(sourcePos));
            const int b = juce::jmin(a + 1, count - 1);
            capture.setSample(ch, i, juce::jmap(static_cast<float>(sourcePos - a), decoded.getSample(ch, a), decoded.getSample(ch, b)));
        }
    grains = {}; captured = sampleLength;
    loadedSampleRate = rateHz;
    restoredAudio.makeCopyOf(capture); restoredRate = rateHz;
    sampleReady.store(true); recordingDisplay.store(false); targetLevel = 1.0f;
    regenerateSound();
    return true;
}

bool WeaveProcessor::exportWav(const juce::File& file)
{
    juce::AudioBuffer<float> snapshot;
    {
        const juce::ScopedLock lock(sampleMutex);
        if (!sampleReady.load()) return false;
        snapshot.makeCopyOf(capture);
    }
    if (file.existsAsFile() && !file.deleteFile()) return false;
    auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
    if (!stream) return false;
    juce::WavAudioFormat format;
    auto* writer = format.createWriterFor(stream.get(), rateHz, 2, 24, {}, 0);
    if (!writer) return false;
    stream.release();
    std::unique_ptr<juce::AudioFormatWriter> owner(writer);
    return owner->writeFromAudioSampleBuffer(snapshot, 0, snapshot.getNumSamples());
}

void WeaveProcessor::savePreset(const juce::File& file)
{
    juce::MemoryBlock saved;
    getStateInformation(saved);
    file.replaceWithData(saved.getData(), saved.getSize());
}

bool WeaveProcessor::loadPreset(const juce::File& file)
{
    juce::MemoryBlock block;
    if (!file.loadFileAsData(block) || block.getSize() > 4 * 1024 * 1024) return false;
    setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    return true;
}

juce::AudioProcessorEditor* WeaveProcessor::createEditor() { return new WeaveEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WeaveProcessor(); }
