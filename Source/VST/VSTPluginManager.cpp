#include "VSTPluginManager.h"

VSTPluginManager::VSTPluginManager()
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(new juce::VST3PluginFormat());
#endif
}

VSTPluginManager::~VSTPluginManager() = default;

void VSTPluginManager::scanForPlugins(const juce::StringArray& paths)
{
    for (auto* format : formatManager.getFormats())
    {
        juce::PluginDirectoryScanner scanner(
            knownPlugins,
            *format,
            juce::FileSearchPath(paths.joinIntoString(";")),
            true,
            juce::File());

        juce::String pluginName;
        while (scanner.scanNextFile(true, pluginName))
        {
        }
    }

    for (auto& callback : scanCompleteCallbacks)
        callback();
}

void VSTPluginManager::scanDefaultPaths()
{
    juce::StringArray paths;

#if JUCE_WINDOWS
    paths.add("C:\\Program Files\\Common Files\\VST3");
    paths.add("C:\\Program Files (x86)\\Common Files\\VST3");

    auto localAppData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    paths.add(localAppData.getChildFile("VST3").getFullPathName());
#elif JUCE_MAC
    paths.add("/Library/Audio/Plug-Ins/VST3");
    paths.add("~/Library/Audio/Plug-Ins/VST3");
#elif JUCE_LINUX
    paths.add("/usr/lib/vst3");
    paths.add("/usr/local/lib/vst3");
    paths.add("~/.vst3");
#endif

    scanForPlugins(paths);
}

std::unique_ptr<juce::AudioPluginInstance> VSTPluginManager::createPluginInstance(
    const juce::PluginDescription& description,
    double sampleRate,
    int blockSize,
    juce::String& errorMessage)
{
    return formatManager.createPluginInstance(
        description,
        sampleRate,
        blockSize,
        errorMessage);
}

void VSTPluginManager::addScanCompleteCallback(std::function<void()> callback)
{
    scanCompleteCallbacks.push_back(std::move(callback));
}
