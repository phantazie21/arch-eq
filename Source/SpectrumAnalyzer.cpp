/*
  ==============================================================================

    SpectrumAnalyzer.cpp
    Created: 28 May 2026 11:46:41pm
    Author:  User

  ==============================================================================
*/

#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer() : longFFT(longOrder), shortFFT(shortOrder), longWindow(longSize, juce::dsp::WindowingFunction<float>::hann), shortWindow(shortSize, juce::dsp::WindowingFunction<float>::hann), longFifo{}, shortFifo{}, longReady{}, shortReady{}, longWorkspace{}, shortWorkspace{}, longDisplay{}, shortDisplay{}
{
	shortLatest.fill(-100.0f);
	longLatest.fill(-100.0f);
}

void SpectrumAnalyzer::pushSamples(const juce::AudioBuffer<float>& buffer)
{
    int n = buffer.getNumSamples();
    int ch = buffer.getNumChannels();

    int longPos = longWritePos.load();
    int shortPos = shortWritePos.load();

    for (int i = 0; i < n; ++i) {
        float sample = 0.0f;
        for (int c = 0; c < ch; ++c) sample += buffer.getSample(c, i);
        sample /= (float)ch;

        longFifo[longPos] = sample;
        longPos = (longPos + 1) % longSize;

        if (--longSamplesUntilNextFFT <= 0) {
            longSamplesUntilNextFFT = longHopSize;

            for (int j = 0; j < longSize; ++j) {
                int srcIdx = (longPos + j) % longSize;
                longReady[j] = longFifo[srcIdx];
            }
            longReady_flag.store(true);
        }
        shortFifo[shortPos] = sample;
        shortPos = (shortPos + 1) % shortSize;
        if (shortPos == 0) {
            std::copy(shortFifo.begin(), shortFifo.end(), shortReady.begin());
            shortReady_flag.store(true);
        }
    }

    longWritePos.store(longPos);
    shortWritePos.store(shortPos);
}

bool SpectrumAnalyzer::processFFTIfReady()
{
    bool processed = false;

    if (longReady_flag.load()) {
        std::copy(longReady.begin(), longReady.end(), longWorkspace.begin());
        std::fill(longWorkspace.begin() + longSize, longWorkspace.end(), 0.0f);
        longReady_flag.store(false);

        longWindow.multiplyWithWindowingTable(longWorkspace.data(), longSize);
        longFFT.performFrequencyOnlyForwardTransform(longWorkspace.data());

        for (int i = 0; i < longSize / 2; ++i) {
            float mag = longWorkspace[i] / (longSize / 2);
            longLatest[i] = juce::Decibels::gainToDecibels(mag, -100.0f);
        }
        processed = true;
    }

    if (shortReady_flag.load()) {
        std::copy(shortReady.begin(), shortReady.end(), shortWorkspace.begin());
        std::fill(shortWorkspace.begin() + shortSize, shortWorkspace.end(), 0.0f);
        shortReady_flag.store(false);

        shortWindow.multiplyWithWindowingTable(shortWorkspace.data(), shortSize);
        shortFFT.performFrequencyOnlyForwardTransform(shortWorkspace.data());

        for (int i = 0; i < shortSize / 2; ++i) {
            float mag = shortWorkspace[i] / (shortSize / 2);
            shortLatest[i] = juce::Decibels::gainToDecibels(mag, -100.0f);
        }
        processed = true;
    }

    return processed;
}

void SpectrumAnalyzer::applyFrameSmoothing(float longRise, float longFall,
    float shortRise, float shortFall)
{
    auto smooth = [](auto& display, const auto& latest, float rise, float fall) {
        for (size_t i = 0; i < display.size(); ++i) {
            float f = (latest[i] > display[i]) ? rise : fall;
            display[i] += (latest[i] - display[i]) * f;
        }
        };
    smooth(longDisplay, longLatest, longRise, longFall);
    smooth(shortDisplay, shortLatest, shortRise, shortFall);
}