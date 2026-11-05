#include "LooperTab.h"

namespace
{
    constexpr float trimThreshold = 0.0125f;
    constexpr int laneHeight = 270;
    constexpr int laneSpacing = 10;
    constexpr int layoutMargin = 16;
    constexpr int headerHeight = 118;
    constexpr int clipHeight = 74;
}

class LooperTabComponent::LaneComponent : public juce::Component
{
public:
    LaneComponent(LooperTabComponent& ownerIn, int laneIndexIn)
        : owner(ownerIn), laneIndex(laneIndexIn)
    {
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        statusLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(statusLabel);

        levelLabel.setText("Level", juce::dontSendNotification);
        levelLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(levelLabel);

        levelSlider.setRange(0.0, 2.0, 0.01);
        levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
        levelSlider.onValueChange = [this]
        {
            owner.setTrackLevel(laneIndex, static_cast<float>(levelSlider.getValue()));
        };
        addAndMakeVisible(levelSlider);

        autoTrimButton.onClick = [this]
        {
            owner.autoTrimTrack(laneIndex);
        };
        addAndMakeVisible(autoTrimButton);

        clearButton.onClick = [this]
        {
            owner.pendingLaneClearCommands[laneIndex].fetch_add(1);
        };
        addAndMakeVisible(clearButton);

        muteButton.onClick = [this]
        {
            owner.tracks[laneIndex].muted.store(muteButton.getToggleState());
        };
        addAndMakeVisible(muteButton);

        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void refresh()
    {
        const auto& track = owner.tracks[laneIndex];
        titleLabel.setText("Lane " + juce::String(laneIndex + 1), juce::dontSendNotification);
        statusLabel.setText(owner.getTrackStatusText(laneIndex), juce::dontSendNotification);

        levelSlider.setValue(track.level.load(), juce::dontSendNotification);
        muteButton.setToggleState(track.muted.load(), juce::dontSendNotification);

        const bool ready = track.ready.load();
        levelSlider.setEnabled(true);
        autoTrimButton.setEnabled(ready);
        clearButton.setEnabled(ready || track.recording.load());
        muteButton.setEnabled(ready);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const bool ready = owner.tracks[laneIndex].ready.load();
        const bool recording = owner.tracks[laneIndex].recording.load();

        juce::Colour baseColour = recording ? juce::Colour(0xff8f5cff)
                                            : ready ? juce::Colour(0xff2f82ff)
                                                    : juce::Colour(0xff252a33);

        juce::ColourGradient backgroundGradient(
            baseColour.withAlpha(0.20f), bounds.getX(), bounds.getY(),
            juce::Colour(0xff171b22), bounds.getRight(), bounds.getBottom(), false);

        g.setGradientFill(backgroundGradient);
        g.fillRoundedRectangle(bounds.reduced(1.0f), 12.0f);

        g.setColour(juce::Colour(0xff515a68));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 12.0f, 1.0f);

        auto waveformArea = getWaveformArea().toFloat();
        g.setColour(juce::Colour(0xff0f1319));
        g.fillRoundedRectangle(waveformArea, 8.0f);

        g.setColour(juce::Colour(0xff2f3742));
        g.drawRoundedRectangle(waveformArea, 8.0f, 1.0f);

        drawGrid(g, waveformArea);
        drawWaveform(g, waveformArea);
        drawTrimOverlay(g, waveformArea);
        drawPlayhead(g, waveformArea);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!owner.tracks[laneIndex].ready.load())
            return;

        auto waveformArea = getWaveformArea();
        const auto startHandle = getTrimHandleRect(waveformArea, true);
        const auto endHandle = getTrimHandleRect(waveformArea, false);

        if (startHandle.contains(event.getPosition()))
        {
            draggingStartHandle = true;
            draggingEndHandle = false;
        }
        else if (endHandle.contains(event.getPosition()))
        {
            draggingStartHandle = false;
            draggingEndHandle = true;
        }
        else if (waveformArea.contains(event.getPosition()))
        {
            const int leftDistance = std::abs(event.x - startHandle.getCentreX());
            const int rightDistance = std::abs(event.x - endHandle.getCentreX());
            draggingStartHandle = leftDistance <= rightDistance;
            draggingEndHandle = !draggingStartHandle;
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!owner.tracks[laneIndex].ready.load())
            return;

        auto waveformArea = getWaveformArea();
        const float percent = positionToPercent(event.x, waveformArea);

        if (draggingStartHandle)
        {
            owner.setTrackTrimPercent(laneIndex, true, percent);
            refresh();
        }
        else if (draggingEndHandle)
        {
            const float endPercent = juce::jlimit(0.0f, 100.0f, 100.0f - percent);
            owner.setTrackTrimPercent(laneIndex, false, endPercent);
            refresh();
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        if (!owner.tracks[laneIndex].ready.load())
            return;

        auto waveformArea = getWaveformArea();
        const auto startHandle = getTrimHandleRect(waveformArea, true);
        const auto endHandle = getTrimHandleRect(waveformArea, false);

        if (startHandle.contains(event.getPosition()) || endHandle.contains(event.getPosition()))
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else if (waveformArea.contains(event.getPosition()))
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        draggingStartHandle = false;
        draggingEndHandle = false;
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);

