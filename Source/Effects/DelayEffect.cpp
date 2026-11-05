#include "DelayEffect.h"

DelayEffect::DelayEffect()
{
    intensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    intensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    intensitySlider.setRange(0.0, 1.0, 0.01);
    intensitySlider.setValue(intensity.load());
    intensitySlider.onValueChange = [this] {
        intensity.store(static_cast<float>(intensitySlider.getValue()));
    };

    timeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    timeSlider.setRange(0.1, 2.0, 0.01);
    timeSlider.setValue(delayTime.load());
    timeSlider.onValueChange = [this] {
        delayTime.store(static_cast<float>(timeSlider.getValue()));
    };

    intensityLabel.setText("Intensity", juce::dontSendNotification);
    intensityLabel.setJustificationType(juce::Justification::centredLeft);
    timeLabel.setText("Time (s)", juce::dontSendNotification);
    timeLabel.setJustificationType(juce::Justification::centredLeft);

    enabledButton.setToggleState(enabled.load(), juce::dontSendNotification);
    enabledButton.onClick = [this] {
        enabled.store(enabledButton.getToggleState());
    };

    addAndMakeVisible(enabledButton);
    addAndMakeVisible(intensitySlider);
    addAndMakeVisible(intensityLabel);
    addAndMakeVisible(timeSlider);
    addAndMakeVisible(timeLabel);
}

void DelayEffect::prepareToPlay(double sampleRate, int /*blockSize*/)
{
    currentSampleRate = sampleRate;
    const int maxDelaySamples = static_cast<int>(2.0 * currentSampleRate);

    delayBuffer.setSize(2, juce::jmax(1, maxDelaySamples));
    delayBuffer.clear();
    writePosition = 0;
}

void DelayEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled())
        return;

    const float mix = juce::jlimit(0.0f, 1.0f, intensity.load());
    const int numChannels = juce::jmin(buffer.getNumChannels(), delayBuffer.getNumChannels());
    const int bufferLength = delayBuffer.getNumSamples();

    if (numChannels == 0 || bufferLength == 0)
        return;

    const int delaySamples = juce::jlimit(1, bufferLength - 1,
        static_cast<int>(delayTime.load() * currentSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const int readPosition = (writePosition + bufferLength - delaySamples) % bufferLength;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* samples = buffer.getWritePointer(channel, startSample);
            auto* delayData = delayBuffer.getWritePointer(channel);

            const float input = samples[i];
            const float delayedSample = delayData[readPosition];

            delayData[writePosition] = input + (delayedSample * feedback);
            samples[i] = input + (delayedSample * mix);
        }

        writePosition = (writePosition + 1) % bufferLength;
    }
}

void DelayEffect::releaseResources()
{
    delayBuffer.setSize(0, 0);
    writePosition = 0;
}

void DelayEffect::resized()
{
    auto bounds = getLocalBounds();
    const int rowHeight = 30;
    const int labelWidth = 120;
    const int labelGap = 8;

    enabledButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(6);

    {
        auto row = bounds.removeFromTop(rowHeight);
        intensityLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(labelGap);
        intensitySlider.setBounds(row);
    }
    bounds.removeFromTop(6);
    {
        auto row = bounds.removeFromTop(rowHeight);
        timeLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(labelGap);
        timeSlider.setBounds(row);
    }
}
