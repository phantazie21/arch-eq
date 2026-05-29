/*
  ==============================================================================

    SpectrumAnalyzer.h
    Created: 28 May 2026 11:46:41pm
    Author:  User

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class SpectrumAnalyzer {
public:
	static constexpr int longOrder = 14;
	static constexpr int longSize = 1 << longOrder;

	static constexpr int shortOrder = 11;
	static constexpr int shortSize = 1 << shortOrder;

	static constexpr float crossoverFreq = 1000.0f;

	static constexpr int longHopSize = longSize / 8;
	int longSamplesUntilNextFFT = longHopSize;

	SpectrumAnalyzer();

	void pushSamples(const juce::AudioBuffer<float>& buffer);

	bool processFFTIfReady();

	void applyFrameSmoothing(float longRise, float longFall, float shortRise, float shortFall);

	const auto& getLongMagnitudes() const { return longDisplay; }
	const auto& getShortMagnitudes() const { return shortDisplay; }
private:
	juce::dsp::FFT longFFT;
	juce::dsp::FFT shortFFT;
	juce::dsp::WindowingFunction<float> longWindow;
	juce::dsp::WindowingFunction<float> shortWindow;

	std::array<float, longSize>  longFifo, longReady, longLatest, longDisplay;
	std::array<float, longSize * 2> longWorkspace;
	std::atomic<int> longWritePos{ 0 };
	std::atomic<bool> longReady_flag{ false };

	std::array<float, shortSize>  shortFifo, shortReady, shortLatest, shortDisplay;
	std::array<float, shortSize * 2> shortWorkspace;
	std::atomic<int> shortWritePos{ 0 };
	std::atomic<bool> shortReady_flag{ false };
};
