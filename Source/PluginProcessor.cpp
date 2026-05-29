/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EqAudioProcessor::EqAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
	 ), apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

juce::AudioProcessorValueTreeState::ParameterLayout EqAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

	juce::StringArray filterTypeChoices{ "Bell", "Low Shelf", "High Shelf",
										   "High Pass", "Low Pass", "Notch" };
	juce::NormalisableRange<float> freqRange(20.0f, 20000.0f, 1.0f);
	freqRange.setSkewForCentre(1000.0f);

	juce::NormalisableRange<float> gainRange(-24.0f, 24.0f, 0.1f);

	juce::NormalisableRange<float> qRange(0.1f, 10.0f, 0.001f);
	qRange.setSkewForCentre(0.707f);

	for (int i = 1; i <= 24; ++i) {
		float defaultFreq = 20.0f * std::pow(1000.0f / 20.0f, (i - 1) / 23.0f);
		layout.add(std::make_unique<juce::AudioParameterFloat>(bandID(i, "freq"), "Band " + juce::String(i) + " Frequency", freqRange, defaultFreq));
		layout.add(std::make_unique<juce::AudioParameterFloat>(bandID(i, "gain"), "Band " + juce::String(i) + " Gain", gainRange, 0.0f));
		layout.add(std::make_unique<juce::AudioParameterFloat>(bandID(i, "q"), "Band " + juce::String(i) + " Q", qRange, 0.707f));
		layout.add(std::make_unique<juce::AudioParameterChoice>(bandID(i, "type"), "Band " + juce::String(i) + " Type", filterTypeChoices, 0));
		layout.add(std::make_unique<juce::AudioParameterBool>(bandID(i, "active"), "Band " + juce::String(i) + " Enabled", i == 1));
	}
    return layout;
}

Coefficients::Ptr EqAudioProcessor::makeCoefficientsForBand(int bandIndex) const {
	auto freq = apvts.getRawParameterValue(bandID(bandIndex, "freq"))->load();
	auto gain = apvts.getRawParameterValue(bandID(bandIndex, "gain"))->load();
	auto q = apvts.getRawParameterValue(bandID(bandIndex, "q"))->load();
	auto type = static_cast<FilterType>(static_cast<int>(apvts.getRawParameterValue(bandID(bandIndex, "type"))->load()));
	switch (type) {
	case FilterType::Bell:
		return Coefficients::makePeakFilter(currentSampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
	case FilterType::LowShelf:
		return Coefficients::makeLowShelf(currentSampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
	case FilterType::HighShelf:
		return Coefficients::makeHighShelf(currentSampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
	case FilterType::HighPass:
		return Coefficients::makeHighPass(currentSampleRate, freq, q);
	case FilterType::LowPass:
		return Coefficients::makeLowPass(currentSampleRate, freq, q);
	case FilterType::Notch:
		return Coefficients::makeNotch(currentSampleRate, freq, q);
	default:
		return nullptr;
	}
}

EqAudioProcessor::~EqAudioProcessor()
{
}

//==============================================================================
const juce::String EqAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EqAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EqAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EqAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EqAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EqAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EqAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EqAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EqAudioProcessor::getProgramName (int index)
{
    return {};
}

void EqAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EqAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::dsp::ProcessSpec spec;
	spec.sampleRate = sampleRate;
	spec.maximumBlockSize = samplesPerBlock;
	spec.numChannels = getTotalNumOutputChannels();
    for (auto& band : bands) {
        band.filter.prepare(spec);
    }
	currentSampleRate = sampleRate;
}

void EqAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void EqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

	juce::dsp::AudioBlock<float> block(buffer);
	juce::dsp::ProcessContextReplacing<float> context(block);

    for (size_t i = 0; i < bands.size(); ++i) {
        auto& band = bands[i];
		bool active = apvts.getRawParameterValue(bandID(i + 1, "active"))->load() > 0.5f;
        if (!active)
            continue;

		auto coefficients = makeCoefficientsForBand(i + 1);

		*band.filter.state = *coefficients;
        band.filter.process(context);
    }

    spectrumAnalyzer.pushSamples(buffer);
}

//==============================================================================
bool EqAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EqAudioProcessor::createEditor()
{
    return new EqAudioProcessorEditor (*this);
}

//==============================================================================
void EqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
	auto state = apvts.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void EqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
	if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
		apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqAudioProcessor();
}
