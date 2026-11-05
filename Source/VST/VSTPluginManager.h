#pragma once

#include <JuceHeader.h>
#include <memory>
#include <functional>

class VSTPluginManager
{
public:
    VSTPluginManager();
    ~VSTPluginManager();

    void scanForPlugins(const juce::StringArray& paths);
    void scanDefaultPaths();

    const juce::KnownPluginList& getKnownPlugins() const { return knownPlugins; }
    juce::KnownPluginList& getKnownPlugins() { return knownPlugins; }

    std::unique_ptr<juce::AudioPluginInstance> createPluginInstance(
        const juce::PluginDescription& description,
        double sampleRate,
        int blockSize,
        juce::String& errorMessage);

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }

    void addScanCompleteCallback(std::function<void()> callback);

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    std::vector<std::function<void()>> scanCompleteCallbacks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTPluginManager)
};
