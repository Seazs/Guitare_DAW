#include "MainComponent.h"
#include "Effects/DistortionEffect.h"
#include "Effects/DelayEffect.h"
#include "Effects/ReverbEffect.h"
#include "Effects/Overdrive.h"
#include "Effects/CabinetEffect.h"
#include "Effects/NoiseGateEffect.h"
#include "VST/VSTEffect.h"
#include "Looper/LooperTab.h"

namespace
{
    juce::String findBestDeviceName(const juce::StringArray& deviceNames, const juce::String& preferredName)
    {
        for (const auto& name : deviceNames)
            if (name.equalsIgnoreCase(preferredName))
                return name;

        for (const auto& name : deviceNames)
            if (name.containsIgnoreCase(preferredName))
                return name;

        return {};
    }
}

class PluginListModel : public juce::ListBoxModel
{
public:
    PluginListModel(juce::KnownPluginList& list) : pluginList(list) {}

    int getNumRows() override
    {
        return pluginList.getNumTypes();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colours::lightblue);

        if (rowNumber < pluginList.getNumTypes())
        {
            auto& desc = pluginList.getTypes()[rowNumber];
            g.setColour(juce::Colours::white);
            g.drawText(desc.name, 4, 0, width - 8, height, juce::Justification::centredLeft);
        }
    }

    const juce::PluginDescription* getPluginDescription(int index) const
    {
        if (index >= 0 && index < pluginList.getNumTypes())
            return &pluginList.getTypes()[index];
        return nullptr;
    }

private:
    juce::KnownPluginList& pluginList;
};

MainComponent::MainComponent()
{
    addAndMakeVisible(tabs);
    setWantsKeyboardFocus(true);

    // Audio settings tab
    audioSettingsComponent = std::make_unique<juce::AudioDeviceSelectorComponent>(
        deviceManager,
        1, 2,
        1, 2,
        true, false, true, false);

    tabs.addTab("Audio", juce::Colours::lightgrey, audioSettingsComponent.get(), false);

    // Effects container
    effectsContainer = std::make_unique<juce::Component>();
    effectsViewport.setViewedComponent(effectsContainer.get(), false);
    effectsViewport.setScrollBarsShown(true, false);

    // Create effects
    effects.push_back(std::make_unique<DistortionEffect>());
    effects.push_back(std::make_unique<OverdriveEffect>());
    effects.push_back(std::make_unique<NoiseGateEffect>());
    effects.push_back(std::make_unique<CabinetEffect>());
    effects.push_back(std::make_unique<DelayEffect>());
    effects.push_back(std::make_unique<ReverbEffect>());


    for (auto& effect : effects)
        effectsContainer->addAndMakeVisible(effect.get());

    tabs.addTab("Effects", juce::Colours::darkgrey, &effectsViewport, false);

    // VST tab
    setupVSTTab();

    // Looper tab
    setupLoopTab();

    setSize(800, 600);
    startTimerHz(2);

    loadPersistedState();

    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
            [this](bool granted) {
                setAudioChannels(granted ? 2 : 0, 2);
                restoreAudioDeviceState();
            });
    }
    else
    {
        setAudioChannels(2, 2);
        restoreAudioDeviceState();
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    savePersistedState();
    shutdownAudio();
}

juce::File MainComponent::getPersistedStateFile() const
{
    auto baseDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(ProjectInfo::projectName);

    return baseDirectory.getChildFile("state.xml");
}

void MainComponent::loadPersistedState()
{
    persistedStateXml.reset();

    const auto stateFile = getPersistedStateFile();
    if (!stateFile.existsAsFile())
    {
        restoreVSTState();
        restoreLooperState();
        return;
    }

    persistedStateXml = juce::XmlDocument::parse(stateFile);
    if (persistedStateXml == nullptr)
    {
        juce::Logger::writeToLog("Failed to parse persisted state file: " + stateFile.getFullPathName());
        return;
    }

    restoreVSTState();
    restoreLooperState();
}

