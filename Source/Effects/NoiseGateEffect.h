#pragma once

#include "EffectBase.h"

class NoiseGateEffect : public EffectBase
{
public:
    NoiseGateEffect();

    juce::String getName() const override { return "Noise Gate"; }
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;
    void resized() override;

private:
    juce::ToggleButton enabledButton{ "Noise Gate On" };
    juce::Slider thresholdSlider;
    juce::Label thresholdLabel;
    juce::Slider releaseSlider;
    juce::Label releaseLabel;

    std::atomic<float> threshold{ 0.6f };
    std::atomic<float> release{ 0.1f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGateEffect)
};
