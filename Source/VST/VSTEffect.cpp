#include "VSTEffect.h"

class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow(const juce::String& name, juce::AudioProcessorEditor* editor)
        : juce::DocumentWindow(name, juce::Colours::darkgrey, juce::DocumentWindow::closeButton)
    {
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};

VSTEffect::VSTEffect(VSTPluginManager& manager, const juce::PluginDescription& description)
    : pluginManager(manager)
    , pluginDescription(description)
{
    nameLabel.setText(description.name, juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    enableButton.setToggleState(false, juce::dontSendNotification);
    enableButton.onClick = [this]() { setEnabled(enableButton.getToggleState()); };
    addAndMakeVisible(enableButton);

    editorButton.onClick = [this]() { showPluginEditor(); };
    addAndMakeVisible(editorButton);

    removeButton.onClick = [this]()
    {
        if (onRemoveCallback)
            onRemoveCallback(this);
    };
    addAndMakeVisible(removeButton);
}

VSTEffect::~VSTEffect()
{
    closePluginEditor();
    unloadPlugin();
}

juce::String VSTEffect::getName() const
{
    return pluginDescription.name;
}

void VSTEffect::prepareToPlay(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    if (!isLoaded())
        loadPlugin(sampleRate, blockSize);

    if (pluginInstance != nullptr)
    {
        pluginInstance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
        pluginInstance->prepareToPlay(sampleRate, blockSize);
        applyPendingState();
    }
}

void VSTEffect::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled() || pluginInstance == nullptr)
        return;

    if (startSample == 0 && numSamples == buffer.getNumSamples())
    {
        juce::MidiBuffer midiBuffer;
        pluginInstance->processBlock(buffer, midiBuffer);
    }
    else
    {
        juce::AudioBuffer<float> subBuffer(
            buffer.getArrayOfWritePointers(),
            buffer.getNumChannels(),
            startSample,
            numSamples);

        juce::MidiBuffer midiBuffer;
        pluginInstance->processBlock(subBuffer, midiBuffer);
    }
}

void VSTEffect::releaseResources()
{
    if (pluginInstance != nullptr)
        pluginInstance->releaseResources();
}

bool VSTEffect::loadPlugin(double sampleRate, int blockSize)
{
    juce::String errorMessage;
    pluginInstance = pluginManager.createPluginInstance(
        pluginDescription,
        sampleRate,
        blockSize,
        errorMessage);

    if (pluginInstance == nullptr)
    {
        juce::Logger::writeToLog("Failed to load VST plugin: " + pluginDescription.name + " - " + errorMessage);
        return false;
    }

    juce::Logger::writeToLog("Loaded VST plugin: " + pluginDescription.name);
    return true;
}

void VSTEffect::unloadPlugin()
{
    closePluginEditor();
    pluginInstance.reset();
}

void VSTEffect::showPluginEditor()
{
    if (pluginInstance == nullptr)
        return;

    if (editorWindow != nullptr)
    {
        editorWindow->toFront(true);
        return;
    }

    if (auto* editor = pluginInstance->createEditor())
    {
        editorWindow = std::make_unique<PluginEditorWindow>(
            pluginDescription.name,
            editor);
    }
}

std::unique_ptr<juce::XmlElement> VSTEffect::createStateXml() const
{
    auto stateXml = std::make_unique<juce::XmlElement>("VST_EFFECT");
    stateXml->setAttribute("enabled", isEnabled());

    if (auto descriptionXml = pluginDescription.createXml())
    {
        auto descriptionWrapper = std::make_unique<juce::XmlElement>("PLUGIN_DESCRIPTION");
        descriptionWrapper->addChildElement(descriptionXml.release());
        stateXml->addChildElement(descriptionWrapper.release());
    }

    if (pluginInstance != nullptr)
    {
        juce::MemoryBlock pluginState;
        pluginInstance->getStateInformation(pluginState);

        if (pluginState.getSize() > 0)
        {
            juce::MemoryOutputStream encodedState;
            juce::Base64::convertToBase64(encodedState, pluginState.getData(), pluginState.getSize());
            stateXml->setAttribute("pluginState", encodedState.toString());
        }
    }

    return stateXml;
}

bool VSTEffect::restoreStateFromXml(const juce::XmlElement& stateXml)
{
    pendingEnabledState = stateXml.getBoolAttribute("enabled", false);
    hasPendingStateData = false;
    pendingStateData.reset();

    const auto stateBase64 = stateXml.getStringAttribute("pluginState");
    if (stateBase64.isNotEmpty())
    {
        pendingStateData.fromBase64Encoding(stateBase64);
        hasPendingStateData = pendingStateData.getSize() > 0;
    }

    applyPendingState();
    return true;
}

void VSTEffect::applyPendingState()
{
    setEnabled(pendingEnabledState);
    enableButton.setToggleState(pendingEnabledState, juce::dontSendNotification);

    if (pluginInstance == nullptr || !hasPendingStateData || pendingStateData.getSize() == 0)
        return;

    pluginInstance->setStateInformation(pendingStateData.getData(), static_cast<int>(pendingStateData.getSize()));
}

void VSTEffect::closePluginEditor()
{
    editorWindow.reset();
}

void VSTEffect::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colours::darkgrey.darker(0.2f));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);
}

void VSTEffect::resized()
{
    auto bounds = getLocalBounds().reduced(8);

    enableButton.setBounds(bounds.removeFromLeft(70));
    bounds.removeFromLeft(8);

    removeButton.setBounds(bounds.removeFromRight(70));
    bounds.removeFromRight(8);

    editorButton.setBounds(bounds.removeFromRight(70));
    bounds.removeFromRight(8);

    nameLabel.setBounds(bounds);
}
