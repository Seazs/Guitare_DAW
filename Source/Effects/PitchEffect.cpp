#include "PitchEffect.h"
#include <cmath>

namespace
{
    // Values at 44.1 kHz, scaled to the actual sample rate.
    //                        Lowest  Low   Medium  High
    constexpr int kGrain[4]  = {  512, 1024, 2048, 4096 };
    constexpr int kSearch[4] = {  256,  384,  640,  768 };
    constexpr int kCorr[4]   = {  256,  512,  768, 1024 };
}

PitchEffect::PitchEffect()
{
    enabled.store(false);

    semitoneSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    semitoneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    semitoneSlider.setRange(-12.0, 12.0, 1.0);
    semitoneSlider.setValue(semitones.load());
    semitoneSlider.onValueChange = [this] {
        semitones.store(static_cast<float>(semitoneSlider.getValue()));
    };

    semitoneLabel.setText("Transpose", juce::dontSendNotification);
    semitoneLabel.setJustificationType(juce::Justification::centredLeft);

    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setValue(mix.load());
    mixSlider.onValueChange = [this] {
        mix.store(static_cast<float>(mixSlider.getValue()));
    };

    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centredLeft);

    latencyLabel.setText("Latency", juce::dontSendNotification);
    latencyLabel.setJustificationType(juce::Justification::centredLeft);
    latencyBox.addItem("Lowest (~8 ms, rough)", 1);
    latencyBox.addItem("Low (~15 ms)", 2);
    latencyBox.addItem("Medium (~30 ms)", 3);
    latencyBox.addItem("High (~60 ms, best quality)", 4);
    latencyBox.setSelectedId(2, juce::dontSendNotification);
    latencyBox.onChange = [this] {
        // Only publish the choice; the audio thread applies it. No realloc here.
        latencyPreset.store(latencyBox.getSelectedId() - 1);
    };

    enabledButton.setToggleState(false, juce::dontSendNotification);
    enabledButton.onClick = [this] {
        enabled.store(enabledButton.getToggleState());
    };

    addAndMakeVisible(enabledButton);
    addAndMakeVisible(semitoneSlider);
    addAndMakeVisible(semitoneLabel);
    addAndMakeVisible(mixSlider);
    addAndMakeVisible(mixLabel);
    addAndMakeVisible(latencyBox);
    addAndMakeVisible(latencyLabel);
}

void PitchEffect::prepareToPlay(double sampleRateToUse, int /*blockSize*/)
{
    sampleRate = sampleRateToUse;

    // Size the ring for the largest preset so the latency box never reallocates.
    const double scale = sampleRate / 44100.0;
    const int worstCase = juce::roundToInt((kGrain[3] + kSearch[3] + kCorr[3] + minDelay) * scale) + 64;

    ringBuffer.setSize(2, juce::nextPowerOfTwo(worstCase));
    ringBuffer.clear();
    ringMask = ringBuffer.getNumSamples() - 1;
    writePos = 0;

    activePreset = -1;   // forces applyPreset() on the next process()
}

void PitchEffect::applyPreset(int preset)
{
    preset = juce::jlimit(0, 3, preset);
    const double scale = sampleRate / 44100.0;

    grainSize   = juce::jmax(128, juce::roundToInt(kGrain[preset]  * scale));
    searchRange = juce::jmax(64,  juce::roundToInt(kSearch[preset] * scale));
    corrLength  = juce::jmax(64,  juce::roundToInt(kCorr[preset]   * scale));

    phase = { 0.0, 0.5 };
    tapOffset = { 0, 0 };
    activePreset = preset;
}

