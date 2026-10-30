#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "dsp/Chain.h"

namespace ParamID
{
    inline constexpr auto artist  = "artist";
    inline constexpr auto amount  = "amount";
    inline constexpr auto tune    = "tune";
    inline constexpr auto air     = "air";
    inline constexpr auto grit    = "grit";
    inline constexpr auto space   = "space";
    inline constexpr auto width   = "width";
    inline constexpr auto formant = "formant";
    inline constexpr auto key     = "key";
    inline constexpr auto scale   = "scale";
    inline constexpr auto output  = "output";
}

class MimicProcessor : public juce::AudioProcessor
{
public:
    MimicProcessor();
    ~MimicProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return "MIMIC"; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 8.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;

    struct Meters
    {
        std::atomic<float> inDb { -100.0f }, outDb { -100.0f }, compGr { 0.0f }, deessGr { 0.0f },
                           detectedMidi { -1.0f }, targetMidi { -1.0f }, outputMidi { -1.0f };
    } meters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    mimic::Macros readMacros() const;

    mimic::Chain chain;
    juce::AudioBuffer<float> monoScratch;

    struct Raw
    {
        std::atomic<float>* artist = nullptr; std::atomic<float>* amount = nullptr;
        std::atomic<float>* tune = nullptr;   std::atomic<float>* air = nullptr;
        std::atomic<float>* grit = nullptr;   std::atomic<float>* space = nullptr;
        std::atomic<float>* width = nullptr;  std::atomic<float>* formant = nullptr;
        std::atomic<float>* key = nullptr;    std::atomic<float>* scale = nullptr;
        std::atomic<float>* output = nullptr;
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MimicProcessor)
};
