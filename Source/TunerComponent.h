#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class TunerMeter : public juce::Component
{
public:
    void setCents(int cents);
    void paint(juce::Graphics& g) override;

private:
    int displayedCents = 0;
};

class TunerComponent : public juce::Component, private juce::Timer
{
public:
    TunerComponent();
    ~TunerComponent() override;

    void prepareToPlay(double sampleRate);
    void pushSamples(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updatePitch();

    juce::Label noteLabel;
    juce::Label centsLabel;
    juce::Label statusLabel;
    TunerMeter meter;

    juce::AbstractFifo fifo { 8192 };
    std::vector<float> ringBuffer;
    double sampleRate = 44100.0;
    bool hasPitch = false;
    double pitchLastSeen = 0.0;
    double smoothedCents = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerComponent)
};
