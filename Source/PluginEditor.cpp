/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EqAudioProcessorEditor::EqAudioProcessorEditor(EqAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), responseCurve(audioProcessor)
{

    freqSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    gainSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    qSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);

    typeSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1f26));
    typeSelector.setColour(juce::ComboBox::textColourId, juce::Colour(0xffd0d8e0));
    typeSelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a3038));
    typeSelector.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff8090a0));

    controlStripPanel.addAndMakeVisible(typeSelector);
    controlStripPanel.addAndMakeVisible(freqSlider);
    controlStripPanel.addAndMakeVisible(gainSlider);
    controlStripPanel.addAndMakeVisible(qSlider);

    controlStripPanel.onResized = [this]() {
        auto inner = controlStripPanel.getLocalBounds();
        auto colWidth = inner.getWidth() / 4;
        typeSelector.setBounds(inner.removeFromLeft(colWidth).reduced(20, 40));
        freqSlider.setBounds(inner.removeFromLeft(colWidth));
        gainSlider.setBounds(inner.removeFromLeft(colWidth));
        qSlider.setBounds(inner.removeFromLeft(colWidth));
	};

    addAndMakeVisible(controlStripPanel);
    addAndMakeVisible(responseCurve);

    rebindControlStripTo(-1);
    controlStripVisible = false;
    controlStripPanel.setAlpha(0.0f);

    responseCurve.onSelectedBandChanged = [this](int bandIndex) {
        rebindControlStripTo(bandIndex);
        updateControlStripVisibility(bandIndex > 0);
	};
    setSize(900, 600);
}
void EqAudioProcessorEditor::rebindControlStripTo(int bandIndex)
{
    freqAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    typeAttachment.reset();

    if (bandIndex < 1) return;

    freqAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.apvts, bandID(bandIndex, "freq"), freqSlider);
    gainAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.apvts, bandID(bandIndex, "gain"), gainSlider);
    qAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.apvts, bandID(bandIndex, "q"), qSlider);
    typeAttachment = std::make_unique<ComboBoxAttachment>(
        audioProcessor.apvts, bandID(bandIndex, "type"), typeSelector);

    typeSelector.clear(juce::dontSendNotification);
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(
        audioProcessor.apvts.getParameter(bandID(bandIndex, "type")))) {
        typeSelector.addItemList(choiceParam->getAllValueStrings(), 1);
    }

    typeAttachment = std::make_unique<ComboBoxAttachment>(
        audioProcessor.apvts, bandID(bandIndex, "type"), typeSelector);
}

void EqAudioProcessorEditor::updateControlStripVisibility(bool shouldBeVisible)
{
    if (shouldBeVisible == controlStripVisible) return;
    controlStripVisible = shouldBeVisible;

    auto stripHeight = 120;
    auto editorBounds = getLocalBounds();

    auto shownBounds = juce::Rectangle<int>(0, editorBounds.getBottom() - stripHeight,
        editorBounds.getWidth(), stripHeight);
    auto hiddenBounds = shownBounds.withY(editorBounds.getBottom());

    auto targetBounds = shouldBeVisible ? shownBounds : hiddenBounds;

    juce::Desktop::getInstance().getAnimator().animateComponent(
        &controlStripPanel,
        targetBounds,
        shouldBeVisible ? 1.0f : 0.0f,   // alpha
        300,                              // duration ms
        false,                            // use proxy: no
        shouldBeVisible ? 2.0 : 0.0,     // start speed
        shouldBeVisible ? 0.0 : 2.0);    // end speed (ease-out / ease-in)
}

EqAudioProcessorEditor::~EqAudioProcessorEditor()
{
}

//==============================================================================
void EqAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    //g.drawFittedText ("arch eq", getLocalBounds(), juce::Justification::centred, 1);
}

void EqAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto stripHeight = 120;
    auto curveArea = area.withTrimmedBottom(stripHeight);

    responseCurve.setBounds(curveArea);

    auto stripBounds = juce::Rectangle<int>(0, area.getBottom() - stripHeight,
        area.getWidth(), stripHeight);

    if (controlStripVisible)
        controlStripPanel.setBounds(stripBounds);
    else
        controlStripPanel.setBounds(stripBounds.withY(area.getBottom())); // off-screen
}