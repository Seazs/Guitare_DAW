#pragma once

#include "EffectBase.h"

class DelayEffect : public EffectBase
{
public:
    DelayEffect();

    juce::String getName() const override { return "Delay"; }
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;
    void resized() override;

private:
    juce::ToggleButton enabledButton{ "Delay On" };
    juce::Slider intensitySlider;
    juce::Label intensityLabel;
    juce::Slider timeSlider;
    juce::Label timeLabel;

    std::atomic<float> intensity{ 0.35f };
    std::atomic<float> delayTime{ 0.4f };

    juce::AudioBuffer<float> delayBuffer;
    int writePosition = 0;
    double currentSampleRate = 44100.0;
    static constexpr float feedback = 0.45f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayEffect)
};