        auto titleRow = area.removeFromTop(20);
        titleLabel.setBounds(titleRow.removeFromLeft(90));
        statusLabel.setBounds(titleRow);
        area.removeFromTop(2);

        auto waveformRow = area.removeFromTop(clipHeight);
        waveformRow.removeFromLeft(96);
        waveformRow.removeFromRight(6);
        waveformRow.removeFromBottom(1);
        waveformArea = waveformRow;

        constexpr int levelRowHeight = 26;
        levelLabel.setBounds(area.removeFromTop(18));
        levelSlider.setBounds(area.removeFromTop(levelRowHeight));

        auto buttonRow = area.removeFromBottom(26);
        muteButton.setBounds(buttonRow.removeFromLeft(80));
        buttonRow.removeFromLeft(8);
        autoTrimButton.setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(8);
        clearButton.setButtonText("Delete Track");
        clearButton.setBounds(buttonRow.removeFromLeft(96));
    }

private:
    juce::Rectangle<int> getWaveformArea() const
    {
        return waveformArea;
    }

    juce::Rectangle<int> getTrimHandleRect(const juce::Rectangle<int>& waveformBounds, bool startHandle) const
    {
        const auto& track = owner.tracks[laneIndex];
        const float percent = startHandle ? track.trimStartPercent.load() : 100.0f - track.trimEndPercent.load();
        const int x = waveformBounds.getX() + juce::roundToInt(waveformBounds.getWidth() * (percent / 100.0f));
        return { x - 7, waveformBounds.getY() - 4, 14, waveformBounds.getHeight() + 8 };
    }

    float positionToPercent(int x, const juce::Rectangle<int>& waveformBounds) const
    {
        if (waveformBounds.getWidth() <= 0)
            return 0.0f;

        return juce::jlimit(0.0f, 100.0f,
                            100.0f * static_cast<float>(x - waveformBounds.getX()) /
                                static_cast<float>(waveformBounds.getWidth()));
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& waveformBounds)
    {
        g.setColour(juce::Colour(0xff222832));
        for (int i = 1; i < 4; ++i)
        {
            const float x = waveformBounds.getX() + waveformBounds.getWidth() * (static_cast<float>(i) / 4.0f);
            g.drawVerticalLine(juce::roundToInt(x), waveformBounds.getY(), waveformBounds.getBottom());
        }
    }

    void drawWaveform(juce::Graphics& g, const juce::Rectangle<float>& waveformBounds)
    {
        const juce::ScopedLock lock(owner.trackBufferLock);
        const auto& track = owner.tracks[laneIndex];
        const int totalSamples = track.recordedSamples.load();

        if (!track.ready.load() || totalSamples <= 0 || track.buffer.getNumSamples() <= 0)
        {
            g.setColour(juce::Colour(0xff3a4150));
            g.drawFittedText("Empty lane", waveformBounds.toNearestInt(), juce::Justification::centred, 1);
            return;
        }

        const int channelCount = juce::jmax(1, track.buffer.getNumChannels());
        const auto usable = waveformBounds.reduced(4.0f);
        const int pixelWidth = juce::jmax(1, static_cast<int>(usable.getWidth()));

        juce::Path waveformPath;
        waveformPath.preallocateSpace(pixelWidth * 6);

        const float centreY = usable.getCentreY();
        const float halfHeight = juce::jmax(3.0f, usable.getHeight() * 0.44f);

        for (int x = 0; x < pixelWidth; ++x)
        {
            const int sampleStart = juce::jlimit(0, totalSamples - 1, juce::roundToInt((static_cast<float>(x) / pixelWidth) * totalSamples));
            const int sampleEnd = juce::jlimit(sampleStart + 1, totalSamples, juce::roundToInt((static_cast<float>(x + 1) / pixelWidth) * totalSamples));

            float peak = 0.0f;
            for (int sampleIndex = sampleStart; sampleIndex < sampleEnd; ++sampleIndex)
            {
                float localPeak = 0.0f;
                for (int channel = 0; channel < channelCount; ++channel)
                    localPeak = juce::jmax(localPeak, std::abs(track.buffer.getSample(channel, sampleIndex)));
                peak = juce::jmax(peak, localPeak);
            }

            const float yOffset = juce::jlimit(1.0f, halfHeight, peak * halfHeight);
            const float xPos = usable.getX() + static_cast<float>(x);

            waveformPath.startNewSubPath(xPos, centreY - yOffset);
            waveformPath.lineTo(xPos, centreY + yOffset);
        }

        g.setColour(track.recording.load() ? juce::Colour(0xffff8ae5) : juce::Colour(0xff6ec0ff));
        g.strokePath(waveformPath, juce::PathStrokeType(1.4f));
    }

    void drawTrimOverlay(juce::Graphics& g, const juce::Rectangle<float>& waveformBounds)
    {
        const auto& track = owner.tracks[laneIndex];
        if (!track.ready.load())
            return;

        const float startPercent = track.trimStartPercent.load();
        const float endPercent = track.trimEndPercent.load();

        const float trimStartX = waveformBounds.getX() + waveformBounds.getWidth() * (startPercent / 100.0f);
        const float trimEndX = waveformBounds.getRight() - waveformBounds.getWidth() * (endPercent / 100.0f);

        g.setColour(juce::Colour(0xff1b2028).withAlpha(0.82f));
        g.fillRect(waveformBounds.getX(), waveformBounds.getY(), trimStartX - waveformBounds.getX(), waveformBounds.getHeight());
        g.fillRect(trimEndX, waveformBounds.getY(), waveformBounds.getRight() - trimEndX, waveformBounds.getHeight());

        g.setColour(juce::Colour(0xff7cd1ff));
        g.drawLine(trimStartX, waveformBounds.getY(), trimStartX, waveformBounds.getBottom(), 2.0f);
        g.drawLine(trimEndX, waveformBounds.getY(), trimEndX, waveformBounds.getBottom(), 2.0f);

        const auto startHandle = getTrimHandleRect(waveformBounds.toNearestInt(), true).toFloat();
        const auto endHandle = getTrimHandleRect(waveformBounds.toNearestInt(), false).toFloat();

        g.setColour(juce::Colour(0xff8fd3ff));
        g.fillRoundedRectangle(startHandle.reduced(1.0f), 3.0f);
        g.fillRoundedRectangle(endHandle.reduced(1.0f), 3.0f);

        g.setColour(juce::Colour(0xff12161d));
        g.fillRect(startHandle.getCentreX() - 1.0f, startHandle.getY() + 4.0f, 2.0f, startHandle.getHeight() - 8.0f);
        g.fillRect(endHandle.getCentreX() - 1.0f, endHandle.getY() + 4.0f, 2.0f, endHandle.getHeight() - 8.0f);

        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xffd8e7ff));
        g.drawText("in", juce::Rectangle<int>(juce::roundToInt(trimStartX) + 5, waveformBounds.getY() - 2, 18, 14), juce::Justification::centredLeft);
        g.drawText("out", juce::Rectangle<int>(juce::roundToInt(trimEndX) - 23, waveformBounds.getY() - 2, 24, 14), juce::Justification::centredRight);
    }

    void drawPlayhead(juce::Graphics& g, const juce::Rectangle<float>& waveformBounds)
    {
        const auto& track = owner.tracks[laneIndex];
        if (!track.ready.load() || owner.masterLoopLengthSamples.load() <= 0)
            return;

        if (!owner.isTransportPlaying())
            return;

        const int loopLength = juce::jmax(1, owner.masterLoopLengthSamples.load());
        const int playheadSample = juce::jlimit(0, loopLength - 1, owner.masterTimelinePosition.load() % loopLength);

        const float startX = waveformBounds.getX() + waveformBounds.getWidth() * (track.trimStartPercent.load() / 100.0f);
        const float endX = waveformBounds.getRight() - waveformBounds.getWidth() * (track.trimEndPercent.load() / 100.0f);
        const float spanWidth = juce::jmax(1.0f, endX - startX);
        const float x = startX + spanWidth * (static_cast<float>(playheadSample) / static_cast<float>(loopLength));

        g.setColour(juce::Colour(0xff72ff9d));
        g.drawLine(x, waveformBounds.getY(), x, waveformBounds.getBottom(), 2.0f);
    }

    LooperTabComponent& owner;
    int laneIndex = 0;
    bool draggingStartHandle = false;
    bool draggingEndHandle = false;
    juce::Rectangle<int> waveformArea;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label levelLabel;
    juce::Slider levelSlider;
    juce::ToggleButton muteButton { "Mute" };
    juce::TextButton autoTrimButton { "Auto Trim" };
    juce::TextButton clearButton { "Clear" };
    
};

