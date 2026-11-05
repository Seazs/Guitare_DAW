#include "CabinetEffect.h"

CabinetEffect::CabinetEffect()
{
    enabled.store(false);

    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setValue(mix.load());
    mixSlider.onValueChange = [this] {
        mix.store(static_cast<float>(mixSlider.getValue()));
    };

    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centredLeft);

    fileNameLabel.setText("No IR loaded", juce::dontSendNotification);
    fileNameLabel.setJustificationType(juce::Justification::centredLeft);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    enabledButton.setToggleState(enabled.load(), juce::dontSendNotification);
    enabledButton.onClick = [this] {
        enabled.store(enabledButton.getToggleState());
    };

    loadButton.onClick = [this] {
        loadIRFile();
    };

    addAndMakeVisible(enabledButton);
    addAndMakeVisible(mixSlider);
    addAndMakeVisible(mixLabel);
    addAndMakeVisible(loadButton);
    addAndMakeVisible(fileNameLabel);
}

void CabinetEffect::loadIRFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select an Impulse Response file...",
        juce::File{},
        "*.wav;*.aiff;*.flac");

    auto chooserFlags = juce::FileBrowserComponent::openMode
                      | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            loadIRFromFile(file);
        }
    });
}

void CabinetEffect::loadIRFromFile(const juce::File& file)
{
    convolution.loadImpulseResponse(
        file,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes,
        0);  // 0 = use full IR length

    irLoaded = true;
    fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
}

void CabinetEffect::prepareToPlay(double sampleRate, int blockSize)
{
    currentSpec.sampleRate = sampleRate;
    currentSpec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    currentSpec.numChannels = 2;

    convolution.prepare(currentSpec);
}

void CabinetEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled() || !irLoaded)
        return;

    const float wetMix = juce::jlimit(0.0f, 1.0f, mix.load());
    const float dryMix = 1.0f - wetMix;

    // Create a copy for dry signal
    juce::AudioBuffer<float> dryBuffer;
    if (dryMix > 0.0f)
    {
        dryBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
    }

    // Process wet signal through convolution
    juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
                                        static_cast<size_t>(buffer.getNumChannels()),
                                        static_cast<size_t>(startSample),
                                        static_cast<size_t>(numSamples));

    juce::dsp::ProcessContextReplacing<float> context(block);
    convolution.process(context);

    // Mix dry and wet
    if (dryMix > 0.0f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer(ch, startSample);
            const auto* dry = dryBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
                wet[i] = dry[i] * dryMix + wet[i] * wetMix;
        }
    }
}

void CabinetEffect::releaseResources()
{
    convolution.reset();
}

void CabinetEffect::resized()
{
    auto bounds = getLocalBounds();
    const int rowHeight = 30;
    const int labelWidth = 120;
    const int labelGap = 8;

    enabledButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(6);

    {
        auto row = bounds.removeFromTop(rowHeight);
        loadButton.setBounds(row.removeFromLeft(100));
        row.removeFromLeft(labelGap);
        fileNameLabel.setBounds(row);
    }
    bounds.removeFromTop(6);
    {
        auto row = bounds.removeFromTop(rowHeight);
        mixLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(labelGap);
        mixSlider.setBounds(row);
    }
}
