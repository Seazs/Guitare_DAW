#include "DistortionEffect.h"

DistortionEffect::DistortionEffect()
{
    enabled.store(true);

    intensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    intensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    intensitySlider.setRange(0.0, 1.0, 0.01);
    intensitySlider.setValue(intensity.load());
    intensitySlider.onValueChange = [this] {
        intensity.store(static_cast<float>(intensitySlider.getValue()));
        };

    intensityLabel.setText("Intensity", juce::dontSendNotification);
    intensityLabel.setJustificationType(juce::Justification::centredLeft);

    enabledButton.setToggleState(enabled.load(), juce::dontSendNotification);
    enabledButton.onClick = [this] {
        enabled.store(enabledButton.getToggleState());
        };

    addAndMakeVisible(enabledButton);
    addAndMakeVisible(intensitySlider);
    addAndMakeVisible(intensityLabel);
}

void DistortionEffect::prepareToPlay(double /*sampleRate*/, int /*blockSize*/)
{
}

void DistortionEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled())
        return;

    const float mix = juce::jlimit(0.0f, 1.0f, intensity.load());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer(channel, startSample);

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = samples[i];
            const float drive = 1.0f + (mix * 29.0f);
            const float distorted = std::tanh(input * drive);
            samples[i] = juce::jmap(mix, input, distorted);
        }
    }
}

void DistortionEffect::releaseResources()
{
}

void DistortionEffect::resized()
{
    auto bounds = getLocalBounds();
    const int rowHeight = 30;
    const int labelWidth = 120;
    const int labelGap = 8;

    enabledButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(6);

    auto row = bounds.removeFromTop(rowHeight);
    intensityLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(labelGap);
    intensitySlider.setBounds(row);
}
