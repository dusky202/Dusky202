#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

MimicProcessor::MimicProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "MIMIC", createLayout())
{
    raw.artist  = state.getRawParameterValue (ParamID::artist);
    raw.amount  = state.getRawParameterValue (ParamID::amount);
    raw.tune    = state.getRawParameterValue (ParamID::tune);
    raw.air     = state.getRawParameterValue (ParamID::air);
    raw.grit    = state.getRawParameterValue (ParamID::grit);
    raw.space   = state.getRawParameterValue (ParamID::space);
    raw.width   = state.getRawParameterValue (ParamID::width);
    raw.formant = state.getRawParameterValue (ParamID::formant);
    raw.key     = state.getRawParameterValue (ParamID::key);
    raw.scale   = state.getRawParameterValue (ParamID::scale);
    raw.output  = state.getRawParameterValue (ParamID::output);
}

AudioProcessorValueTreeState::ParameterLayout MimicProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray artists;
    for (int i = 0; i < mimic::kNumArtists; ++i) artists.add (String::fromUTF8 (mimic::kArtists[i].name));

    StringArray scales;
    for (int i = 0; i < mimic::kNumScales; ++i) scales.add (String::fromUTF8 (mimic::kScaleNames[i]));

    const StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%")
                                                          .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; }));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::artist, 1 }, "Artist", artists, 0));
    layout.add (pct (ParamID::amount, "Amount", 100.0f));
    layout.add (pct (ParamID::tune,   "Tune",   50.0f));
    layout.add (pct (ParamID::air,    "Air",    50.0f));
    layout.add (pct (ParamID::grit,   "Grit",   50.0f));
    layout.add (pct (ParamID::space,  "Space",  50.0f));
    layout.add (pct (ParamID::width,  "Width",  50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::formant, 1 }, "Formant",
                                                       NormalisableRange<float> (-4.0f, 4.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("st")
                                                           .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::key, 1 }, "Key", keys, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::scale, 1 }, "Scale", scales, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")
                                                           .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));
    return layout;
}

bool MimicProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
}

void MimicProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate);
    chain.setSettings (mimic::resolve (mimic::kArtists[0], readMacros()));
    setLatencySamples (chain.getLatency());
    monoScratch.setSize (2, jmax (1, samplesPerBlock), false, false, true);
}

mimic::Macros MimicProcessor::readMacros() const
{
    mimic::Macros m;
    m.amount = raw.amount->load() * 0.01f;
    m.tune   = raw.tune->load()   * 0.01f;
    m.air    = raw.air->load()    * 0.01f;
    m.grit   = raw.grit->load()   * 0.01f;
    m.space  = raw.space->load()  * 0.01f;
    m.width  = raw.width->load()  * 0.01f;
    m.formantOffset = raw.formant->load();
    m.outputDb = raw.output->load();
    m.key   = (int) raw.key->load();
    m.scale = (int) raw.scale->load();
    return m;
}

void MimicProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                chain.setTempo (*bpm);

    const int artist = jlimit (0, mimic::kNumArtists - 1, (int) raw.artist->load());
    chain.setSettings (mimic::resolve (mimic::kArtists[artist], readMacros()));

    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (monoScratch.getNumSamples() < n) monoScratch.setSize (2, n, false, false, true);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = inCh > 1 ? buffer.getReadPointer (1) : inL;

    if (outCh > 1)
    {
        chain.process (inL, inR, buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }
    else
    {
        float* l = monoScratch.getWritePointer (0);
        float* r = monoScratch.getWritePointer (1);
        chain.process (inL, inR, l, r, n);
        float* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (l[i] + r[i]);
    }

    const auto& t = chain.telemetry;
    meters.inDb.store (t.inDb);
    meters.outDb.store (t.outDb);
    meters.compGr.store (t.compGr);
    meters.deessGr.store (t.deessGr);
    meters.detectedMidi.store (t.detectedMidi);
    meters.targetMidi.store (t.targetMidi);
    meters.outputMidi.store (t.outputMidi);
}

AudioProcessorEditor* MimicProcessor::createEditor()
{
    return new MimicEditor (*this);
}

void MimicProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MimicProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MimicProcessor();
}
