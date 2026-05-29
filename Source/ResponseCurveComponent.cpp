/*
  ==============================================================================

    ResponseCurveComponent.cpp
    Created: 28 May 2026 12:43:10am
    Author:  User

  ==============================================================================
*/

#include "ResponseCurveComponent.h"

static float readMagAtFrequency(const float* mags, int numBins,
    int fftSize, float freqLow, float freqHigh,
    double sampleRate)
{
    float binLowFloat = freqLow * fftSize / (float)sampleRate;
    float binHighFloat = freqHigh * fftSize / (float)sampleRate;

    if (binHighFloat - binLowFloat < 1.0f)
    {
        // Less than one bin per pixel: interpolate between adjacent bins
        int binLow = juce::jlimit(0, numBins - 1, (int)std::floor(binLowFloat));
        int binHigh = juce::jlimit(0, numBins - 1, binLow + 1);
        float t = binLowFloat - binLow;
        return mags[binLow] * (1.0f - t) + mags[binHigh] * t;
    }
    else
    {
        // Multiple bins per pixel: average
        int binLow = juce::jlimit(0, numBins - 1, (int)std::floor(binLowFloat));
        int binHigh = juce::jlimit(0, numBins - 1, (int)std::ceil(binHighFloat));
        float sum = 0.0f;
        int count = 0;
        for (int b = binLow; b <= binHigh; ++b)
        {
            sum += mags[b];
            ++count;
        }
        return sum / count;
    }
}


ResponseCurveComponent::ResponseCurveComponent(EqAudioProcessor& p) : audioProcessor(p)
{
	startTimerHz(60);
}

void ResponseCurveComponent::timerCallback()
{
	repaint();
}

float ResponseCurveComponent::mapXtoFrequency(int x) const {
    float minFreq = 20.0f, maxFreq = 20000.0f;
    float relX = x - curveArea.getX();
    float normalizedX = relX / (float)curveArea.getWidth();
    return minFreq * std::pow(maxFreq / minFreq, normalizedX);
}

float ResponseCurveComponent::mapDBtoY(float db) const {
    return juce::jmap(db, -12.0f, 12.0f,
        (float)curveArea.getBottom(),
        (float)curveArea.getY());
}
float ResponseCurveComponent::mapYtoDB(int y) const
{
    return juce::jmap((float)y,
        (float)curveArea.getBottom(), (float)curveArea.getY(),
        -12.0f, 12.0f);
}

int ResponseCurveComponent::mapFrequencyToX(float freq) const {
    float minFreq = 20.0f, maxFreq = 20000.0f;
    float normalizedX = std::log(freq / minFreq) / std::log(maxFreq / minFreq);
    return curveArea.getX() + (int)(normalizedX * curveArea.getWidth());
}

void ResponseCurveComponent::beginParameterGesture(int bandIndex, juce::StringRef suffix)
{
    if (auto* param = audioProcessor.apvts.getParameter(bandID(bandIndex, suffix)))
        param->beginChangeGesture();
}

void ResponseCurveComponent::setBandParameterDuringGesture(int bandIndex,
    juce::StringRef suffix,
    float value)
{
    auto* param = audioProcessor.apvts.getParameter(bandID(bandIndex, suffix));
    if (param == nullptr) return;
    auto normalized = param->getNormalisableRange().convertTo0to1(value);
    param->setValueNotifyingHost(normalized);
}

void ResponseCurveComponent::endParameterGesture(int bandIndex, juce::StringRef suffix)
{
    if (auto* param = audioProcessor.apvts.getParameter(bandID(bandIndex, suffix)))
        param->endChangeGesture();
}

void ResponseCurveComponent::mouseDown(const juce::MouseEvent& e)
{
	auto pos = e.getPosition();
	int bandIndex = getBandAtPosition(pos);
	if (bandIndex > 0)
	{
		selectedBand = bandIndex;
		draggingBand = bandIndex;

		beginParameterGesture(bandIndex, "freq");
		beginParameterGesture(bandIndex, "gain");
	}
    else 
        selectedBand = -1;

    if (onSelectedBandChanged)
        onSelectedBandChanged(selectedBand);

    repaint();
}

void ResponseCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingBand < 0)
        return;

	auto pos = e.getPosition();
	pos.x = juce::jlimit(curveArea.getX(), curveArea.getRight() - 1, pos.x);
	pos.y = juce::jlimit(curveArea.getY(), curveArea.getBottom() - 1, pos.y);

	float newFreq = mapXtoFrequency(pos.x);
	float newGain = mapYtoDB(pos.y);

	auto type = static_cast<FilterType>(static_cast<int>(
		audioProcessor.apvts.getRawParameterValue(
			bandID(draggingBand, "type"))->load()));

    setBandParameterDuringGesture(draggingBand, "freq", newFreq);
	if (type != FilterType::HighPass && type != FilterType::LowPass && type != FilterType::Notch)
		setBandParameterDuringGesture(draggingBand, "gain", newGain);
}

void ResponseCurveComponent::mouseUp(const juce::MouseEvent& e)
{
	if (draggingBand < 0)
		return;
	endParameterGesture(draggingBand, "freq");
	endParameterGesture(draggingBand, "gain");
	draggingBand = -1;
}

void ResponseCurveComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
	int band = getBandAtPosition(e.getPosition());
	if (band < 0) return;

	auto currentQ = audioProcessor.apvts.getRawParameterValue(bandID(band, "q"))->load();

	auto factor = std::exp(wheel.deltaY * 0.65f);
	auto newQ = juce::jlimit(0.1f, 10.0f, currentQ * factor);

	setBandParameter(band, "q", newQ);
}

int ResponseCurveComponent::getBandAtPosition(juce::Point<int> pos) const
{
    constexpr float radius = 12.0f;
    constexpr float hitRadiusSquared = (radius + 4.0f) * (radius + 4.0f);

    int closest = -1;
    float closestDistSquared = hitRadiusSquared;

    for (int i = 1; i <= 24; ++i)
    {
        bool active = audioProcessor.apvts.getRawParameterValue(
            bandID(i, "active"))->load() > 0.5f;
        if (!active) continue;

        auto freq = audioProcessor.apvts.getRawParameterValue(
            bandID(i, "freq"))->load();
        auto gain = audioProcessor.apvts.getRawParameterValue(
            bandID(i, "gain"))->load();
        auto type = static_cast<FilterType>(static_cast<int>(
            audioProcessor.apvts.getRawParameterValue(
                bandID(i, "type"))->load()));

        float displayGain = (type == FilterType::HighPass
            || type == FilterType::LowPass
            || type == FilterType::Notch) ? 0.0f : gain;

        auto nodeX = mapFrequencyToX(freq);
        auto nodeY = mapDBtoY(displayGain);

        float dx = (float)(pos.x - nodeX);
        float dy = (float)(pos.y - nodeY);
        float distSquared = dx * dx + dy * dy;

        if (distSquared < closestDistSquared)
        {
            closestDistSquared = distSquared;
            closest = i;
        }
    }

    return closest;
}

void ResponseCurveComponent::drawResponseCurve(juce::Graphics& g)
{
    auto width = curveArea.getWidth();

    std::array<Coefficients::Ptr, 24> activeCoeffs;
    std::array<bool, 24> bandActive;

    for (int i = 0; i < 24; ++i)
    {
        bandActive[i] = audioProcessor.apvts.getRawParameterValue(bandID(i + 1, "active"))->load() > 0.5f;
        if (bandActive[i])
            activeCoeffs[i] = audioProcessor.makeCoefficientsForBand(i + 1);
    }

    juce::Path curve;
    auto sampleRate = audioProcessor.getCurrentSampleRate();

    for (int x = curveArea.getX(); x < curveArea.getRight(); ++x)
    {
        auto freq = mapXtoFrequency(x);
        float magnitudeDB = 0.0f;

        for (int i = 0; i < 24; ++i)
        {
            if (!bandActive[i]) continue;
            auto mag = activeCoeffs[i]->getMagnitudeForFrequency(freq, sampleRate);
            magnitudeDB += juce::Decibels::gainToDecibels(mag);
        }

        auto y = mapDBtoY(magnitudeDB);

        if (x == curveArea.getX())
            curve.startNewSubPath((float)x, (float)y);
        else
            curve.lineTo((float)x, (float)y);
    }

    juce::Path strokedCurve;
    juce::PathStrokeType strokeType(2.0f);
    strokeType.createStrokedPath(strokedCurve, curve);

    // Build the gradient across the full curveArea width
    juce::ColourGradient gradient(
        getColourForFrequency(20.0f), (float)curveArea.getX(), 0.0f,
        getColourForFrequency(20000.0f), (float)curveArea.getRight(), 0.0f,
        false);

    // Add intermediate stops for smooth multi-color transition
    for (int i = 1; i < 6; ++i) {
        float t = i / 6.0f;  // 0..1
        float freq = 20.0f * std::pow(20000.0f / 20.0f, t);  // log-spaced
        gradient.addColour(t, getColourForFrequency(freq));
    }

    g.setGradientFill(gradient);
    g.fillPath(strokedCurve);
}