void MainComponent::savePersistedState() const
{
    auto root = std::make_unique<juce::XmlElement>("GUITARFX_STATE");

    auto audioState = deviceManager.createStateXml();
    if (audioState != nullptr)
    {
        auto audioWrapper = std::make_unique<juce::XmlElement>("AUDIO_DEVICE_STATE");
        audioWrapper->addChildElement(audioState.release());
        root->addChildElement(audioWrapper.release());
    }

    if (vstEffectsContainer != nullptr)
    {
        auto vstWrapper = std::make_unique<juce::XmlElement>("VST_CHAIN");

        for (const auto& vstEffect : vstEffects)
            vstWrapper->addChildElement(vstEffect->createStateXml().release());

        root->addChildElement(vstWrapper.release());
    }

    if (looperTabComponent != nullptr)
        root->addChildElement(looperTabComponent->createStateXml().release());

    const auto stateFile = getPersistedStateFile();
    stateFile.getParentDirectory().createDirectory();

    if (!stateFile.replaceWithText(root->toString()))
        juce::Logger::writeToLog("Failed to write persisted state file: " + stateFile.getFullPathName());
}

void MainComponent::restoreAudioDeviceState()
{
    if (persistedStateXml != nullptr)
    {
        if (auto* audioWrapper = persistedStateXml->getChildByName("AUDIO_DEVICE_STATE"))
        {
            if (auto* audioState = audioWrapper->getFirstChildElement())
            {
                if (deviceManager.initialise(2, 2, audioState, true).isEmpty())
                    return;
            }
        }
    }

    configureDefaultAudioDevice();
}

void MainComponent::restoreVSTState()
{
    if (persistedStateXml == nullptr || vstEffectsContainer == nullptr)
        return;

    if (auto* vstWrapper = persistedStateXml->getChildByName("VST_CHAIN"))
    {
        for (auto* pluginState = vstWrapper->getFirstChildElement(); pluginState != nullptr; pluginState = pluginState->getNextElement())
        {
            auto* descriptionWrapper = pluginState->getChildByName("PLUGIN_DESCRIPTION");
            if (descriptionWrapper == nullptr)
                continue;

            auto* descriptionXml = descriptionWrapper->getFirstChildElement();
            if (descriptionXml == nullptr)
                continue;

            juce::PluginDescription description;
            if (!description.loadFromXml(*descriptionXml))
                continue;

            addVSTPlugin(description, pluginState);
        }
    }
}

void MainComponent::restoreLooperState()
{
    if (persistedStateXml == nullptr || looperTabComponent == nullptr)
        return;

    if (auto* looperState = persistedStateXml->getChildByName("LOOPER_STATE"))
        looperTabComponent->restoreStateFromXml(*looperState);
}

void MainComponent::setupVSTTab()
{
    vstTabComponent = std::make_unique<juce::Component>();

    // scanButton.onClick = [this]()
    // {
    //     scanButton.setEnabled(false);
    //     scanButton.setButtonText("Scanning...");

    //     juce::Thread::launch([this]()
    //     {
    //         

    //         juce::MessageManager::callAsync([this]()
    //         {
    //             scanButton.setEnabled(true);
    //             scanButton.setButtonText("Scan for Plugins");
    //             updatePluginList();
    //         });
    //     });
    // };
    // vstTabComponent->addAndMakeVisible(scanButton);
    vstPluginManager.scanDefaultPaths();

    pluginListModel = std::make_unique<PluginListModel>(vstPluginManager.getKnownPlugins());
    pluginListBox.setModel(pluginListModel.get());
    pluginListBox.setRowHeight(24);
    vstTabComponent->addAndMakeVisible(pluginListBox);

    addPluginButton.onClick = [this]()
    {
        int selectedRow = pluginListBox.getSelectedRow();
        if (auto* model = dynamic_cast<PluginListModel*>(pluginListModel.get()))
        {
            if (auto* desc = model->getPluginDescription(selectedRow))
                addVSTPlugin(*desc);
        }
    };
    vstTabComponent->addAndMakeVisible(addPluginButton);

    vstEffectsContainer = std::make_unique<juce::Component>();
    vstEffectsViewport.setViewedComponent(vstEffectsContainer.get(), false);
    vstEffectsViewport.setScrollBarsShown(true, false);
    vstTabComponent->addAndMakeVisible(vstEffectsViewport);

    tabs.addTab("VST Plugins", juce::Colours::darkgrey, vstTabComponent.get(), false);
}

