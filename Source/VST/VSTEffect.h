#pragma once

#include "../Effects/EffectBase.h"
#include "VSTPluginManager.h"
#include <memory>

class VSTEffect : public EffectBase
{
public:
    VSTEffect(VSTPluginManager& manager, const juce::PluginDescription& description);
    ~VSTEffect() override;

    juce::String getName() const override;
    void prepareToPlay(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool loadPlugin(double sampleRate, int blockSize);
    void unloadPlugin();
    bool isLoaded() const { return pluginInstance != nullptr; }

    void showPluginEditor();
    void closePluginEditor();

    std::unique_ptr<juce::XmlElement> createStateXml() const;
    bool restoreStateFromXml(const juce::XmlElement& stateXml);

    const juce::PluginDescription& getPluginDescription() const { return pluginDescription; }

private:
    VSTPluginManager& pluginManager;
    juce::PluginDescription pluginDescription;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<juce::DocumentWindow> editorWindow;

    juce::ToggleButton enableButton{ "Enable" };
    juce::TextButton editorButton{ "Editor" };
    juce::TextButton removeButton{ "Remove" };

    juce::Label nameLabel;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 128;

    juce::MemoryBlock pendingStateData;
    bool hasPendingStateData = false;
    bool pendingEnabledState = false;

    std::function<void(VSTEffect*)> onRemoveCallback;

    void applyPendingState();

public:
    void setOnRemoveCallback(std::function<void(VSTEffect*)> callback) { onRemoveCallback = std::move(callback); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTEffect)
};
