#pragma once

#include "EffectBase.h"
#include <array>
#include <atomic>

class PitchEffect : public EffectBase
{
public:
    PitchEffect();

    juce::String getName() const override { return "Transpose"; }
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;
    void resized() override;

private:
    void applyPreset(int presetIndex);
    int  findBestOffset(int tapIndex) const;
    static float readCubic(const float* ring, int mask, double position) noexcept;

    juce::ToggleButton enabledButton{ "Transpose On" };
    juce::Slider semitoneSlider;
    juce::Label semitoneLabel;
    juce::Slider mixSlider;
    juce::Label mixLabel;
    juce::ComboBox latencyBox;
    juce::Label latencyLabel;

    // UI thread -> audio thread
    std::atomic<float> semitones{ 0.0f };
    std::atomic<float> mix{ 1.0f };
    std::atomic<int>   latencyPreset{ 1 };

    // Audio thread only
    juce::AudioBuffer<float> ringBuffer;   // allocated once in prepareToPlay
    int ringMask = 0;
    int writePos = 0;

    int activePreset = -1;
    int grainSize    = 2048;   // sweep length of the delay (samples)
    int searchRange  = 640;    // max extra delay searched for alignment
    int corrLength   = 768;    // correlation window length

    static constexpr int minDelay = 8;

    std::array<double, 2> phase { 0.0, 0.5 };  // 0..1 per tap
    std::array<int, 2>    tapOffset { 0, 0 };  // aligned extra delay per tap

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchEffect)
};