LooperTabComponent::LooperTabComponent()
{
    headerLabel.setText("Looper", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    addAndMakeVisible(headerLabel);

    transportLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(transportLabel);

    recordArmButton.onClick = [this]
    {
        pendingSpaceCommands.fetch_add(1);
    };
    addAndMakeVisible(recordArmButton);

    stopButton.onClick = [this]
    {
        pendingStopCommands.fetch_add(1);
    };
    addAndMakeVisible(stopButton);

    clearButton.onClick = [this]
    {
        pendingClearCommands.fetch_add(1);
    };
    addAndMakeVisible(clearButton);

    autoTrimCapturedToggle.setToggleState(true, juce::dontSendNotification);
    autoTrimCapturedToggle.onClick = [this]
    {
        autoTrimCapturedEnabled.store(autoTrimCapturedToggle.getToggleState());
    };
    addAndMakeVisible(autoTrimCapturedToggle);

    lanesContainer = std::make_unique<juce::Component>();
    lanesViewport.setViewedComponent(lanesContainer.get(), false);
    lanesViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(lanesViewport);

    AddLaneButton.onClick = [this]
    {
        setTrackCount(currentTrackCount + 1);
    };
    addAndMakeVisible(AddLaneButton);

    // for (int laneIndex = 0; laneIndex < maxTracks-2; ++laneIndex)
    // {
    //     laneUi[laneIndex] = std::make_unique<LaneComponent>(*this, laneIndex);
    //     lanesContainer->addAndMakeVisible(laneUi[laneIndex].get());
    // }
    ensureLaneUiExists(0);

    startTimerHz(15);
}

LooperTabComponent::~LooperTabComponent() = default;

void LooperTabComponent::prepareToPlay(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    masterTimelinePosition.store(0);
}

void LooperTabComponent::releaseResources()
{
    transportMode.store(static_cast<int>(TransportMode::Stopped));
    recordingTrackIndex.store(-1);
    armedTrackIndex.store(-1);
}

std::unique_ptr<juce::XmlElement> LooperTabComponent::createStateXml() const
{
    auto stateXml = std::make_unique<juce::XmlElement>("LOOPER_STATE");
    stateXml->setAttribute("trackCount", currentTrackCount);
    stateXml->setAttribute("autoTrimCapturedEnabled", autoTrimCapturedEnabled.load());

    for (int laneIndex = 0; laneIndex < currentTrackCount; ++laneIndex)
    {
        auto trackXml = std::make_unique<juce::XmlElement>("TRACK");
        trackXml->setAttribute("index", laneIndex);
        trackXml->setAttribute("level", tracks[laneIndex].level.load());
        stateXml->addChildElement(trackXml.release());
    }

    return stateXml;
}

void LooperTabComponent::restoreStateFromXml(const juce::XmlElement& stateXml)
{
    auto desiredTrackCount = juce::jlimit(0, maxTracks, stateXml.getIntAttribute("trackCount", currentTrackCount));
    setTrackCount(desiredTrackCount);

    auto autoTrimState = stateXml.getBoolAttribute("autoTrimCapturedEnabled", true);
    autoTrimCapturedEnabled.store(autoTrimState);
    autoTrimCapturedToggle.setToggleState(autoTrimState, juce::dontSendNotification);

    for (auto* trackXml = stateXml.getFirstChildElement(); trackXml != nullptr; trackXml = trackXml->getNextElement())
    {
        if (!trackXml->hasTagName("TRACK"))
            continue;

        const auto trackIndex = trackXml->getIntAttribute("index", -1);
        if (trackIndex < 0 || trackIndex >= currentTrackCount)
            continue;

        setTrackLevel(trackIndex, trackXml->getDoubleAttribute("level", 1.0));
    }

    refreshLaneUi();
}

void LooperTabComponent::setTrackCount(int trackCount)
{
    const auto clampedCount = juce::jlimit(0, maxTracks, trackCount);

    for (int laneIndex = currentTrackCount; laneIndex < clampedCount; ++laneIndex)
        ensureLaneUiExists(laneIndex);

    currentTrackCount = clampedCount;

    for (int laneIndex = 0; laneIndex < maxTracks; ++laneIndex)
        if (laneUi[laneIndex] != nullptr)
            laneUi[laneIndex]->setVisible(laneIndex < currentTrackCount);

    resized();
    refreshLaneUi();
}

void LooperTabComponent::setTrackLevel(int trackIndex, float level)
{
    if (trackIndex < 0 || trackIndex >= currentTrackCount)
        return;

    tracks[trackIndex].level.store(juce::jlimit(0.0f, 2.0f, level));

    if (laneUi[trackIndex] != nullptr)
        laneUi[trackIndex]->refresh();
}

float LooperTabComponent::getTrackLevel(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= currentTrackCount)
        return 1.0f;

    return tracks[trackIndex].level.load();
}

