#pragma once

#include "EffectBase.h"

class DistortionEffect : public EffectBase
{
public:
    DistortionEffect();

    juce::String getName() const override { return "Distortion"; }
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;
    void resized() override;

private:
    juce::ToggleButton enabledButton{ "Distortion On" };
    juce::Slider intensitySlider;
    juce::Label intensityLabel;

    std::atomic<float> intensity{ 0.6f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionEffect)
};
