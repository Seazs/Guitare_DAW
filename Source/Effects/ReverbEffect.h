#pragma once

#include "EffectBase.h"

class ReverbEffect : public EffectBase
{
public:
    ReverbEffect();

    juce::String getName() const override { return "Reverb"; }
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;
    void resized() override;

private:
    juce::ToggleButton enabledButton{ "Reverb On" };
    juce::Slider intensitySlider;
    juce::Label intensityLabel;

    std::atomic<float> intensity{ 0.3f };
    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbEffect)
};
