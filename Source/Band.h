/*
  ==============================================================================

    Band.h
    Created: 27 May 2026 9:27:34pm
    Author:  User

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

using Filter = juce::dsp::IIR::Filter<float>;
using Coefficients = juce::dsp::IIR::Coefficients<float>;
using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coefficients>;

enum class FilterType
{
    Bell,
    LowShelf,
	HighShelf,
	HighPass,
	LowPass,
    Notch
};

struct Band
{
    StereoFilter filter;
};

static inline juce::String bandID(int bandIndex, juce::StringRef suffix) {
    return "band" + juce::String(bandIndex).paddedLeft('0', 2) + "_" + suffix;
}