void ResponseCurveComponent::drawGrid(juce::Graphics& g)
{
    // 1. Background fill (very dark)
    g.fillAll(juce::Colour(21, 26, 33));

    // 2. Vertical frequency lines + labels
    const std::array<float, 10> freqs{ 20.f, 50.f, 100.f, 200.f, 500.f,
                                         1000.f, 2000.f, 5000.f, 10000.f, 20000.f };

    g.setColour(juce::Colour(27, 31, 37));   // dim grid line
    for (auto freq : freqs)
    {
        auto x = mapFrequencyToX(freq);
        g.drawVerticalLine(x, 0.0f, (float)curveArea.getHeight());
    }

    g.setColour(juce::Colour(55, 60, 66));   // brighter label text
    g.setFont(11.0f);
    for (auto freq : freqs)
    {
        auto x = mapFrequencyToX(freq);
        auto label = freq >= 1000.f ? juce::String(freq / 1000.f, 0) + "k"
            : juce::String(freq, 0);
        // small text box at the top of the curve area
        g.drawText(label, x - 20, 0, 40, 14, juce::Justification::centred);
    }

    // 3. Horizontal dB lines + labels
    const std::array<float, 5> dbs{ -12.f, -6.f, 0.f, 6.f, 12.f };

    g.setColour(juce::Colour(27, 30, 36));
	g.drawHorizontalLine(mapDBtoY(0.0f), 0.0f, (float)curveArea.getWidth());

    for (auto db : dbs)
    {
        auto y = mapDBtoY(db);
        g.setColour(juce::Colour(55, 60, 66));
        g.drawText(juce::String((int)db) + " dB",
            curveArea.getRight() - 40, y - 7, 35, 14,
            juce::Justification::centredRight);
    }
}
void ResponseCurveComponent::drawBandNodes(juce::Graphics& g)
{
    auto sampleRate = audioProcessor.getCurrentSampleRate();

    for (int i = 0; i < 24; ++i)
    {
        bool active = audioProcessor.apvts.getRawParameterValue(
            bandID(i + 1, "active"))->load() > 0.5f;
        if (!active) continue;

        auto freq = audioProcessor.apvts.getRawParameterValue(
            bandID(i + 1, "freq"))->load();
        auto gain = audioProcessor.apvts.getRawParameterValue(
            bandID(i + 1, "gain"))->load();
        auto type = static_cast<FilterType>(static_cast<int>(
            audioProcessor.apvts.getRawParameterValue(
                bandID(i + 1, "type"))->load()));

        // For HPF/LPF/Notch, gain doesn't matter — node sits at 0 dB on the curve
        float displayGain = (type == FilterType::HighPass
            || type == FilterType::LowPass
            || type == FilterType::Notch) ? 0.0f : gain;

        auto x = mapFrequencyToX(freq);
        auto y = mapDBtoY(displayGain);

        auto colour = getColourForFrequency(freq);
        constexpr float radius = 12.0f;

        // Filled circle, semi-transparent so the curve color shows through
        g.setColour(juce::Colour(16, 21, 25));
        g.fillEllipse(x - radius, y - radius, radius * 2, radius * 2);

        g.setColour(colour.withAlpha(0.1f));
        g.fillEllipse(x - radius, y - radius, radius * 2, radius * 2);

        // Outline ring, slightly darker for definition
        g.setColour(colour);
        float ringThickness = (i + 1 == selectedBand) ? 2.5f : 1.5f;
        g.drawEllipse(x - radius, y - radius, radius * 2, radius * 2, ringThickness);

        // Band number text
        g.setColour(colour);
        g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
        g.drawText(juce::String(i + 1),
            (int)(x - radius), (int)(y - radius),
            (int)(radius * 2), (int)(radius * 2),
            juce::Justification::centred);
    }
}

int ResponseCurveComponent::findInactiveBandSlot() const
{
    for (int i = 1; i <= 24; ++i)
    {
        bool active = audioProcessor.apvts.getRawParameterValue(
            bandID(i, "active"))->load() > 0.5f;
        if (!active) return i;
    }
    return -1;
}

void ResponseCurveComponent::setBandParameter(int bandIndex, juce::StringRef suffix, float value)
{
    auto* param = audioProcessor.apvts.getParameter(bandID(bandIndex, suffix));
    if (param == nullptr) return;

    // Normalize the value into the parameter's 0..1 range
    auto normalized = param->getNormalisableRange().convertTo0to1(value);

    // Set via the host-aware path
    param->beginChangeGesture();
    param->setValueNotifyingHost(normalized);
    param->endChangeGesture();
}

void ResponseCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Convert click position to frequency
    if (!curveArea.contains(e.getPosition())) return;

    auto clickedFreq = mapXtoFrequency(e.x);
    auto clickedGainDB = mapYtoDB(e.y);   // need this inverse!

    // Find first inactive band slot
    int slot = findInactiveBandSlot();
    if (slot < 0) return;  // all 24 bands in use

    // Set its parameters
    setBandParameter(slot, "freq", clickedFreq);
    setBandParameter(slot, "gain", clickedGainDB);
    setBandParameter(slot, "q", 1.0f);  // sensible default for new band
    setBandParameter(slot, "type", 0.0f);  // bell
    setBandParameter(slot, "active", 1.0f);

	selectedBand = slot;
    if (onSelectedBandChanged)
        onSelectedBandChanged(selectedBand);
}

void ResponseCurveComponent::drawSpectrum(juce::Graphics& g)
{
    constexpr float shortFFTOffset = -8.0f;
    audioProcessor.spectrumAnalyzer.processFFTIfReady();
    audioProcessor.spectrumAnalyzer.applyFrameSmoothing(0.15f, 0.05f, 0.4f, 0.05f);

    const auto& longMags = audioProcessor.spectrumAnalyzer.getLongMagnitudes();
    const auto& shortMags = audioProcessor.spectrumAnalyzer.getShortMagnitudes();
    auto sampleRate = audioProcessor.getCurrentSampleRate();

    juce::Path spectrumPath;
    spectrumPath.startNewSubPath((float)curveArea.getX(), (float)curveArea.getBottom());

    constexpr int longSize = SpectrumAnalyzer::longSize;
    constexpr int shortSize = SpectrumAnalyzer::shortSize;
    constexpr int longBins = longSize / 2;
    constexpr int shortBins = shortSize / 2;

    // Crossover region: blend the two FFTs smoothly between these frequencies
    constexpr float crossoverLow = 1200.0f;
    constexpr float crossoverHigh = 2500.0f;

    for (int x = curveArea.getX(); x < curveArea.getRight(); ++x)
    {
        float freqLow = mapXtoFrequency(x);
        float freqHigh = mapXtoFrequency(x + 1);
        float freqCenter = (freqLow + freqHigh) * 0.5f;

        float magDb;

        if (freqCenter < crossoverLow)
        {
            magDb = readMagAtFrequency(longMags.data(), longBins, longSize,
                freqLow, freqHigh, sampleRate);
        }
        else if (freqCenter > crossoverHigh)
        {
            magDb = readMagAtFrequency(shortMags.data(), shortBins, shortSize,
                freqLow, freqHigh, sampleRate) + shortFFTOffset;
        }
        else
        {
            float t = (freqCenter - crossoverLow) / (crossoverHigh - crossoverLow);
            float magLong = readMagAtFrequency(longMags.data(), longBins, longSize,
                freqLow, freqHigh, sampleRate);
            float magShort = readMagAtFrequency(shortMags.data(), shortBins, shortSize,
                freqLow, freqHigh, sampleRate) + shortFFTOffset;
            magDb = magLong * (1.0f - t) + magShort * t;
        }

        // Pink-noise slope compensation
        float slopePerOctave = 3.0f;
        float octavesAbove1k = std::log2(freqCenter / 1000.0f);
        magDb += slopePerOctave * octavesAbove1k;

        // Map dB to Y
        float y = juce::jmap(magDb, -60.f, 0.f,
            (float)curveArea.getBottom(),
            (float)curveArea.getY());
        y = juce::jlimit((float)curveArea.getY(), (float)curveArea.getBottom(), y);
        spectrumPath.lineTo((float)x, y);
    }

    spectrumPath.lineTo((float)curveArea.getRight(), (float)curveArea.getBottom());
    spectrumPath.closeSubPath();

    g.setColour(juce::Colour(34, 38, 45).withAlpha(0.4f));
    g.fillPath(spectrumPath);

    g.setColour(juce::Colour(80, 90, 110));
    g.strokePath(spectrumPath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved));
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
	drawGrid(g);
    drawSpectrum(g);
    drawResponseCurve(g);
    drawBandNodes(g);
}

void ResponseCurveComponent::resized()
{
	curveArea = getLocalBounds().reduced(40, 20);
}
