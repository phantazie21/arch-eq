/*
  ==============================================================================

    ResponseCurveComponent.h
    Created: 28 May 2026 12:43:10am
    Author:  User

  ==============================================================================
*/

#pragma once 

#include "PluginProcessor.h"

class ResponseCurveComponent : public juce::Component, public juce::Timer
{
public:
    ResponseCurveComponent(EqAudioProcessor& p);

	void paint(juce::Graphics& g) override;
	void resized() override;
	void timerCallback() override;
	void drawResponseCurve(juce::Graphics& g);
	void drawGrid(juce::Graphics& g);
	void drawBandNodes(juce::Graphics& g);
	void drawSpectrum(juce::Graphics& g);
	void mouseDoubleClick(const juce::MouseEvent& e) override;
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;
	void mouseWheelMove(const juce::MouseEvent& e,
		const juce::MouseWheelDetails& wheel) override;
	std::function<void(int)> onSelectedBandChanged;
private:
	EqAudioProcessor& audioProcessor;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResponseCurveComponent);
	juce::Rectangle<int> curveArea;
	juce::Rectangle<int> freqLabelArea;
	juce::Rectangle<int> dBLabelsArea;
	int selectedBand = -1;
	int draggingBand = -1;
	float mapXtoFrequency(int x) const;
	int mapFrequencyToX(float freq) const;
	float mapDBtoY(float db) const;
	float mapYtoDB(int y) const;
	int findInactiveBandSlot() const;
	void setBandParameter(int bandIndex, juce::StringRef suffix, float value);
	void beginParameterGesture(int bandIndex, juce::StringRef suffix);
	void setBandParameterDuringGesture(int bandIndex, juce::StringRef suffix, float value);
	void endParameterGesture(int bandIndex, juce::StringRef suffix);
	int getBandAtPosition(juce::Point<int> pos) const;
};

static juce::Colour getColourForFrequency(float freq)
{
	// Map freq logarithmically to a hue
	float minFreq = 20.0f, maxFreq = 20000.0f;
	float normalizedX = std::log(freq / minFreq) / std::log(maxFreq / minFreq);
	// normalizedX is 0..1 (left to right)

	// Easiest: just rotate hue across the full circle as freq goes left to right
	auto hue = std::fmod(0.75f + normalizedX * 0.85f, 1.0f);
	return juce::Colour::fromHSV(hue, 0.7f, 1.0f, 1.0f);
}