float PitchEffect::readCubic(const float* ring, int mask, double position) noexcept
{
    const double fl = std::floor(position);
    const float frac = static_cast<float>(position - fl);
    const int i = static_cast<int>(fl);

    const float y0 = ring[(i - 1) & mask];
    const float y1 = ring[i & mask];
    const float y2 = ring[(i + 1) & mask];
    const float y3 = ring[(i + 2) & mask];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// Called when tap k wraps (its window gain is 0, so jumping is inaudible).
// Find the extra delay for tap k whose recent waveform best matches what the
// other tap (currently at full gain) is playing, so the crossfade is in phase.
int PitchEffect::findBestOffset(int k) const
{
    const int j = 1 - k;
    const float* ring = ringBuffer.getReadPointer(0);   // analysis on channel 0
    const int mask = ringMask;

    const int refDelay = minDelay + static_cast<int>(std::lround(phase[(size_t) j] * grainSize))
                         + tapOffset[(size_t) j];
    const int refEnd = writePos - refDelay;

    const int baseDelay = minDelay + static_cast<int>(std::lround(phase[(size_t) k] * grainSize));
    const int baseEnd = writePos - baseDelay;

    auto score = [&](int off, int stride) -> double
    {
        double dot = 0.0, energy = 0.0;
        for (int m = 0; m < corrLength; m += stride)
        {
            const float a = ring[(refEnd - m) & mask];
            const float b = ring[(baseEnd - off - m) & mask];
            dot += (double) a * b;
            energy += (double) b * b;
        }
        return dot / std::sqrt(energy + 1.0e-9);
    };

    // Coarse pass (every 2nd candidate, every 2nd sample)
    int best = 0;
    double bestScore = -1.0e30;
    for (int off = 0; off <= searchRange; off += 2)
    {
        double s = score(off, 2);
        if (s > 0.0)
            s *= 1.0 - 0.1 * off / searchRange;   // prefer lower latency on ties
        if (s > bestScore) { bestScore = s; best = off; }
    }

    // Fine pass around the winner
    const int lo = juce::jmax(0, best - 1);
    const int hi = juce::jmin(searchRange, best + 1);
    int fineBest = best;
    double fineScore = -1.0e30;
    for (int off = lo; off <= hi; ++off)
    {
        double s = score(off, 1);
        if (s > 0.0)
            s *= 1.0 - 0.1 * off / searchRange;
        if (s > fineScore) { fineScore = s; fineBest = off; }
    }
    return fineBest;
}

void PitchEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (ringBuffer.getNumSamples() == 0)
        return;

    const int numChannels = juce::jmin(buffer.getNumChannels(), ringBuffer.getNumChannels());

    const int preset = latencyPreset.load();
    if (preset != activePreset)
        applyPreset(preset);

    const double st = std::round(static_cast<double>(semitones.load()));
    const bool active = isEnabled() && std::abs(st) >= 0.5;

    float* ringPtr[2] = { nullptr, nullptr };
    float* io[2] = { nullptr, nullptr };
    for (int c = 0; c < numChannels; ++c)
    {
        ringPtr[c] = ringBuffer.getWritePointer(c);
        io[c] = buffer.getWritePointer(c, startSample);
    }

    // Bypassed: keep the history fresh so enabling doesn't play stale audio.
    if (!active)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < numChannels; ++c)
                ringPtr[c][writePos] = io[c][i];
            writePos = (writePos + 1) & ringMask;
        }
        return;
    }

    const double ratio = std::pow(2.0, st / 12.0);
    const double phaseInc = (1.0 - ratio) / static_cast<double>(grainSize);
    const float wetMix = juce::jlimit(0.0f, 1.0f, mix.load());
    const float dryMix = 1.0f - wetMix;
    constexpr double twoPi = 6.283185307179586;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < numChannels; ++c)
            ringPtr[c][writePos] = io[c][i];

        // Advance both taps; re-align a tap when it wraps
        for (int k = 0; k < 2; ++k)
        {
            double& p = phase[(size_t) k];
            p += phaseInc;
            bool wrapped = false;
            if (p >= 1.0)      { p -= 1.0; wrapped = true; }
            else if (p < 0.0)  { p += 1.0; wrapped = true; }
            if (wrapped)
                tapOffset[(size_t) k] = findBestOffset(k);
        }

        const float g0 = static_cast<float>(0.5 - 0.5 * std::cos(twoPi * phase[0]));
        const float g1 = static_cast<float>(0.5 - 0.5 * std::cos(twoPi * phase[1]));
        const double d0 = minDelay + phase[0] * grainSize + tapOffset[0];
        const double d1 = minDelay + phase[1] * grainSize + tapOffset[1];

        for (int c = 0; c < numChannels; ++c)
        {
            const float dry = io[c][i];
            const float wet = g0 * readCubic(ringPtr[c], ringMask, writePos - d0)
                            + g1 * readCubic(ringPtr[c], ringMask, writePos - d1);
            io[c][i] = dry * dryMix + wet * wetMix;
        }

        writePos = (writePos + 1) & ringMask;
    }
}

void PitchEffect::releaseResources()
{
    ringBuffer.clear();
    writePos = 0;
    phase = { 0.0, 0.5 };
    tapOffset = { 0, 0 };
}

void PitchEffect::resized()
{
    auto bounds = getLocalBounds();
    const int rowHeight = 30;
    const int labelWidth = 120;
    const int labelGap = 8;

    enabledButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(6);

    {
        auto row = bounds.removeFromTop(rowHeight);
        semitoneLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(labelGap);
        semitoneSlider.setBounds(row);
    }
    bounds.removeFromTop(6);
    {
        auto row = bounds.removeFromTop(rowHeight);
        mixLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(labelGap);
        mixSlider.setBounds(row);
    }
    bounds.removeFromTop(6);
    {
        auto row = bounds.removeFromTop(rowHeight);
        latencyLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(labelGap);
        latencyBox.setBounds(row);
    }
}