void LooperTabComponent::ensureLaneUiExists(int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= maxTracks)
        return;

    if (laneUi[laneIndex] == nullptr)
        laneUi[laneIndex] = std::make_unique<LaneComponent>(*this, laneIndex);

    lanesContainer->addAndMakeVisible(laneUi[laneIndex].get());
}

bool LooperTabComponent::handleKeyPress(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        pendingSpaceCommands.fetch_add(1);
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        pendingStopCommands.fetch_add(1);
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        pendingClearCommands.fetch_add(1);
        return true;
    }

    return false;
}

void LooperTabComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.fillAll(juce::Colour(0xff1c1f26));
    g.setColour(juce::Colour(0xff2d3440));
    g.fillRoundedRectangle(area.reduced(8.0f), 16.0f);
}

void LooperTabComponent::resized()
{
    auto area = getLocalBounds().reduced(layoutMargin);

    auto headerArea = area.removeFromTop(headerHeight);
    headerLabel.setBounds(headerArea.removeFromTop(30));
    headerArea.removeFromTop(6);
    transportLabel.setBounds(headerArea.removeFromTop(30));
    headerArea.removeFromTop(10);

    auto buttonRow = headerArea.removeFromTop(28);
    recordArmButton.setBounds(buttonRow.removeFromLeft(160));
    buttonRow.removeFromLeft(8);
    stopButton.setBounds(buttonRow.removeFromLeft(110));
    buttonRow.removeFromLeft(8);
    clearButton.setBounds(buttonRow.removeFromLeft(130));
    buttonRow.removeFromLeft(8);
    autoTrimCapturedToggle.setBounds(buttonRow.removeFromLeft(180));
    buttonRow.removeFromLeft(8);
    AddLaneButton.setBounds(buttonRow.removeFromLeft(120));


    area.removeFromTop(8);
    lanesViewport.setBounds(area);

    if (lanesContainer == nullptr)
        return;

    auto lanesArea = lanesContainer->getLocalBounds().reduced(layoutMargin);
    for (int laneIndex = 0; laneIndex < currentTrackCount; ++laneIndex)
    {
        laneUi[laneIndex]->setBounds(lanesArea.removeFromTop(laneHeight));
        if (laneIndex + 1 < maxTracks)
            lanesArea.removeFromTop(laneSpacing);
    }

    const int containerHeight = layoutMargin * 2 + currentTrackCount * laneHeight + (currentTrackCount - 1) * laneSpacing;
    lanesContainer->setSize(juce::jmax(1, lanesViewport.getWidth() - 2), juce::jmax(containerHeight, lanesViewport.getHeight()));
}

