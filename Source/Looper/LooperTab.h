#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>

class LooperTabComponent : public juce::Component, private juce::Timer
{
public:
    static constexpr int maxTracks = 10;

    enum class TransportMode
    {
        Stopped = 0,
        Playing = 1,
        FirstRecording = 2
    };

    struct TrackState
    {
        juce::AudioBuffer<float> buffer;
        std::atomic<int> recordedSamples { 0 };
        std::atomic<float> trimStartPercent { 0.0f };
        std::atomic<float> trimEndPercent { 0.0f };
        std::atomic<float> level { 1.0f };
        std::atomic<bool> ready { false };
        std::atomic<bool> recording { false };
        std::atomic<bool> muted { false };

        int recordPosition = 0;
        int playbackPosition = 0;
    };

    LooperTabComponent();
    ~LooperTabComponent() override;

    void prepareToPlay(double sampleRate, int blockSize);
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void releaseResources();

    std::unique_ptr<juce::XmlElement> createStateXml() const;
    void restoreStateFromXml(const juce::XmlElement& stateXml);
    bool saveLoopSet(const juce::File& directory);
    bool loadLoopSet(const juce::File& directory);
    juce::File getAutosaveDirectory() const;
    void setTrackCount(int trackCount);
    int getTrackCount() const { return currentTrackCount; }
    void setTrackLevel(int trackIndex, float level);
    float getTrackLevel(int trackIndex) const;

    bool handleKeyPress(const juce::KeyPress& key);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class LaneComponent;

    friend class LaneComponent;

    void timerCallback() override;
    void autosave();
    void chooseLoopDirectory();
    void saveNamedLoopSet();
    void showLoadLoopSetDialog();
    void clearAutosave();
    void updateLoopDirectoryLabel();
    bool writeTrackAudio(const juce::File& file, const juce::AudioBuffer<float>& buffer, int numSamples) const;
    bool readTrackAudio(const juce::File& file, juce::AudioBuffer<float>& buffer) const;

    void handlePendingCommands();
    void handleSpaceCommand();
    void handleStopCommand();
    void handleClearCommand();
    void ensureLaneUiExists(int laneIndex);
    void startFirstRecording();
    void armNextRecording();
    void beginRecordingTrack(int trackIndex, int targetLengthSamples, bool firstRecording);
    void finalizeRecordingTrack(int trackIndex);
    void stopTransport();
    void clearAllTracks();
    void clearTrack(int trackIndex);
    void recalculateMasterLoop();
    void autoTrimTrack(int trackIndex);
    void setTrackTrimPercent(int trackIndex, bool startTrim, float percent);
    float getTrackTrimPercent(int trackIndex, bool startTrim) const;
    juce::String getTrackStatusText(int trackIndex) const;
    juce::String getTransportStatusText() const;
    int getTrimStartSamples(const TrackState& track) const;
    int getTrimEndSamples(const TrackState& track) const;
    int getPlaybackLengthSamples(const TrackState& track) const;
    bool isTransportPlaying() const;
    int getNextFreeTrackIndex() const;
    void ensureRecordingCapacity(TrackState& track, int requiredSamples, int channelCount, bool exactSize);
    void recordChunk(TrackState& track, juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void mixTrackPlayback(TrackState& track, juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void mixSeamPreview(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void processSegment(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void refreshLaneUi();
    int getTrackCountReady() const;

    std::array<TrackState, maxTracks> tracks;
    std::array<std::unique_ptr<LaneComponent>, maxTracks> laneUi;
    juce::CriticalSection trackBufferLock;

    juce::Label headerLabel;
    juce::Label transportLabel;
    juce::TextButton recordArmButton { "Space: Record / Arm" };
    juce::TextButton stopButton { "Stop (Esc)" };
    juce::TextButton clearButton { "Clear All (Del)" };
    juce::ToggleButton autoTrimCapturedToggle { "Auto-trim new lanes" };
    juce::ToggleButton seamAuditionToggle { "Audition Seam" };
    juce::Label seamPreviewLabel;
    juce::ComboBox seamPreviewLengthBox;
    juce::TextButton saveLoopButton { "Save Loop Set" };
    juce::TextButton loadLoopButton { "Load Loop Set" };
    juce::TextButton loadAutosaveButton { "Load Autosave" };
    juce::TextButton chooseLoopDirectoryButton { "Set Save Folder" };
    juce::Label loopDirectoryLabel;
    std::unique_ptr<juce::FileChooser> loopFileChooser;
    std::unique_ptr<juce::AlertWindow> loopLoadWindow;
    juce::Viewport lanesViewport;
    std::unique_ptr<juce::Component> lanesContainer;

    juce::TextButton AddLaneButton { "Add Lane" };

    std::atomic<int> pendingSpaceCommands { 0 };
    std::atomic<int> pendingStopCommands { 0 };
    std::atomic<int> pendingClearCommands { 0 };
    std::array<std::atomic<int>, maxTracks> pendingLaneClearCommands {};
    std::atomic<bool> autoTrimCapturedEnabled { true };
    std::atomic<bool> seamAuditionEnabled { false };
    std::atomic<int> seamPreviewLengthMs { 1000 };
    std::atomic<int> seamPreviewPosition { 0 };

    std::atomic<int> transportMode { static_cast<int>(TransportMode::Stopped) };
    std::atomic<int> masterLoopLengthSamples { 0 };
    std::atomic<int> masterTimelinePosition { 0 };
    std::atomic<int> recordingTrackIndex { -1 };
    std::atomic<int> armedTrackIndex { -1 };
    std::atomic<int> masterTrackIndex { -1 };

    double currentSampleRate = 44100.0;
    int currentBlockSize = 128;
    int currentChannelCount = 2;
    int currentTrackCount = 0;
    int autosaveCounter = 0;
    bool autosaveHasBeenInitialized = false;
    std::atomic<bool> autosaveClearRequested { false };
    std::atomic<bool> autosaveSaveRequested { false };
    juce::File defaultLoopDirectory;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTabComponent)
};