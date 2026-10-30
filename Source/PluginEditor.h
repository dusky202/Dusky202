#pragma once

#include "PluginProcessor.h"

/** WebView editor: the UI lives in ui/ (HTML/CSS/JS), embedded in the binary.
    Parameters are bound with JUCE's Web*Relay + Web*ParameterAttachment pairs. */
class MimicEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit MimicEditor (MimicProcessor&);
    ~MimicEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    juce::WebBrowserComponent::Options makeOptions();
    static juce::var artistTable();

    MimicProcessor& proc;

    // relays (must be declared before the browser that uses them)
    juce::WebComboBoxRelay artistRelay  { ParamID::artist };
    juce::WebSliderRelay   amountRelay  { ParamID::amount };
    juce::WebSliderRelay   tuneRelay    { ParamID::tune };
    juce::WebSliderRelay   airRelay     { ParamID::air };
    juce::WebSliderRelay   gritRelay    { ParamID::grit };
    juce::WebSliderRelay   spaceRelay   { ParamID::space };
    juce::WebSliderRelay   widthRelay   { ParamID::width };
    juce::WebSliderRelay   formantRelay { ParamID::formant };
    juce::WebComboBoxRelay keyRelay     { ParamID::key };
    juce::WebComboBoxRelay scaleRelay   { ParamID::scale };
    juce::WebSliderRelay   outputRelay  { ParamID::output };

    std::unique_ptr<juce::WebBrowserComponent> browser;

    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>>   sliderAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttachments;

    juce::Label fallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MimicEditor)
};
