#pragma once

#include "EffectBase.h"

class CabinetEffect : public EffectBase
{
public:
    CabinetEffect();

    juce::String getName() const override { return "Cabinet"; }
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    void resized() override;

private:
    void loadIRFile();
    void loadIRFromFile(const juce::File& file);

    juce::ToggleButton enabledButton{ "Cabinet On" };
    juce::Slider mixSlider;
    juce::Label mixLabel;
    juce::TextButton loadButton{ "Load IR..." };
    juce::Label fileNameLabel;

    std::atomic<float> mix{ 1.0f };

    juce::dsp::Convolution convolution;
    juce::dsp::ProcessSpec currentSpec{ 44100.0, 128, 2 };
    bool irLoaded = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CabinetEffect)
};
