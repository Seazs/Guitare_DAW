#include "ReverbEffect.h"

ReverbEffect::ReverbEffect()
{
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

void ReverbEffect::prepareToPlay(double /*sampleRate*/, int /*blockSize*/)
{
    reverb.reset();
}

void ReverbEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled())
        return;

    const float mix = juce::jlimit(0.0f, 1.0f, intensity.load());

    juce::Reverb::Parameters params;
    params.roomSize = 0.2f + (0.7f * mix);
    params.damping = 0.25f + (0.55f * mix);
    params.wetLevel = 0.05f + (0.55f * mix);
    params.dryLevel = 1.0f;
    params.width = 1.0f;
    params.freezeMode = 0.0f;

    reverb.setParameters(params);

    if (buffer.getNumChannels() >= 2)
    {
        reverb.processStereo(buffer.getWritePointer(0, startSample),
                             buffer.getWritePointer(1, startSample),
                             numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        reverb.processMono(buffer.getWritePointer(0, startSample), numSamples);
    }
}

void ReverbEffect::releaseResources()
{
    reverb.reset();
}

void ReverbEffect::resized()
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
