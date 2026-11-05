#pragma once

#include "EffectBase.h"

class OverdriveEffect : public EffectBase
{
public:
    OverdriveEffect();

    juce::String getName() const override { return "Overdrive"; }
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;
    void resized() override;

private:
    juce::ToggleButton enabledButton{ "Overdrive On" };
    juce::Slider intensitySlider;
    juce::Label intensityLabel;
    juce::ComboBox clippingTypeBox;
    juce::Label clippingTypeLabel;
    int clippingType = 1;

    std::atomic<float> intensity{ 0.6f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverdriveEffect)
};
