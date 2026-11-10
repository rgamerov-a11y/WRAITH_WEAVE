#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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
    capture.setSize(2, sampleLength, false, true, true);
    capture.clear();
    captured = 0;
    textureClock = rhythmClock = 0;
    grains = {};
    sampleReady.store(false);
    recordingDisplay.store(false);
    smoothWet = 0;
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

void WeaveProcessor::spawnGrain(bool rhythm, float variation) noexcept
{
    for (auto& g : grains)
    {
        if (g.active) continue;
        const auto lenSeconds = rhythm ? (0.035f + random01() * 0.24f) : (0.18f + random01() * 0.65f);
        g.length = juce::jlimit(32, sampleLength, static_cast<int>(lenSeconds * rateHz));
        g.age = 0;
        g.cursor = random01() * (sampleLength - 1);
        const float transpose = (random01() * 2.0f - 1.0f) * (rhythm ? 12.0f : 7.0f) * variation;
        g.step = std::pow(2.0, transpose / 12.0);
        if (random01() < variation * 0.35f) g.step *= -1.0;
        g.pan = random01() * 2.0f - 1.0f;
        g.gain = rhythm ? 0.65f : 0.27f;
        g.active = true;
        break;
    }
}

void WeaveProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inputs = getTotalNumInputChannels();
    const auto* inL = buffer.getReadPointer(0);
    const auto* inR = inputs > 1 ? buffer.getReadPointer(1) : inL;
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);

    if (resetRequest.exchange(false))
    {
        sampleReady.store(false); recordingDisplay.store(false);
        captured = 0; grains = {};
    }
    if (recordRequest.exchange(false))
    {
        captured = 0; grains = {};
        sampleReady.store(false); recordingDisplay.store(true);
    }
    if (regenerateRequest.exchange(false))
    {
        seed ^= 0x9E3779B9u + nextRand();
        grains = {};
        textureClock = rhythmClock = 0;
    }
    const bool rhythm = *parameters.getRawParameterValue("mode") >= 0.5f;
    const float variation = *parameters.getRawParameterValue("variation");
    const float mix = *parameters.getRawParameterValue("mix");
    const float density = *parameters.getRawParameterValue("density");
    const double textureInterval = rateHz * (0.115 - 0.09 * density);
    // Rhythm uses the host tempo, falling back to 120 BPM when unavailable.
    double bpm = 120.0;
    if (auto* playhead = getPlayHead())
        if (auto position = playhead->getPosition())
            if (auto hostBpm = position->getBpm()) bpm = juce::jlimit(30.0, 300.0, *hostBpm);
    const double stepSamples = rateHz * 60.0 / bpm / 4.0;

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
            }
        }
        float wetL = 0, wetR = 0;
        if (sampleReady.load(std::memory_order_relaxed))
        {
            if (rhythm)
            {
                if (rhythmClock <= 0)
                {
                    if (random01() < 0.25f + 0.75f * density) spawnGrain(true, variation);
                    rhythmClock += stepSamples;
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
            for (auto& g : grains)
            {
                if (!g.active) continue;
                const float phase = static_cast<float>(g.age) / static_cast<float>(g.length);
                const float window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
                const float left = std::sqrt((1.0f - g.pan) * 0.5f);
                const float right = std::sqrt((1.0f + g.pan) * 0.5f);
                wetL += readSample(0, g.cursor) * window * g.gain * left;
                wetR += readSample(1, g.cursor) * window * g.gain * right;
                g.cursor += g.step;
                if (++g.age >= g.length) g.active = false;
            }
        }
        smoothWet += 0.001f * (mix - smoothWet);
        const float dryGain = 1.0f - smoothWet;
        // soft clip limits accidental peaks but is not a true-peak limiter.
        outL[i] = std::tanh(dryL * dryGain + wetL * smoothWet);
        outR[i] = std::tanh(dryR * dryGain + wetR * smoothWet);
    }
}

void WeaveProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto xml = parameters.copyState().createXml();
    copyXmlToBinary(*xml, data);
}

void WeaveProcessor::setStateInformation(const void* ptr, int bytes)
{
    if (auto xml = getXmlFromBinary(ptr, bytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* WeaveProcessor::createEditor() { return new WeaveEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WeaveProcessor(); }