void LooperTabComponent::timerCallback()
{
    refreshLaneUi();
    transportLabel.setText(getTransportStatusText(), juce::dontSendNotification);
}

void LooperTabComponent::handlePendingCommands()
{
    while (pendingClearCommands.exchange(0) > 0)
        handleClearCommand();

    while (pendingStopCommands.exchange(0) > 0)
        handleStopCommand();

    while (pendingSpaceCommands.exchange(0) > 0)
        handleSpaceCommand();

    for (int laneIndex = 0; laneIndex < currentTrackCount; ++laneIndex)
    {
        while (pendingLaneClearCommands[laneIndex].exchange(0) > 0)
            clearTrack(laneIndex);
    }
}

void LooperTabComponent::handleSpaceCommand()
{
    const auto mode = static_cast<TransportMode>(transportMode.load());
    const int activeRecordTrack = recordingTrackIndex.load();

    if (mode == TransportMode::Stopped)
    {
        if (getTrackCountReady() == 0)
            startFirstRecording();
        else
            transportMode.store(static_cast<int>(TransportMode::Playing));

        masterTimelinePosition.store(0);
        return;
    }

    if (activeRecordTrack >= 0)
    {
        if (mode == TransportMode::FirstRecording)
        {
            finalizeRecordingTrack(activeRecordTrack);
            transportMode.store(static_cast<int>(TransportMode::Playing));
        }

        return;
    }

    if (mode == TransportMode::Playing && armedTrackIndex.load() < 0)
        armNextRecording();
}

void LooperTabComponent::handleStopCommand()
{
    stopTransport();
}

void LooperTabComponent::handleClearCommand()
{
    clearAllTracks();
}

void LooperTabComponent::startFirstRecording()
{
    clearTrack(0);
    transportMode.store(static_cast<int>(TransportMode::FirstRecording));
    masterTrackIndex.store(0);
    masterTimelinePosition.store(0);
    beginRecordingTrack(0, 0, true);
}

void LooperTabComponent::armNextRecording()
{
    if (masterLoopLengthSamples.load() <= 0)
        return;

    const int nextTrackIndex = getNextFreeTrackIndex();

    if (nextTrackIndex < 0)
        return;

    armedTrackIndex.store(nextTrackIndex);
}

void LooperTabComponent::beginRecordingTrack(int trackIndex, int targetLengthSamples, bool firstRecording)
{
    if (trackIndex < 0 || trackIndex >= maxTracks)
        return;

    auto& track = tracks[trackIndex];
    track.recordPosition = 0;
    track.playbackPosition = 0;
    track.recording.store(true);
    track.ready.store(false);
    track.muted.store(false);

    const int initialSamples = targetLengthSamples > 0
        ? targetLengthSamples
        : juce::jmax(currentBlockSize * 4, 2048);

    ensureRecordingCapacity(track, initialSamples, currentChannelCount, targetLengthSamples > 0);

    recordingTrackIndex.store(trackIndex);

    if (!firstRecording)
        armedTrackIndex.store(-1);
}

void LooperTabComponent::finalizeRecordingTrack(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= maxTracks)
        return;

    auto& track = tracks[trackIndex];
    int capturedSamples = juce::jmax(0, track.recordPosition);

    track.recording.store(false);
    track.ready.store(capturedSamples > 0);
    track.recordedSamples.store(capturedSamples);
    track.playbackPosition = 0;

    if (autoTrimCapturedEnabled.load())
        autoTrimTrack(trackIndex);

    if (trackIndex == masterTrackIndex.load())
    {
        masterLoopLengthSamples.store(getPlaybackLengthSamples(track));
        masterTimelinePosition.store(0);
    }

    recordingTrackIndex.store(-1);
    armedTrackIndex.store(-1);

    if (capturedSamples > 0 && masterTrackIndex.load() < 0)
    {
        masterTrackIndex.store(trackIndex);
        masterLoopLengthSamples.store(getPlaybackLengthSamples(track));
        masterTimelinePosition.store(0);
    }
}

