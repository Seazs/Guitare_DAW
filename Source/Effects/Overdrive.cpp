#include "Overdrive.h"

OverdriveEffect::OverdriveEffect()
{
    enabled.store(false);

    intensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    intensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    intensitySlider.setRange(0.0, 1.0, 0.01);
    intensitySlider.setValue(intensity.load());
    intensitySlider.onValueChange = [this] {
        intensity.store(static_cast<float>(intensitySlider.getValue()));
        };

    intensityLabel.setText("Intensity", juce::dontSendNotification);
    intensityLabel.setJustificationType(juce::Justification::centredLeft);

    clippingTypeLabel.setText("Clipping Type", juce::dontSendNotification);
    clippingTypeLabel.setJustificationType(juce::Justification::centredLeft);
    clippingTypeBox.addItem("Two-Stage Quadratic", 1);
    clippingTypeBox.addItem("Tanh", 2);
    clippingTypeBox.addItem("Arctangent", 3);
    clippingTypeBox.addItem("Cubic", 4);
    clippingTypeBox.addItem("Hard", 5);
    clippingTypeBox.addItem("Asymmetric", 6);
    clippingTypeBox.setSelectedId(1);

    clippingTypeBox.onChange = [this] {
        clippingType = clippingTypeBox.getSelectedId();
    };



    enabledButton.setToggleState(enabled.load(), juce::dontSendNotification);
    enabledButton.onClick = [this] {
        enabled.store(enabledButton.getToggleState());
        };

    addAndMakeVisible(enabledButton);
    addAndMakeVisible(intensitySlider);
    addAndMakeVisible(intensityLabel);
    addAndMakeVisible(clippingTypeBox);
    addAndMakeVisible(clippingTypeLabel);
}

void OverdriveEffect::prepareToPlay(double /*sampleRate*/, int /*blockSize*/)
{
}

// Two-Stage Quadratic Soft Clipping (from the paper)
float twoStageQuadraticClip(float x)
{
    const float absX = std::abs(x);
    const float sign = (x > 0.0f) - (x < 0.0f);

    if (absX > 2.0f / 3.0f)
        return sign;
    else if (absX >= 1.0f / 3.0f)
    {
        const float term = 2.0f - 3.0f * absX;
        return sign * (3.0f - term * term) / 3.0f;
    }
    else
        return 2.0f * x;
}

// Hyperbolic tangent - smooth, warm overdrive
float tanhClip(float x)
{
    return std::tanh(x);
}

// Arctangent - similar to tanh, slightly different character
float atanClip(float x)
{
    return std::atan(x) * (2.0f / juce::MathConstants<float>::pi);
}

// Cubic soft clipping - common tube amp approximation
float cubicClip(float x)
{
    if (x > 1.0f) return 2.0f / 3.0f;
    if (x < -1.0f) return -2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}

// Hard clipping - aggressive, harsh
float hardClip(float x)
{
    return juce::jlimit(-1.0f, 1.0f, x);
}

// Asymmetric soft clipping - adds even harmonics like real tubes
float asymmetricClip(float x)
{
    if (x > 0.0f)
        return std::tanh(x);
    else
        return std::tanh(x * 0.5f) * 2.0f;
}

void OverdriveEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled())
        return;

    const float mix = juce::jlimit(0.0f, 1.0f, intensity.load());

    // Drive: amplify signal BEFORE clipping (1x to 50x gain)
    const float drive = 1.0f + (mix * 49.0f);

    // Output level compensation (clipping makes it louder)
    const float outputGain = 0.7f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer(channel, startSample);

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = samples[i];

            // Apply drive (this pushes the signal into the clipping region!)
            const float driven = dry * drive;
            float clipped = 0.0f;
            // Apply clipping function - uncomment one to test:
            if (clippingType == 1)
                clipped = twoStageQuadraticClip(driven);
            else if (clippingType == 2)
                clipped = tanhClip(driven);
            else if (clippingType == 3)
                clipped = atanClip(driven);
            else if (clippingType == 4)
                clipped = cubicClip(driven);
            else if (clippingType == 5)
                clipped = hardClip(driven);
            else if (clippingType == 6)
                clipped = asymmetricClip(driven);

            // Mix dry and wet signals
            samples[i] = (dry * (1.0f - mix) + clipped * mix) * outputGain;
        }
    }
}

void OverdriveEffect::releaseResources()
{
}

void OverdriveEffect::resized()
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

	row = bounds.removeFromTop(rowHeight);
    clippingTypeLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(labelGap);
	clippingTypeBox.setBounds(row);
}
