#pragma once

#include <JuceHeader.h>
#include <atomic>

class EffectBase : public juce::Component
{
public:
    virtual ~EffectBase() = default;

    virtual juce::String getName() const = 0;
    virtual void prepareToPlay(double sampleRate, int blockSize) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) = 0;
    virtual void releaseResources() = 0;

    bool isEnabled() const { return enabled.load(); }
    void setEnabled(bool e) { enabled.store(e); }

protected:
    std::atomic<bool> enabled{ false };
};