void LooperTabComponent::stopTransport()
{
    const int activeRecordTrack = recordingTrackIndex.load();

    if (activeRecordTrack >= 0)
        finalizeRecordingTrack(activeRecordTrack);

    armedTrackIndex.store(-1);
    transportMode.store(static_cast<int>(TransportMode::Stopped));
    masterTimelinePosition.store(0);
}

void LooperTabComponent::clearAllTracks()
{
    for (int laneIndex = 0; laneIndex < currentTrackCount; ++laneIndex)
        clearTrack(laneIndex);

    transportMode.store(static_cast<int>(TransportMode::Stopped));
    masterLoopLengthSamples.store(0);
    masterTimelinePosition.store(0);
    masterTrackIndex.store(-1);
    recordingTrackIndex.store(-1);
    armedTrackIndex.store(-1);
}

void LooperTabComponent::clearTrack(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= maxTracks)
        return;

    const juce::ScopedLock lock(trackBufferLock);
    auto& track = tracks[trackIndex];
    const bool wasMaster = (masterTrackIndex.load() == trackIndex);

    track.buffer.setSize(0, 0);
    track.recordedSamples.store(0);
    track.trimStartPercent.store(0.0f);
    track.trimEndPercent.store(0.0f);
    track.ready.store(false);
    track.recording.store(false);
    track.muted.store(false);
    track.recordPosition = 0;
    track.playbackPosition = 0;

    if (recordingTrackIndex.load() == trackIndex)
        recordingTrackIndex.store(-1);

    if (armedTrackIndex.load() == trackIndex)
        armedTrackIndex.store(-1);

    if (wasMaster)
        recalculateMasterLoop();

    if (getTrackCountReady() == 0)
        transportMode.store(static_cast<int>(TransportMode::Stopped));
}

void LooperTabComponent::recalculateMasterLoop()
{
    masterTrackIndex.store(-1);
    masterLoopLengthSamples.store(0);
    masterTimelinePosition.store(0);

    for (int laneIndex = 0; laneIndex < currentTrackCount; ++laneIndex)
    {
        if (!tracks[laneIndex].ready.load())
            continue;

        masterTrackIndex.store(laneIndex);
        masterLoopLengthSamples.store(getPlaybackLengthSamples(tracks[laneIndex]));
        break;
    }
}

void LooperTabComponent::autoTrimTrack(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= currentTrackCount)
        return;

    const juce::ScopedLock lock(trackBufferLock);
    auto& track = tracks[trackIndex];
    const int totalSamples = track.recordedSamples.load();

    if (!track.ready.load() || totalSamples <= 0 || track.buffer.getNumSamples() <= 0)
        return;

    int firstAudioSample = -1;
    int lastAudioSample = -1;
    const int channelCount = track.buffer.getNumChannels();

    for (int sampleIndex = 0; sampleIndex < totalSamples; ++sampleIndex)
    {
        float peak = 0.0f;

        for (int channel = 0; channel < channelCount; ++channel)
            peak = juce::jmax(peak, std::abs(track.buffer.getSample(channel, sampleIndex)));

        if (peak > trimThreshold)
        {
            if (firstAudioSample < 0)
                firstAudioSample = sampleIndex;

            lastAudioSample = sampleIndex;
        }
    }

    if (firstAudioSample < 0 || lastAudioSample < 0)
    {
        track.trimStartPercent.store(0.0f);
        track.trimEndPercent.store(0.0f);
        return;
    }

    const float startPercent = 100.0f * static_cast<float>(firstAudioSample) / static_cast<float>(totalSamples);
    const float endPercent = 100.0f * static_cast<float>(juce::jmax(0, totalSamples - 1 - lastAudioSample)) / static_cast<float>(totalSamples);

    setTrackTrimPercent(trackIndex, true, startPercent);
    setTrackTrimPercent(trackIndex, false, endPercent);
}

void LooperTabComponent::setTrackTrimPercent(int trackIndex, bool startTrim, float percent)
{
    if (trackIndex < 0 || trackIndex >= currentTrackCount)
        return;

    auto& track = tracks[trackIndex];
    auto clampPercent = [](float value)
    {
        return juce::jlimit(0.0f, 100.0f, value);
    };

    percent = clampPercent(percent);

    const float otherPercent = startTrim
        ? track.trimEndPercent.load()
        : track.trimStartPercent.load();

    const float maxAllowed = juce::jmax(0.0f, 99.0f - otherPercent);
    percent = juce::jlimit(0.0f, maxAllowed, percent);

    if (startTrim)
        track.trimStartPercent.store(percent);
    else
        track.trimEndPercent.store(percent);

    if (trackIndex == masterTrackIndex.load() && track.ready.load())
    {
        const int playbackLength = getPlaybackLengthSamples(track);
        masterLoopLengthSamples.store(playbackLength);
        masterTimelinePosition.store(masterTimelinePosition.load() % juce::jmax(1, playbackLength));
    }
}

