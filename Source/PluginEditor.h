/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ResponseCurveComponent.h"

//==============================================================================
/**
*/

struct ControlStripPanel : juce::Component
{
	std::function<void()> onResized;
    void resized() override {
        if (onResized) onResized();
    }
};

class EqAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    EqAudioProcessorEditor (EqAudioProcessor&);
    ~EqAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void rebindControlStripTo(int bandIndex);
    void updateControlStripVisibility(bool shouldBeVisible);

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    EqAudioProcessor& audioProcessor;
	juce::Slider freqSlider, gainSlider, qSlider;
	juce::ComboBox typeSelector;
	using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
	std::unique_ptr<ComboBoxAttachment> typeAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> freqAttachment, gainAttachment, qAttachment;
    ResponseCurveComponent responseCurve;
    ControlStripPanel controlStripPanel;
	bool controlStripVisible = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqAudioProcessorEditor)
};
