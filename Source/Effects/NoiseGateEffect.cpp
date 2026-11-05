#include "NoiseGateEffect.h"

NoiseGateEffect::NoiseGateEffect()
{
    enabled.store(false);

    thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    releaseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    thresholdLabel.setText("Threshold", juce::dontSendNotification);
    thresholdLabel.setJustificationType(juce::Justification::centredLeft);

    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centredLeft);

    enabledButton.setToggleState(enabled.load(), juce::dontSendNotification);
    enabledButton.onClick = [this] {
        enabled.store(enabledButton.getToggleState());
    };

    addAndMakeVisible(enabledButton);
    addAndMakeVisible(thresholdSlider);
    addAndMakeVisible(thresholdLabel);
    addAndMakeVisible(releaseSlider);
    addAndMakeVisible(releaseLabel);
}

void NoiseGateEffect::prepareToPlay(double /*sampleRate*/, int /*blockSize*/)
{
}

void NoiseGateEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled())
        return;

    const float thresholdValue = threshold.load();
    const float releaseValue = release.load();
    const float releaseFactor = std::exp(-1.0f / (releaseValue * 44100.0f)); // Assuming 44.1kHz sample rate

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer(channel, startSample);
        float gain = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = samples[i];
            if (std::abs(input) < thresholdValue)
                gain *= releaseFactor;
            else
                gain = 1.0f;

            samples[i] = input * gain;
        }
    }
}

void NoiseGateEffect::releaseResources()
{
}

void NoiseGateEffect::resized()
{
    auto bounds = getLocalBounds();
    const int rowHeight = 30;
    const int labelWidth = 120;
    const int labelGap = 8;

    enabledButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(6);

    auto row = bounds.removeFromTop(rowHeight);
    thresholdLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(labelGap);
    thresholdSlider.setBounds(row);
    bounds.removeFromTop(6);
    row = bounds.removeFromTop(rowHeight);
    releaseLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(labelGap);
    releaseSlider.setBounds(row);
    bounds.removeFromTop(6);
    
}