void MainComponent::setupLoopTab()
{
    looperTabComponent = std::make_unique<LooperTabComponent>();
    tabs.addTab("Looper", juce::Colours::darkgrey, looperTabComponent.get(), false);
}



void MainComponent::updatePluginList()
{
    pluginListBox.updateContent();
    pluginListBox.repaint();
}

VSTEffect* MainComponent::addVSTPlugin(const juce::PluginDescription& description, const juce::XmlElement* stateToRestore)
{
    auto vstEffect = std::make_unique<VSTEffect>(vstPluginManager, description);
    vstEffect->setOnRemoveCallback([this](VSTEffect* effect) { removeVSTPlugin(effect); });
    vstEffect->prepareToPlay(currentSampleRate, currentBlockSize);

    auto* effect = vstEffect.get();

    if (stateToRestore != nullptr)
        effect->restoreStateFromXml(*stateToRestore);

    vstEffectsContainer->addAndMakeVisible(effect);
    vstEffects.push_back(std::move(vstEffect));
    updateVSTEffectsLayout();

    return effect;
}

void MainComponent::removeVSTPlugin(VSTEffect* effect)
{
    auto it = std::find_if(vstEffects.begin(), vstEffects.end(),
        [effect](const std::unique_ptr<VSTEffect>& e) { return e.get() == effect; });

    if (it != vstEffects.end())
    {
        vstEffectsContainer->removeChildComponent(it->get());
        vstEffects.erase(it);
        updateVSTEffectsLayout();
    }
}

void MainComponent::updateVSTEffectsLayout()
{
    const int margin = 16;
    const int spacing = 8;
    const int effectHeight = 50;

    int requiredHeight = margin * 2 + static_cast<int>(vstEffects.size()) * (effectHeight + spacing);
    const int containerWidth = juce::jmax(1, vstEffectsViewport.getWidth());
    const int containerHeight = juce::jmax(requiredHeight, vstEffectsViewport.getHeight());
    vstEffectsContainer->setSize(containerWidth, containerHeight);

    auto bounds = vstEffectsContainer->getLocalBounds().reduced(margin);

    for (auto& effect : vstEffects)
    {
        effect->setBounds(bounds.removeFromTop(effectHeight));
        bounds.removeFromTop(spacing);
    }
}