float LooperTabComponent::getTrackTrimPercent(int trackIndex, bool startTrim) const
{
    if (trackIndex < 0 || trackIndex >= currentTrackCount)
        return 0.0f;

    return startTrim ? tracks[trackIndex].trimStartPercent.load()
                     : tracks[trackIndex].trimEndPercent.load();
}

juce::String LooperTabComponent::getTrackStatusText(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= currentTrackCount)
        return {};

    const auto& track = tracks[trackIndex];

    if (track.recording.load())
        return trackIndex == 0 && transportMode.load() == static_cast<int>(TransportMode::FirstRecording)
            ? "Recording first loop"
            : "Recording new lane";

    if (!track.ready.load())
        return "Empty";

    if (track.muted.load())
        return "Ready (muted)";

    const int recordedSamples = juce::jmax(0, track.recordedSamples.load());
    const int playbackLength = getPlaybackLengthSamples(track);
    const double recordedSeconds = recordedSamples / currentSampleRate;
    const double playbackSeconds = playbackLength / currentSampleRate;

    return "Ready - " + juce::String(recordedSeconds, 2) + " s total, " + juce::String(playbackSeconds, 2) + " s playback";
}

juce::String LooperTabComponent::getTransportStatusText() const
{
    const auto mode = static_cast<TransportMode>(transportMode.load());
    const int readyTracks = getTrackCountReady();
    const int loopSamples = masterLoopLengthSamples.load();
    const double loopSeconds = loopSamples > 0 ? static_cast<double>(loopSamples) / currentSampleRate : 0.0;

    juce::String status = "Mode: ";

    switch (mode)
    {
        case TransportMode::Stopped: status += "stopped"; break;
        case TransportMode::Playing: status += "playing"; break;
        case TransportMode::FirstRecording: status += "recording first loop"; break;
    }

    status += " | Ready lanes: " + juce::String(readyTracks);

    if (loopSamples > 0)
        status += " | Master loop: " + juce::String(loopSeconds, 2) + " s";

    if (armedTrackIndex.load() >= 0)
        status += " | armed lane " + juce::String(armedTrackIndex.load() + 1);

    if (recordingTrackIndex.load() >= 0)
        status += " | recording lane " + juce::String(recordingTrackIndex.load() + 1);

    return status + " | Space: record/arm, Esc: stop, Del: clear";
}

int LooperTabComponent::getTrimStartSamples(const TrackState& track) const
{
    const int recordedSamples = track.recordedSamples.load();
    return juce::jlimit(0, recordedSamples, static_cast<int>(std::round(track.trimStartPercent.load() * recordedSamples / 100.0f)));
}

int LooperTabComponent::getTrimEndSamples(const TrackState& track) const
{
    const int recordedSamples = track.recordedSamples.load();
    return juce::jlimit(0, recordedSamples, static_cast<int>(std::round(track.trimEndPercent.load() * recordedSamples / 100.0f)));
}

int LooperTabComponent::getPlaybackLengthSamples(const TrackState& track) const
{
    const int recordedSamples = track.recordedSamples.load();
    const int playbackLength = recordedSamples - getTrimStartSamples(track) - getTrimEndSamples(track);

    return track.ready.load() ? juce::jmax(1, playbackLength) : 0;
}

bool LooperTabComponent::isTransportPlaying() const
{
    return transportMode.load() == static_cast<int>(TransportMode::Playing);
}

int LooperTabComponent::getNextFreeTrackIndex() const
{
    for (int laneIndex = 0; laneIndex < currentTrackCount; ++laneIndex)
        if (!tracks[laneIndex].ready.load() && !tracks[laneIndex].recording.load())
            return laneIndex;

    return -1;
}

void LooperTabComponent::ensureRecordingCapacity(TrackState& track, int requiredSamples, int channelCount, bool exactSize)
{
    const juce::ScopedLock lock(trackBufferLock);
    const int targetSamples = exactSize ? requiredSamples : juce::jmax(requiredSamples, track.buffer.getNumSamples());
    if (targetSamples <= track.buffer.getNumSamples() && track.buffer.getNumChannels() == channelCount)
        return;

    const int newSamples = juce::jmax(targetSamples, track.buffer.getNumSamples());
    track.buffer.setSize(channelCount, newSamples, true, false, true);
}

void LooperTabComponent::recordChunk(TrackState& track, juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const juce::ScopedLock lock(trackBufferLock);
    const int channelCount = juce::jmin(buffer.getNumChannels(), track.buffer.getNumChannels());
    const int writableSamples = juce::jmin(numSamples, juce::jmax(0, track.buffer.getNumSamples() - track.recordPosition));

    if (writableSamples <= 0 || channelCount <= 0)
        return;

    for (int channel = 0; channel < channelCount; ++channel)
        juce::FloatVectorOperations::copy(track.buffer.getWritePointer(channel, track.recordPosition),
                                          buffer.getReadPointer(channel, startSample),
                                          writableSamples);

    track.recordPosition += writableSamples;

    if (track.recordPosition > track.recordedSamples.load())
        track.recordedSamples.store(track.recordPosition);
}

