#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "Effects/EffectBase.h"
#include "VST/VSTPluginManager.h"
#include "Looper/LooperTab.h"

class VSTEffect;

class MainComponent : public juce::AudioAppComponent, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void loadPersistedState();
    void savePersistedState() const;
    void restoreAudioDeviceState();
    void restoreVSTState();
    void restoreLooperState();
    void configureDefaultAudioDevice();
    void setupVSTTab();
    void setupLoopTab();
    void updatePluginList();
    VSTEffect* addVSTPlugin(const juce::PluginDescription& description, const juce::XmlElement* stateToRestore = nullptr);
    void removeVSTPlugin(VSTEffect* effect);
    void updateVSTEffectsLayout();
    void selectWorkspace(int tabIndex);
    void timerCallback() override;
    void tryRestoreAudioDevice();

    juce::File getPersistedStateFile() const;

    juce::TabbedComponent tabs{ juce::TabbedButtonBar::TabsAtTop };
    juce::Label appTitle;
    juce::Label appSubtitle;
    juce::Label audioStatusLabel;
    juce::TextButton pedalboardButton { "Pedalboard" };
    juce::TextButton pluginsButton { "Plugins" };
    juce::TextButton looperButton { "Looper" };
    juce::TextButton settingsButton { "Settings" };

    std::unique_ptr<juce::Component> audioSettingsComponent;
    juce::Viewport effectsViewport;
    std::unique_ptr<juce::Component> effectsContainer;

    std::vector<std::unique_ptr<EffectBase>> effects;

    // VST support
    VSTPluginManager vstPluginManager;
    std::unique_ptr<juce::Component> vstTabComponent;
    juce::TextButton scanButton{ "Scan for Plugins" };
    juce::ListBox pluginListBox;
    std::unique_ptr<juce::ListBoxModel> pluginListModel;
    juce::TextButton addPluginButton{ "Add Selected Plugin" };
    juce::Viewport vstEffectsViewport;
    std::unique_ptr<juce::Component> vstEffectsContainer;
    std::vector<std::unique_ptr<VSTEffect>> vstEffects;

    std::unique_ptr<LooperTabComponent> looperTabComponent;
    std::unique_ptr<juce::XmlElement> persistedStateXml;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 128;
    std::atomic<bool> audioDeviceNeedsRecovery { false };
    bool restoringAudioDevice = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