void MainComponent::configureDefaultAudioDevice()
{
    const juce::String preferredType = "ASIO";
    const juce::String preferredDeviceName = "Focusrite USB ASIO";

    if (deviceManager.getCurrentAudioDeviceType() != preferredType)
        deviceManager.setCurrentAudioDeviceType(preferredType, true);

    auto* deviceType = deviceManager.getCurrentDeviceTypeObject();

    if (deviceType == nullptr)
    {
        juce::Logger::writeToLog("Audio setup: no current device type object available.");
        return;
    }

    auto setup = deviceManager.getAudioDeviceSetup();

    const auto inputDeviceNames = deviceType->getDeviceNames(true);
    const auto outputDeviceNames = deviceType->getDeviceNames(false);

    const auto selectedInputName = findBestDeviceName(inputDeviceNames, preferredDeviceName);
    const auto selectedOutputName = findBestDeviceName(outputDeviceNames, preferredDeviceName);

    if (selectedInputName.isNotEmpty())
        setup.inputDeviceName = selectedInputName;

    if (selectedOutputName.isNotEmpty())
        setup.outputDeviceName = selectedOutputName;

    if (selectedInputName.isEmpty() && selectedOutputName.isEmpty())
    {
        juce::Logger::writeToLog("Audio setup: preferred ASIO device not found: " + preferredDeviceName);
        return;
    }

    const auto setupError = deviceManager.setAudioDeviceSetup(setup, true);

    if (setupError.isNotEmpty())
        juce::Logger::writeToLog("Audio setup: failed applying preferred device setup: " + setupError);
    else
        juce::Logger::writeToLog("Audio setup: using ASIO device " + preferredDeviceName);
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlockExpected;

    for (auto& effect : effects)
        effect->prepareToPlay(sampleRate, samplesPerBlockExpected);

    for (auto& vstEffect : vstEffects)
        vstEffect->prepareToPlay(sampleRate, samplesPerBlockExpected);

    if (looperTabComponent != nullptr)
        looperTabComponent->prepareToPlay(sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;

    // Process built-in effects
    for (auto& effect : effects)
        effect->process(*buffer, bufferToFill.startSample, bufferToFill.numSamples);

    // Process VST effects
    for (auto& vstEffect : vstEffects)
        vstEffect->process(*buffer, bufferToFill.startSample, bufferToFill.numSamples);

    if (looperTabComponent != nullptr)
        looperTabComponent->process(*buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    for (auto& effect : effects)
        effect->releaseResources();

    for (auto& vstEffect : vstEffects)
        vstEffect->releaseResources();

    if (looperTabComponent != nullptr)
        looperTabComponent->releaseResources();
}

void MainComponent::timerCallback()
{
    auto* audioDevice = deviceManager.getCurrentAudioDevice();
    if (audioDeviceNeedsRecovery.load()
        || audioDevice == nullptr
        || !audioDevice->isOpen()
        || !audioDevice->isPlaying())
        tryRestoreAudioDevice();
}

void MainComponent::tryRestoreAudioDevice()
{
    if (restoringAudioDevice)
        return;

    restoringAudioDevice = true;
    deviceManager.closeAudioDevice();
    restoreAudioDeviceState();
    restoringAudioDevice = false;

    if (deviceManager.getCurrentAudioDevice() != nullptr)
        audioDeviceNeedsRecovery.store(false);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    tabs.setBounds(getLocalBounds());

    if (effectsContainer == nullptr)
        return;

    const auto getEffectHeight = [](const EffectBase* effect)
    {
        if (dynamic_cast<const DelayEffect*>(effect) != nullptr)
            return 102;

        if (dynamic_cast<const OverdriveEffect*>(effect) != nullptr)
            return 102;

        if (dynamic_cast<const CabinetEffect*>(effect) != nullptr)
            return 102;

        return 66;
    };

    const int margin = 16;
    const int spacing = 12;

    int requiredHeight = margin * 2;
    for (size_t i = 0; i < effects.size(); ++i)
    {
        requiredHeight += getEffectHeight(effects[i].get());
        if (i + 1 < effects.size())
            requiredHeight += spacing;
    }

    const int containerWidth = juce::jmax(1, effectsViewport.getWidth());
    const int containerHeight = juce::jmax(requiredHeight, effectsViewport.getHeight());
    effectsContainer->setSize(containerWidth, containerHeight);

    auto effectBounds = effectsContainer->getLocalBounds().reduced(margin);

    for (size_t i = 0; i < effects.size(); ++i)
    {
        effects[i]->setBounds(effectBounds.removeFromTop(getEffectHeight(effects[i].get())));
        if (i + 1 < effects.size())
            effectBounds.removeFromTop(spacing);
    }

    // Layout VST tab
    if (vstTabComponent != nullptr)
    {
        auto vstBounds = vstTabComponent->getLocalBounds().reduced(16);
        auto topSection = vstBounds.removeFromTop(200);

        scanButton.setBounds(topSection.removeFromTop(30));
        topSection.removeFromTop(8);

        auto listSection = topSection.removeFromTop(120);
        pluginListBox.setBounds(listSection);
        topSection.removeFromTop(8);

        addPluginButton.setBounds(topSection.removeFromTop(30));

        vstBounds.removeFromTop(16);
        vstEffectsViewport.setBounds(vstBounds);
        updateVSTEffectsLayout();
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (looperTabComponent != nullptr && looperTabComponent->handleKeyPress(key))
        return true;

    return false;
}