void LooperTabComponent::mixTrackPlayback(TrackState& track, juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const juce::ScopedLock lock(trackBufferLock);
    if (!track.ready.load() || track.muted.load())
        return;

    const int playbackLength = getPlaybackLengthSamples(track);
    if (playbackLength <= 0)
        return;

    const int startOffset = getTrimStartSamples(track);
    const int channelCount = juce::jmin(buffer.getNumChannels(), track.buffer.getNumChannels());
    const int loopLength = juce::jmax(1, masterLoopLengthSamples.load());
    const int transportStart = masterTimelinePosition.load();
    const float trackLevel = track.level.load();

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        const int transportPosition = (transportStart + sampleIndex) % loopLength;
        const int sourceIndex = startOffset + (transportPosition % playbackLength);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            const float inputSample = track.buffer.getSample(channel, sourceIndex) * trackLevel;
            buffer.addSample(channel, startSample + sampleIndex, inputSample);
        }
    }
}

void LooperTabComponent::processSegment(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int activeRecordTrack = recordingTrackIndex.load();
    const auto mode = static_cast<TransportMode>(transportMode.load());

    if (activeRecordTrack >= 0)
    {
        auto& track = tracks[activeRecordTrack];
        ensureRecordingCapacity(track, track.recordPosition + numSamples, currentChannelCount, false);
        recordChunk(track, buffer, startSample, numSamples);

        if (mode == TransportMode::Playing && masterLoopLengthSamples.load() > 0 && track.recordPosition >= masterLoopLengthSamples.load())
        {
            finalizeRecordingTrack(activeRecordTrack);
        }
        else if (track.recording.load() && activeRecordTrack == 0
                 && mode == TransportMode::FirstRecording)
        {
            bool needsCapacity = false;
            {
                const juce::ScopedLock lock(trackBufferLock);
                needsCapacity = track.recordPosition >= track.buffer.getNumSamples();
            }

            if (needsCapacity)
                ensureRecordingCapacity(track, track.recordPosition + currentBlockSize, currentChannelCount, false);
        }
    }

    if (armedTrackIndex.load() >= 0
        && recordingTrackIndex.load() < 0
        && transportMode.load() == static_cast<int>(TransportMode::Playing)
        && masterLoopLengthSamples.load() > 0
        && masterTimelinePosition.load() == 0)
    {
        beginRecordingTrack(armedTrackIndex.load(), masterLoopLengthSamples.load(), false);
        auto& track = tracks[recordingTrackIndex.load()];
        ensureRecordingCapacity(track, masterLoopLengthSamples.load(), currentChannelCount, true);
        recordChunk(track, buffer, startSample, numSamples);

        if (track.recording.load() && track.recordPosition >= masterLoopLengthSamples.load())
            finalizeRecordingTrack(recordingTrackIndex.load());
    }

    if (mode != TransportMode::Playing)
        return;

    for (int laneIndex = 0; laneIndex < maxTracks; ++laneIndex)
    {
        if (laneIndex == recordingTrackIndex.load())
            continue;

        mixTrackPlayback(tracks[laneIndex], buffer, startSample, numSamples);
    }
}

void LooperTabComponent::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (buffer.getNumChannels() <= 0 || numSamples <= 0)
        return;

    currentChannelCount = buffer.getNumChannels();
    handlePendingCommands();

    if (transportMode.load() == static_cast<int>(TransportMode::Stopped))
        return;

    int remaining = numSamples;
    int offset = startSample;

    while (remaining > 0)
    {
        int segmentSamples = remaining;
        const int loopLength = juce::jmax(1, masterLoopLengthSamples.load());

        if (transportMode.load() == static_cast<int>(TransportMode::Playing)
            && loopLength > 0)
        {
            int samplesUntilWrap = loopLength - masterTimelinePosition.load();

            if (samplesUntilWrap <= 0)
                samplesUntilWrap = loopLength;

            segmentSamples = juce::jmin(segmentSamples, samplesUntilWrap);
        }

        processSegment(buffer, offset, segmentSamples);

        if (transportMode.load() == static_cast<int>(TransportMode::Playing)
            && loopLength > 0)
        {
            const int nextPosition = masterTimelinePosition.load() + segmentSamples;
            masterTimelinePosition.store(nextPosition % loopLength);
        }

        remaining -= segmentSamples;
        offset += segmentSamples;
    }
}

void LooperTabComponent::refreshLaneUi()
{
    for (int laneIndex = 0; laneIndex < currentTrackCount; ++laneIndex)
        laneUi[laneIndex]->refresh();
}

int LooperTabComponent::getTrackCountReady() const
{
    int readyCount = 0;

    for (const auto& track : tracks)
        if (track.ready.load())
            ++readyCount;

    return readyCount;
}